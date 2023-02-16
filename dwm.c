/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <time.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define OPAQUE                  0xffU

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { CurNormal, CurResize, CurMove, CurSwal, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayVisual,
	   NetWMName, NetWMIcon, NetWMState, NetWMFullscreen, NetActiveWindow, NetWMWindowType, NetWMWindowTypeDock,
	   NetSystemTrayOrientationHorz, NetWMWindowTypeDialog, NetClientList, NetWMCheck, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum { ClientRegular = 1, ClientSwallowee, ClientSwallower }; /* client types */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float cfact;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isfakefullscreen;
	unsigned int icw, ich; Picture icon;
	int issteam;
	int floatborderpx;
	Client *next;
	Client *snext;
	Client *swallowedby;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
    KeySym chain;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	unsigned int signum;
	void (*func)(const Arg *);
	const Arg arg;
} Signal;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;
struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int altTabN;		  /* move that many clients forward */
	int nTabs;			  /* number of active clients in tag */
	int isAlt; 			  /* 1,0 */
	int maxWTab;
	int maxHTab;
	int gappih;           /* horizontal gap between windows */
	int gappiv;           /* vertical gap between windows */
	int gappoh;           /* horizontal outer gaps */
	int gappov;           /* vertical outer gaps */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Client ** altsnext; /* array of all clients in the tag */
	Monitor *next;
	Window barwin;
	Window tabwin;
	const Layout *lt[2];
	Pertag *pertag;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
	int floatx, floaty, floatw, floath;
	int floatborderpx;
	int isfakefullscreen;
} Rule;

typedef struct Systray Systray;
struct Systray {
	Window win;
	Client *icons;
};

typedef struct {
	const char *cmd;
	int id;
} StatusCmd;

typedef struct {
	int monitor;
	int tag;
	int layout;
	float mfact;
	int nmaster;
	int showbar;
	int topbar;
} MonitorRule;

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

typedef struct Swallow Swallow;
struct Swallow {
	/* Window class name, instance name (WM_CLASS) and title
	 * (WM_NAME/_NET_WM_NAME, latter preferred if it exists). An empty string
	 * implies a wildcard as per strstr(). */
	char class[256];
	char inst[256];
	char title[256];

	/* Used to delete swallow instance after 'swaldecay' windows were mapped
	 * without the swallow having been consumed. 'decay' keeps track of the
	 * remaining "charges". */
	int decay;

	/* The swallower, i.e. the client which will swallow the next mapped window
	 * whose filters match the above properties. */
	Client *client;

	/* Linked list of registered swallow instances. */
	Swallow *next;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayout(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatusbar(Monitor *m, int bh, char* text);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static int fakesignal(void);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static Picture geticonprop(Window w, unsigned int *icw, unsigned int *ich);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static unsigned int getsystraywidth();
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static int fake_signal(void);
static void killclient(const Arg *arg);
static void layoutmenu(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static unsigned int nexttag(void);
static Client *nexttiled(Client *c);
static void pop(Client *);
static Client *prevtiled(Client *c);
static unsigned int prevtag(void);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void removesystrayicon(Client *i);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void runautostart(void);
static void scan(void);
static void scratchpad_hide ();
static _Bool scratchpad_last_showed_is_killed (void);
static void scratchpad_remove ();
static void scratchpad_show ();
static void scratchpad_show_client (Client * c);
static void scratchpad_show_first (void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void swal(Client *swer, Client *swee, int manage);
static void swalreg(Client *c, const char* class, const char* inst, const char* title);
static void swaldecayby(int decayby);
static void swalmanage(Swallow *s, Window w, XWindowAttributes *wa);
static Swallow *swalmatch(Window w);
static void swalmouse(const Arg *arg);
static void swalrm(Swallow *s);
static void swalunreg(Client *c);
static void swalstop(Client *c, Client *root);
static void swalstopsel(const Arg *unused);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tagtonext(const Arg *arg);
static void tagtoprev(const Arg *arg);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void freeicon(Client *c);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(int updatebar);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updateicon(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static void viewnext(const Arg *arg);
static void viewprev(const Arg *arg);
static Client *wintoclient(Window w);
static int wintoclient2(Window w, Client **pc, Client **proot);
static Monitor *wintomon(Window w);
static void winview(const Arg* arg);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);

void drawTab(int nwins, int first, Monitor *m);
void altTabStart(const Arg *arg);
static void altTabEnd();

static void load_xresources(void);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);

static void keyrelease(XEvent *e);
static void combotag(const Arg *arg);
static void comboview(const Arg *arg);


/* variables */
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[] = "autostart.sh";
static const char broken[] = "broken";
static const char dwmdir[] = "dwm";
static const char localshare[] = ".local/share";
static char stext[1024];
static int statusw;
static int statuscmdn;
static char lastbutton[] = "-";
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int vp;               /* vertical padding for bar */
static int sp;               /* side padding for bar */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = keyrelease,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyRelease] = keyrelease,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int restart = 0;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Clr **tagscheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Swallow *swallows;
static Window root, wmcheckwin;
static KeySym keychain = -1;

static Systray *systray = NULL;
static unsigned long systrayorientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

/* scratchpad */
# define SCRATCHPAD_MASK (1u << sizeof tags / sizeof * tags)
static Client * scratchpad_last_showed = NULL;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 30 ? -1 : 1]; };

/* function implementations */
static int combo = 0;

void
keyrelease(XEvent *e) {
	combo = 0;
}

void
combotag(const Arg *arg) {
	if(selmon->sel && arg->ui & TAGMASK) {
		if (combo) {
			selmon->sel->tags |= arg->ui & TAGMASK;
		} else {
			combo = 1;
			selmon->sel->tags = arg->ui & TAGMASK;
		}
		focus(NULL);
		arrange(selmon);
	}
}

void
comboview(const Arg *arg) {
	unsigned newtags = arg->ui & TAGMASK;
	if (combo) {
		selmon->tagset[selmon->seltags] |= newtags;
	} else {
		selmon->seltags ^= 1;	/*toggle tagset*/
		combo = 1;
		if (newtags)
			selmon->tagset[selmon->seltags] = newtags;
	}
	focus(NULL);
	arrange(selmon);
}

void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	if (strstr(class, "Steam") || strstr(class, "steam_app_"))
		c->issteam = 1;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->isfakefullscreen = r->isfakefullscreen;
			c->tags |= r->tags;
			c->floatborderpx = r->floatborderpx;
			if (r->isfloating) {
				c->x = r->floatx;
				c->y = r->floaty;
				c->w = r->floatw;
				c->h = r->floath;
			}
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (c->tags != SCRATCHPAD_MASK)
		c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - statusw - getsystraywidth()) {
			char *text, *s, ch;
			*lastbutton = '0' + ev->button;

			x = selmon->ww - statusw;
			click = ClkStatusText;

			statuscmdn = 0;
			for (text = s = stext; *s && x <= ev->x; s++) {
				if ((unsigned char)(*s) < ' ') {
					ch = *s;
					*s = '\0';
					x += TEXTW(text) - lrpad;
					*s = ch;
					text = s + 1;
					if (x >= ev->x)
						break;
					statuscmdn = ch;
				}
			}
		}
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	altTabEnd();
	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	if (showsystray) {
		while (systray->icons)
			removesystrayicon(systray->icons);
		XUnmapWindow(dpy, systray->win);
		XDestroyWindow(dpy, systray->win);
		free(systray);
	}
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client))))
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}

			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			XGetWindowAttributes(dpy, c->win, &wa);
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* use parents background color */
			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			setclientstate(c, NormalState);
			updatesystray(1);
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				      || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */
                                      && (!c->isfullscreen || c->isfakefullscreen))));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen
                                        &&  !c->isfakefullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx + sp, m->by + vp, m->ww -  2 * sp, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	switch (wintoclient2(ev->window, &c, NULL)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
		} else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (!c->issteam) {
				if (ev->value_mask & CWX) {
					c->oldx = c->x;
					c->x = m->mx + ev->x;
				}
				if (ev->value_mask & CWY) {
					c->oldy = c->y;
					c->y = m->my + ev->y;
				}
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
		break;
	case ClientSwallower:
		/* Reject any move/resize requests for swallowers and communicate
		 * refusal to client via a synthetic ConfigureNotify (ICCCM 4.1.5). */
		configure(c);
		break;
	default:
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
		break;
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m, *mon;
	unsigned int i;
	int mi, j, layout;
	const MonitorRule *mr;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	/*
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	m->nTabs = 0;
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	*/

	for (mi = 0, mon = mons; mon; mon = mon->next, mi++);
	for (j = 0; j < LENGTH(monrules); j++) {
		mr = &monrules[j];
		if ((mr->monitor == -1 || mr->monitor == mi)
				&& (mr->tag <= 0 || (m->tagset[0] & (1 << (mr->tag - 1))))
		) {
			layout = MAX(mr->layout, 0);
			layout = MIN(layout, LENGTH(layouts) - 1);
			m->lt[0] = &layouts[layout];
			m->lt[1] = &layouts[1 % LENGTH(layouts)];
			strncpy(m->ltsymbol, layouts[layout].symbol, sizeof m->ltsymbol);

			if (mr->mfact > -1)
				m->mfact = mr->mfact;
			if (mr->nmaster > -1)
				m->nmaster = mr->nmaster;
			if (mr->showbar > -1)
				m->showbar = mr->showbar;
			if (mr->topbar > -1)
				m->topbar = mr->topbar;
			break;
		}
	}

	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		/*
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		*/
		m->pertag->sellts[i] = m->sellt;

		m->pertag->showbars[i] = m->showbar;

		for (j = 0; j < LENGTH(monrules); j++) {
			mr = &monrules[j];
			if ((mr->monitor == -1 || mr->monitor == mi) && (mr->tag == -1 || mr->tag == i)) {
				layout = MAX(mr->layout, 0);
				layout = MIN(layout, LENGTH(layouts) - 1);
				m->pertag->ltidxs[i][0] = &layouts[layout];
				m->pertag->ltidxs[i][1] = m->lt[0];
				m->pertag->nmasters[i] = (mr->nmaster > -1 ? mr->nmaster : m->nmaster);
				m->pertag->mfacts[i] = (mr->mfact > -1 ? mr->mfact : m->mfact);
				m->pertag->showbars[i] = (mr->showbar > -1 ? mr->showbar : m->showbar);
				break;
			}
		}
	}

	return m;
}

void
cyclelayout(const Arg *arg) {
	Layout *l;
	for(l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++);
	if(arg->i > 0) {
		if(l->symbol && (l + 1)->symbol)
			setlayout(&((Arg) { .v = (l + 1) }));
		else
			setlayout(&((Arg) { .v = layouts }));
	} else {
		if(l != layouts && (l - 1)->symbol)
			setlayout(&((Arg) { .v = (l - 1) }));
		else
			setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
	}
}

void
destroynotify(XEvent *e)
{
	Client *c, *swee, *root;
  int ClientType;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

  ClientType = wintoclient2(ev->window, &c, &root);

	//if ((c = wintoclient(ev->window)))
  if (ClientType == ClientRegular)
		unmanage(c, 1);
  else if (ClientType == ClientSwallowee) {
    swalstop(c, NULL);
    unmanage(c, 1);
  } else if (ClientType == ClientSwallower) {
    /* If the swallower is swallowed by another client, terminate the
     * swallow. This cuts off the swallow chain after the client. */
    swalstop(c, root);

    /* Cut off the swallow chain before the client. */
    for (swee = root; swee->swallowedby != c; swee = swee->swallowedby);
    swee->swallowedby = NULL;

    free(c);
    updateclientlist();
  } else if (showsystray && (c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		updatesystray(1);
	} 
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

int
drawstatusbar(Monitor *m, int bh, char* stext) {
	int ret, i, j, w, x, len;
	short isCode = 0;
	char *text;
	char *p;

	len = strlen(stext) + 1 ;
	if (!(text = (char*) malloc(sizeof(char)*len)))
		die("malloc");
	p = text;

	i = -1, j = 0;
	while (stext[++i])
		if ((unsigned char)stext[i] >= ' ')
			text[j++] = stext[i];
	text[j] = '\0';

	/* compute width of the status text */
	w = 0;
	i = -1;
	while (text[++i]) {
		if (text[i] == '^') {
			if (!isCode) {
				isCode = 1;
				text[i] = '\0';
				w += TEXTW(text) - lrpad;
				text[i] = '^';
				if (text[++i] == 'f')
					w += atoi(text + ++i);
			} else {
				isCode = 0;
				text = text + i + 1;
				i = -1;
			}
		}
	}
	if (!isCode)
		w += TEXTW(text) - lrpad;
	else
		isCode = 0;
	text = p;

	w += 2; /* 1px padding on both sides */
	ret = x = m->ww - w;
	x = m->ww - w - getsystraywidth();

	drw_setscheme(drw, scheme[LENGTH(colors)]);
	drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
	drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
	drw_rect(drw, x, 0, w, bh, 1, 1);
	x++;

	/* process status text */
	i = -1;
	while (text[++i]) {
		if (text[i] == '^' && !isCode) {
			isCode = 1;

			text[i] = '\0';
			w = TEXTW(text) - lrpad;
			drw_text(drw, x - 2 * sp, 0, w, bh, 0, text, 0);

			x += w;

			/* process code */
			while (text[++i] != '^') {
				if (text[i] == 'c') {
					char buf[8];
					memcpy(buf, (char*)text+i+1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColFg], buf, 0);
					i += 7;
				} else if (text[i] == 'b') {
					char buf[8];
					memcpy(buf, (char*)text+i+1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColBg], buf, 0);
					i += 7;
				} else if (text[i] == 'd') {
					drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
					drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
				} else if (text[i] == 'r') {
					int rx = atoi(text + ++i);
					while (text[++i] != ',');
					int ry = atoi(text + ++i);
					while (text[++i] != ',');
					int rw = atoi(text + ++i);
					while (text[++i] != ',');
					int rh = atoi(text + ++i);

					drw_rect(drw, rx + x, ry, rw, rh, 1, 0);
				} else if (text[i] == 'f') {
					x += atoi(text + ++i);
				}
			}

			text = text + i + 1;
			i=-1;
			isCode = 0;
		}
	}

	if (!isCode) {
		w = TEXTW(text) - lrpad;
		drw_text(drw, x - 2 * sp, 0, w, bh, 0, text, 0);
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	free(p);

	return ret;
}

void
drawbar(Monitor *m)
{
	int x, w, tw = 0, stw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	if (!m->showbar)
		return;
 
	if (showsystray && m == systraytomon(m)) {
		stw = getsystraywidth();
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, m->ww - stw, 0, stw, bh, 1, 1);
	}


	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		//drw_setscheme(drw, scheme[SchemeNorm]);
		//tw = TEXTW(stext) - lrpad + 2; // 2px right padding
		//drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);
		//drw_text(drw, m->ww - tw - stw, 0, tw, bh, 0, stext, 0);
		tw = statusw = m->ww - drawstatusbar(m, bh, stext);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, (m->tagset[m->seltags] & 1 << i ? tagscheme[i] : scheme[SchemeNorm]));
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	/* Draw swalsymbol next to ltsymbol. */
	if (m->sel && m->sel->swallowedby) {
		w = TEXTW(swalsymbol);
		x = drw_text(drw, x, 0, w, bh, lrpad / 2, swalsymbol, 0);
	}

	if ((w = m->ww - tw - stw - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			/*
			drw_text(drw, x, 0, w - 2 * sp, bh, lrpad / 2, m->sel->name, 0);
			*/
			if (TEXTW(m->sel->name) > w) /* title is bigger than the width of the title rectangle, don't center */
				//drw_text(drw, x, 0, w - 2 * sp, bh, lrpad / 2, m->sel->name, 0);
  			drw_text(drw, x, 0, w - 2 * sp, bh, lrpad / 2 + (m->sel->icon ? m->sel->icw + ICONSPACING : 0), m->sel->name, 0);
			else /* center window title */
				//drw_text(drw, x, 0, w - 2 * sp, bh, (w - TEXTW(m->sel->name)) / 2, m->sel->name, 0);
  			drw_text(drw, x, 0, w - 2 * sp, bh, (w - TEXTW(m->sel->name)) / 2 + (m->sel->icon ? m->sel->icw + ICONSPACING : 0), m->sel->name, 0);
			if (m->sel->icon) drw_pic(drw, x + lrpad / 2, (bh - m->sel->ich) / 2, m->sel->icw, m->sel->ich, m->sel->icon);
			if (m->sel->isfloating)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w - 2 * sp, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);

	if (showsystray && !systraypinning)
		updatesystray(0);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);

		if (showsystray && m == systraytomon(m))
			updatesystray(0);
	}
}

int
fakesignal(void)
{
	/* Command syntax: <PREFIX><COMMAND>[<SEP><ARG>]... */
	static const char sep[] = "###";
	static const char prefix[] = "#!";

	size_t numsegments, numargs;
	char rootname[256];
	char *segments[16] = {0};

	/* Get root name, split by separator and find the prefix */
	if (!gettextprop(root, XA_WM_NAME, rootname, sizeof(rootname))
		|| strncmp(rootname, prefix, sizeof(prefix) - 1)) {
		return 0;
	}
	numsegments = split(rootname + sizeof(prefix) - 1, sep, segments, sizeof(segments));
	numargs = numsegments - 1; /* number of arguments to COMMAND */

	if (!strcmp(segments[0], "swalreg")) {
		/* Params: windowid, [class], [instance], [title] */
		Window w;
		Client *c;

		if (numargs >= 1) {
			w = strtoul(segments[1], NULL, 0);
			switch (wintoclient2(w, &c, NULL)) {
			case ClientRegular: /* fallthrough */
			case ClientSwallowee:
				swalreg(c, segments[2], segments[3], segments[4]);
				break;
			}
		}
	}
	else if (!strcmp(segments[0], "swal")) {
		/* Params: swallower's windowid, swallowee's window-id */
		Client *swer, *swee;
		Window winswer, winswee;
		int typeswer, typeswee;

		if (numargs >= 2) {
			winswer = strtoul(segments[1], NULL, 0);
			typeswer = wintoclient2(winswer, &swer, NULL);
			winswee = strtoul(segments[2], NULL, 0);
			typeswee = wintoclient2(winswee, &swee, NULL);
			if ((typeswer == ClientRegular || typeswer == ClientSwallowee)
				&& (typeswee == ClientRegular || typeswee == ClientSwallowee))
				swal(swer, swee, 0);
		}
	}
	else if (!strcmp(segments[0], "swalunreg")) {
		/* Params: swallower's windowid */
		Client *swer;
		Window winswer;

		if (numargs == 1) {
			winswer = strtoul(segments[1], NULL, 0);
			if ((swer = wintoclient(winswer)))
				swalunreg(swer);
		}
	}
	else if (!strcmp(segments[0], "swalstop")) {
		/* Params: swallowee's windowid */
		Client *swee;
		Window winswee;

		if (numargs == 1) {
			winswee = strtoul(segments[1], NULL, 0);
			if ((swee = wintoclient(winswee)))
				swalstop(swee, NULL);
		}
	}
	return 1;
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	/* FIXME getatomprop should return the number of items and a pointer to
	 * the stored data instead of this workaround */
	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

static uint32_t prealpha(uint32_t p) {
	uint8_t a = p >> 24u;
	uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
	uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

Picture
geticonprop(Window win, unsigned int *picw, unsigned int *pich)
{
	int format;
	unsigned long n, extra, *p = NULL;
	Atom real;

	if (XGetWindowProperty(dpy, win, netatom[NetWMIcon], 0L, LONG_MAX, False, AnyPropertyType, 
						   &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return None; 
	if (n == 0 || format != 32) { XFree(p); return None; }

	unsigned long *bstp = NULL;
	uint32_t w, h, sz;
	{
		unsigned long *i; const unsigned long *end = p + n;
		uint32_t bstd = UINT32_MAX, d, m;
		for (i = p; i < end - 1; i += sz) {
			if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
			if ((sz = w * h) > end - i) break;
			if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) { bstd = d; bstp = i; }
		}
		if (!bstp) {
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
				if ((sz = w * h) > end - i) break;
				if ((d = ICONSIZE - (w > h ? w : h)) < bstd) { bstd = d; bstp = i; }
			}
		}
		if (!bstp) { XFree(p); return None; }
	}

	if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) { XFree(p); return None; }

	uint32_t icw, ich;
	if (w <= h) {
		ich = ICONSIZE; icw = w * ICONSIZE / h;
		if (icw == 0) icw = 1;
	}
	else {
		icw = ICONSIZE; ich = h * ICONSIZE / w;
		if (ich == 0) ich = 1;
	}
	*picw = icw; *pich = ich;

	uint32_t i, *bstp32 = (uint32_t *)bstp;
	for (sz = w * h, i = 0; i < sz; ++i) bstp32[i] = prealpha(bstp[i]);

	Picture ret = drw_picture_create_resized(drw, (char *)bstp, w, h, icw, ich);
	XFree(p);

	return ret;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

unsigned int
getsystraywidth()
{
	unsigned int w = 0;
	Client *i;
	if (showsystray)
		for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next);
	return w ? w + systrayspacing : 0;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;
		KeyCode chain;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym))) {
				if (keys[i].chain != -1 &&
					((chain = XKeysymToKeycode(dpy, keys[i].chain))))
						code = chain;
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
			}
	}
}

void
incnmaster(const Arg *arg)
{
	unsigned int i;
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	for(i=0; i<LENGTH(tags); ++i)
		if(selmon->tagset[selmon->seltags] & 1<<i)
			selmon->pertag->nmasters[i+1] = selmon->nmaster;
	
	if(selmon->pertag->curtag == 0)
	{
		selmon->pertag->nmasters[0] = selmon->nmaster;
	}
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i, j;
	KeySym keysym;
	XKeyEvent *ev;
	int current = 0;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++) {
		if (keysym == keys[i].keysym && keys[i].chain == -1
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
				&& keys[i].func)
			keys[i].func(&(keys[i].arg));
		else if (keysym == keys[i].chain && keychain == -1
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
				&& keys[i].func) {
			current = 1;
			keychain = keysym;
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabKey(dpy, AnyKey, 0 | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
		} else if (!current && keysym == keys[i].keysym
				&& keychain != -1
				&& keys[i].chain == keychain
				&& keys[i].func)
			keys[i].func(&(keys[i].arg));
	}
	if (!current) {
		keychain = -1;
		grabkeys();
	}
}

int
fake_signal(void)
{
	char fsignal[256];
	char indicator[9] = "fsignal:";
	char str_signum[16];
	int i, v, signum;
	size_t len_fsignal, len_indicator = strlen(indicator);

	// Get root name property
	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
		len_fsignal = strlen(fsignal);

		// Check if this is indeed a fake signal
		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
			memcpy(str_signum, &fsignal[len_indicator], len_fsignal - len_indicator);
			str_signum[len_fsignal - len_indicator] = '\0';

			// Convert string value into managable integer
			for (i = signum = 0; i < strlen(str_signum); i++) {
				v = str_signum[i] - '0';
				if (v >= 0 && v <= 9) {
					signum = signum * 10 + v;
				}
			}

			// Check if a signal was found, and if so handle it
			if (signum)
				for (i = 0; i < LENGTH(signals); i++)
					if (signum == signals[i].signum && signals[i].func)
						signals[i].func(&(signals[i].arg));

			// A fake signal was sent
			return 1;
		}
	}

	// No fake signal was sent, so proceed with update
	return 0;
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
layoutmenu(const Arg *arg) {
	FILE *p;
	char c[3], *s;
	int i;

	if (!(p = popen(layoutmenu_cmd, "r")))
		 return;
	s = fgets(c, sizeof(c), p);
	pclose(p);

	if (!s || *s == '\0' || c[0] == '\0')
		 return;

	i = atoi(c);
	setlayout(&((Arg) { .v = &layouts[i] }));
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updateicon(c);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	Client *c, *swee, *root;
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;
	Swallow *s;

	Client *i;
	if (showsystray && (i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		updatesystray(1);
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
  /*
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
    */
	switch (wintoclient2(ev->window, &c, &root)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		/* Regulars and swallowees are always mapped. Nothing to do. */
		break;
	case ClientSwallower:
		/* Remapping a swallower will simply stop the swallow. */
		for (swee = root; swee->swallowedby != c; swee = swee->swallowedby);
		swalstop(swee, root);
		break;
	default:
		/* No client is managing the window. See if any swallows match. */
		if ((s = swalmatch(ev->window)))
			swalmanage(s, ev->window, &wa);
		else
			manage(ev->window, &wa);
		break;
	}

	/* Reduce decay counter of all swallow instances. */
	if (swaldecay)
		swaldecayby(1);
}

/*
void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) // override layout symbol
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}
*/

/*
 * from https://gist.github.com/gbgabo/ef588d7ea043ad5fb60d8369250842b7
 */

void
monocle(Monitor *m)
{
	unsigned int n = 0, oe = enablegaps;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (smartgaps == n) {
		oe = 0; // outer gaps disabled
	}
	
	n = 0;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
			if (n > 0) /* override layout symbol */
				snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
				int newx, newy, neww, newh;

				for (c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
					if (m->gappoh == 0) {
						newx = m->wx - c->bw;
						newy = m->wy - c->bw;
						neww = m->ww;
						newh = m->wh;
					} else {
						newx = m->wx + m->gappoh*oe - c->bw;
						newy = m->wy + m->gappoh*oe - c->bw;
						neww = m->ww - 2 * (m->gappoh*oe + c->bw);
						newh = m->wh - 2 * (m->gappoh*oe + c->bw);
					}
					applysizehints(c, &newx, &newy, &neww, &newh, 0);
					if (neww < m->ww)
						newx = m->wx + (m->ww - (neww + 2 * c->bw)) / 2;
					if (newh < m->wh)
						newy = m->wy + (m->wh - (newh + 2 * c->bw)) / 2;
						resize(c, newx, newy, neww, newh, 0);
				}

}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen
	&&  !c->isfakefullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

unsigned int
nexttag(void)
{
	unsigned int seltag = selmon->tagset[selmon->seltags];
	return seltag == (1 << (LENGTH(tags) - 1)) ? 1 : seltag << 1;
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

unsigned int
prevtag(void)
{
	unsigned int seltag = selmon->tagset[selmon->seltags];
	return seltag == 1 ? (1 << (LENGTH(tags) - 1)) : seltag >> 1;
}

Client *
prevtiled(Client *c) {
	Client *p, *r;

	for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
		if(!p->isfloating && ISVISIBLE(p))
			r = p;
	return r;
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	Swallow *s;
	XPropertyEvent *ev = &e->xproperty;

	if (showsystray && (c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		}
		else
			updatesystrayiconstate(c, ev);
		updatesystray(1);
	}

	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		if (!fake_signal() && !fakesignal())
			updatestatus();
  } else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		else if (ev->atom == netatom[NetWMIcon]) {
			updateicon(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
			if (swalretroactive && (s = swalmatch(c->win))) {
				swal(s->client, c, 0);
			}
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
saveSession(void)
{
	FILE *fw = fopen(SESSION_FILE, "w");
	for (Client *c = selmon->clients; c != NULL; c = c->next) { // get all the clients with their tags and write them to the file
		fprintf(fw, "%lu %u\n", c->win, c->tags);
	}
	fclose(fw);
}

void
restoreSession(void)
{
	// restore session
	FILE *fr = fopen(SESSION_FILE, "r");
	if (!fr)
		return;

	char *str = malloc(23 * sizeof(char)); // allocate enough space for excepted input from text file
	while (fscanf(fr, "%[^\n] ", str) != EOF) { // read file till the end
		long unsigned int winId;
		unsigned int tagsForWin;
		int check = sscanf(str, "%lu %u", &winId, &tagsForWin); // get data
		if (check != 2) // break loop if data wasn't read correctly
			break;
		
		for (Client *c = selmon->clients; c ; c = c->next) { // add tags to every window by winId
			if (c->win == winId) {
				c->tags = tagsForWin;
				break;
			}
		}
    }

	for (Client *c = selmon->clients; c ; c = c->next) { // refocus on windows
		focus(c);
		restack(c->mon);
	}

	for (Monitor *m = selmon; m; m = m->next) // rearrange all monitors
		arrange(m);

	free(str);
	fclose(fr);
	
	// delete a file
	remove(SESSION_FILE);
}

void
pushdown(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating)
		return;
	if((c = nexttiled(sel->next))) {
		detach(sel);
		sel->next = c->next;
		c->next = sel;
	} else {
		detach(sel);
		attach(sel);
	}
	focus(sel);
	arrange(selmon);
}

void
pushup(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating)
		return;
	if((c = prevtiled(sel))) {
		detach(sel);
		sel->next = c;
		if(selmon->clients == c)
			selmon->clients = sel;
		else {
			for(c = selmon->clients; c->next != sel->next; c = c->next);
			c->next = sel;
		}
	} else {
		for(c = sel; c->next; c = c->next);
		detach(sel);
		sel->next = NULL;
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

void
quit(const Arg *arg)
{
	if(arg->i) restart = 1;
	running = 0;

	if (restart == 1)
		saveSession();
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	if (c->isfloating)
		wc.border_width = c->floatborderpx;
	else
		wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen
	&&  !c->isfakefullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		updatesystray(1);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
runautostart(void)
{
	char *pathpfx;
	char *path;
	char *xdgdatahome;
	char *home;
	struct stat sb;

	if ((home = getenv("HOME")) == NULL)
		/* this is almost impossible */
		return;

	/* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
	 * otherwise use ~/.local/share/dwm as autostart script directory
	 */
	xdgdatahome = getenv("XDG_DATA_HOME");
	if (xdgdatahome != NULL && *xdgdatahome != '\0') {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

		if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	} else {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(home) + strlen(localshare)
		                     + strlen(dwmdir) + 3);

		if (sprintf(pathpfx, "%s/%s/%s", home, localshare, dwmdir) < 0) {
			free(pathpfx);
			return;
		}
	}

	/* check if the autostart script directory exists */
	if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		/* the XDG conformant path does not exist or is no directory
		 * so we try ~/.dwm instead
		 */
		char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
		if(pathpfx_new == NULL) {
			free(pathpfx);
			return;
		}
		pathpfx = pathpfx_new;

		if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	}

	/* try the blocking script first */
	path = ecalloc(1, strlen(pathpfx) + strlen(autostartblocksh) + 2);
	if (sprintf(path, "%s/%s", pathpfx, autostartblocksh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(path);

	/* now the non-blocking script */
	if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(strcat(path, " &"));

	free(pathpfx);
	free(path);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

static void scratchpad_hide ()
{
	if (selmon -> sel)
	{
		selmon -> sel -> tags = SCRATCHPAD_MASK;
		selmon -> sel -> isfloating = 1;
		focus(NULL);
		arrange(selmon);
	}
}

static _Bool scratchpad_last_showed_is_killed (void)
{
	_Bool killed = 1;
	for (Client * c = selmon -> clients; c != NULL; c = c -> next)
	{
		if (c == scratchpad_last_showed)
		{
			killed = 0;
			break;
		}
	}
	return killed;
}

static void scratchpad_remove ()
{
	if (selmon -> sel && scratchpad_last_showed != NULL && selmon -> sel == scratchpad_last_showed)
		scratchpad_last_showed = NULL;
}

static void scratchpad_show ()
{
	if (scratchpad_last_showed == NULL || scratchpad_last_showed_is_killed ())
		scratchpad_show_first ();
	else
	{
		if (scratchpad_last_showed -> tags != SCRATCHPAD_MASK)
		{
			scratchpad_last_showed -> tags = SCRATCHPAD_MASK;
			focus(NULL);
			arrange(selmon);
		}
		else
		{
			_Bool found_current = 0;
			_Bool found_next = 0;
			for (Client * c = selmon -> clients; c != NULL; c = c -> next)
			{
				if (found_current == 0)
				{
					if (c == scratchpad_last_showed)
					{
						found_current = 1;
						continue;
					}
				}
				else
				{
					if (c -> tags == SCRATCHPAD_MASK)
					{
						found_next = 1;
						scratchpad_show_client (c);
						break;
					}
				}
			}
			if (found_next == 0) scratchpad_show_first ();
		}
	}
}

static void scratchpad_show_client (Client * c)
{
	scratchpad_last_showed = c;
	c -> tags = selmon->tagset[selmon->seltags];
	focus(c);
	arrange(selmon);
}

static void scratchpad_show_first (void)
{
	for (Client * c = selmon -> clients; c != NULL; c = c -> next)
	{
		if (c -> tags == SCRATCHPAD_MASK)
		{
			scratchpad_show_client (c);
			break;
		}
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols, mt;
	int exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	} else {
		exists = True;
		mt = proto;
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		if (c->isfakefullscreen) {
			resizeclient(c, c->x, c->y, c->w, c->h);
			return;
		}
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		if (c->isfakefullscreen) {
			resizeclient(c, c->x, c->y, c->w, c->h);
			return;
		}
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	unsigned int i;
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);

	for(i=0; i<LENGTH(tags); ++i)
		if(selmon->tagset[selmon->seltags] & 1<<i)
		{
			selmon->pertag->ltidxs[i+1][selmon->sellt] = selmon->lt[selmon->sellt]; 
			selmon->pertag->sellts[i+1] = selmon->sellt;
		}
	
	if(selmon->pertag->curtag == 0)
	{
		selmon->pertag->ltidxs[0][selmon->sellt] = selmon->lt[selmon->sellt]; 
		selmon->pertag->sellts[0] = selmon->sellt;
	}

	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setcfact(const Arg *arg) {
	float f;
	Client *c;

	c = selmon->sel;

	if(!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if(arg->f == 0.0)
		f = 1.0;
	else if(f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;
	unsigned int i;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (arg->f == 0.0)
		f = mfact;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	for(i=0; i<LENGTH(tags); ++i)
		if(selmon->tagset[selmon->seltags] & 1<<i)
			selmon->pertag->mfacts[i+1] = f;

	if(selmon->pertag->curtag == 0)
	{
		selmon->pertag->mfacts[0] = f;
	}
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	xinitvisual();
	drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = user_bh ? user_bh : drw->fonts->h + 2;
	updategeom();
	sp = sidepad;
	vp = (topbar == 1) ? vertpad : - vertpad;

	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetSystemTrayVisual] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	cursor[CurSwal] = drw_cur_create(drw, XC_bottom_side);
	/* init appearance */
	if (LENGTH(tags) > LENGTH(tagsel))
		die("too few color schemes for the tags");
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], alphas[0], 3);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
	tagscheme = ecalloc(LENGTH(tagsel), sizeof(Clr *));
	for (i = 0; i < LENGTH(tagsel); i++)
		tagscheme[i] = drw_scm_create(drw, tagsel[i], tagalpha, 2);
	/* init system tray */
	if (showsystray)
		updatesystray(0);
	/* init bars */
	updatebars();
	updatestatus();
	updatebarpos(selmon);
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating)
                &&  (!c->isfullscreen || c->isfakefullscreen))
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
sighup(int unused)
{
	Arg a = {.i = 1};
	quit(&a);
}

void
sigterm(int unused)
{
	Arg a = {.i = 0};
	quit(&a);
}

void
spawn(const Arg *arg)
{
	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		if (arg->v == statuscmd) {
			for (int i = 0; i < LENGTH(statuscmds); i++) {
				if (statuscmdn == statuscmds[i].id) {
					statuscmd[2] = statuscmds[i].cmd;
					setenv("BUTTON", lastbutton, 1);
					break;
				}
			}
			if (!statuscmd[2])
				exit(EXIT_SUCCESS);
		}
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

Monitor *
systraytomon(Monitor *m)
{
	Monitor *t;
	int i, n;
	if (!systraypinning) {
		if (!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for (n = 1, t = mons; t && t->next; n++, t = t->next);
	for (i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next);
	if (systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

/*
 * Perform immediate swallow of client 'swee' by client 'swer'. 'manage' shall
 * be set if swal() is called from swalmanage(). 'swer' and 'swee' must be
 * regular or swallowee, but not swallower.
 */
void
swal(Client *swer, Client *swee, int manage)
{
	Client *c, **pc;
	int sweefocused = selmon->sel == swee;

	/* Remove any swallows registered for the swer. Asking a swallower to
	 * swallow another window is ambiguous and is thus avoided altogether. In
	 * contrast, a swallowee can swallow in a well-defined manner by attaching
	 * to the head of the swallow chain. */
	if (!manage)
		swalunreg(swer);

	/* Disable fullscreen prior to swallow. Swallows involving fullscreen
	 * windows produces quirky artefacts such as fullscreen terminals or tiled
	 * pseudo-fullscreen windows. */
	setfullscreen(swer, 0);
	setfullscreen(swee, 0);

	/* Swap swallowee into client and focus lists. Keeps current focus unless
	 * the swer (which gets unmapped) is focused in which case the swee will
	 * receive focus. */
	detach(swee);
	for (pc = &swer->mon->clients; *pc && *pc != swer; pc = &(*pc)->next);
	*pc = swee;
	swee->next = swer->next;
	detachstack(swee);
	for (pc = &swer->mon->stack; *pc && *pc != swer; pc = &(*pc)->snext);
	*pc = swee;
	swee->snext = swer->snext;
	swee->mon = swer->mon;
	if (sweefocused) {
		detachstack(swee);
		attachstack(swee);
		selmon = swer->mon;
	}
	swee->tags = swer->tags;
	swee->isfloating = swer->isfloating;
	for (c = swee; c->swallowedby; c = c->swallowedby);
	c->swallowedby = swer;

	/* Configure geometry params obtained from patches (e.g. cfacts) here. */
	// swee->cfact = swer->cfact;

	/* ICCCM 4.1.3.1 */
	setclientstate(swer, WithdrawnState);
	if (manage)
		setclientstate(swee, NormalState);

	if (swee->isfloating || !swee->mon->lt[swee->mon->sellt]->arrange)
		XRaiseWindow(dpy, swee->win);
	resize(swee, swer->x, swer->y, swer->w, swer->h, 0);

	focus(NULL);
	arrange(NULL);
	if (manage)
		XMapWindow(dpy, swee->win);
	XUnmapWindow(dpy, swer->win);
	restack(swer->mon);
}

/*
 * Register a future swallow with swallower. 'c' 'class', 'inst' and 'title'
 * shall point null-terminated strings or be NULL, implying a wildcard. If an
 * already existing swallow instance targets 'c' its filters are updated and no
 * new swallow instance is created. 'c' may be ClientRegular or ClientSwallowee.
 * Complement to swalrm().
 */
void swalreg(Client *c, const char *class, const char *inst, const char *title)
{
	Swallow *s;

	if (!c)
		return;

	for (s = swallows; s; s = s->next) {
		if (s->client == c) {
			if (class)
				strncpy(s->class, class, sizeof(s->class) - 1);
			else
				s->class[0] = '\0';
			if (inst)
				strncpy(s->inst, inst, sizeof(s->inst) - 1);
			else
				s->inst[0] = '\0';
			if (title)
				strncpy(s->title, title, sizeof(s->title) - 1);
			else
				s->title[0] = '\0';
			s->decay = swaldecay;

			/* Only one swallow per client. May return after first hit. */
			return;
		}
	}

	s = ecalloc(1, sizeof(Swallow));
	s->decay = swaldecay;
	s->client = c;
	if (class)
		strncpy(s->class, class, sizeof(s->class) - 1);
	if (inst)
		strncpy(s->inst, inst, sizeof(s->inst) - 1);
	if (title)
		strncpy(s->title, title, sizeof(s->title) - 1);

	s->next = swallows;
	swallows = s;
}

/*
 * Decrease decay counter of all registered swallows by 'decayby' and remove any
 * swallow instances whose counter is less than or equal to zero.
 */
void
swaldecayby(int decayby)
{
	Swallow *s, *t;

	for (s = swallows; s; s = t) {
		s->decay -= decayby;
		t = s->next;
		if (s->decay <= 0)
			swalrm(s);
	}
}

/*
 * Window configuration and client setup for new windows which are to be
 * swallowed immediately. Pendant to manage() for such windows.
 */
void
swalmanage(Swallow *s, Window w, XWindowAttributes *wa)
{
	Client *swee, *swer;
	XWindowChanges wc;

	swer = s->client;
	swalrm(s);

	/* Perform bare minimum setup of a client for window 'w' such that swal()
	 * may be used to perform the swallow. The following lines are basically a
	 * minimal implementation of manage() with a few chunks delegated to
	 * swal(). */
	swee = ecalloc(1, sizeof(Client));
	swee->win = w;
	swee->mon = swer->mon;
	swee->oldbw = wa->border_width;
	swee->bw = borderpx;
	attach(swee);
	attachstack(swee);
	updatetitle(swee);
	updatesizehints(swee);
	XSelectInput(dpy, swee->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	wc.border_width = swee->bw;
	XConfigureWindow(dpy, swee->win, CWBorderWidth, &wc);
	grabbuttons(swee, 0);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(swee->win), 1);

	swal(swer, swee, 1);
}

/*
 * Return swallow instance which targets window 'w' as determined by its class
 * name, instance name and window title. Returns NULL if none is found. Pendant
 * to wintoclient().
 */
Swallow *
swalmatch(Window w)
{
	XClassHint ch = { NULL, NULL };
	Swallow *s = NULL;
	char title[sizeof(s->title)];

	XGetClassHint(dpy, w, &ch);
	if (!gettextprop(w, netatom[NetWMName], title, sizeof(title)))
		gettextprop(w, XA_WM_NAME, title, sizeof(title));

	for (s = swallows; s; s = s->next) {
		if ((!ch.res_class || strstr(ch.res_class, s->class))
			&& (!ch.res_name || strstr(ch.res_name, s->inst))
			&& (title[0] == '\0' || strstr(title, s->title)))
			break;
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	return s;
}

/*
 * Interactive drag-and-drop swallow.
 */
void
swalmouse(const Arg *arg)
{
	Client *swer, *swee;
	XEvent ev;

	if (!(swee = selmon->sel))
		return;

	if (XGrabPointer(dpy, root, False, ButtonPressMask|ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, cursor[CurSwal]->cursor, CurrentTime) != GrabSuccess)
		return;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest: /* fallthrough */
		case Expose: /* fallthrough */
		case MapRequest:
			handler[ev.type](&ev);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);

	if ((swer = wintoclient(ev.xbutton.subwindow))
		&& swer != swee)
		swal(swer, swee, 0);

	/* Remove accumulated pending EnterWindow events caused by the mouse
	 * movements. */
	XCheckMaskEvent(dpy, EnterWindowMask, &ev);
}

/*
 * Delete swallow instance swallows and free its resources. Complement to
 * swalreg(). If NULL is passed all swallows are deleted from.
 */
void
swalrm(Swallow *s)
{
	Swallow *t, **ps;

	if (s) {
		for (ps = &swallows; *ps && *ps != s; ps = &(*ps)->next);
		*ps = s->next;
		free(s);
	}
	else {
		for(s = swallows; s; s = t) {
			t = s->next;
			free(s);
		}
		swallows = NULL;
	}
}

/*
 * Removes swallow instance targeting 'c' if it exists. Complement to swalreg().
 */
void swalunreg(Client *c) { Swallow *s;

	for (s = swallows; s; s = s->next) {
		if (c == s->client) {
			swalrm(s);
			/* Max. 1 registered swallow per client. No need to continue. */
			break;
		}
	}
}

/*
 * Stop an active swallow of swallowed client 'swee' and remap the swallower.
 * If 'swee' is a swallower itself 'root' must point the root client of the
 * swallow chain containing 'swee'.
 */
void
swalstop(Client *swee, Client *root)
{
	Client *swer;

	if (!swee || !(swer = swee->swallowedby))
		return;

	swee->swallowedby = NULL;
	root = root ? root : swee;
	swer->mon = root->mon;
	swer->tags = root->tags;
	swer->next = root->next;
	root->next = swer;
	swer->snext = root->snext;
	root->snext = swer;
	swer->isfloating = swee->isfloating;

	/* Configure geometry params obtained from patches (e.g. cfacts) here. */
	// swer->cfact = 1.0;

	/* If swer is not in tiling mode reuse swee's geometry. */
	if (swer->isfloating || !root->mon->lt[root->mon->sellt]->arrange) {
		XRaiseWindow(dpy, swer->win);
		resize(swer, swee->x, swee->y, swee->w, swee->h, 0);
	}

	/* Override swer's border scheme which may be using SchemeSel. */
	XSetWindowBorder(dpy, swer->win, scheme[SchemeNorm][ColBorder].pixel);

	/* ICCCM 4.1.3.1 */
	setclientstate(swer, NormalState);

	XMapWindow(dpy, swer->win);
	focus(NULL);
	arrange(swer->mon);
}

/*
 * Stop active swallow for currently selected client.
 */
void
swalstopsel(const Arg *unused)
{
	if (selmon->sel)
		swalstop(selmon->sel, NULL);
}

void
altTab(const int cycle)
{
	/* move to next window */
	if (selmon->sel != NULL && selmon->sel->snext != NULL) {
    if (cycle > 0) {
      selmon->altTabN++;
      if (selmon->altTabN >= selmon->nTabs)
        selmon->altTabN = 0; /* reset altTabN */

      focus(selmon->altsnext[selmon->altTabN]);
      restack(selmon);
    } else if (cycle < 0) {
      selmon->altTabN--;
      if (selmon->altTabN < 0)
        selmon->altTabN = selmon->nTabs - 1; /* reset altTabN */

      focus(selmon->altsnext[selmon->altTabN]);
      restack(selmon);
    }
	}

	/* redraw tab */
	XRaiseWindow(dpy, selmon->tabwin);
	drawTab(selmon->nTabs, 0, selmon);
}

void
altTabEnd()
{
	if (selmon->isAlt == 0)
		return;

	/*
	* move all clients between 1st and choosen position,
	* one down in stack and put choosen client to the first position 
	* so they remain in right order for the next time that alt-tab is used
	*/
	if (selmon->nTabs > 1) {
		if (selmon->altTabN != 0) { /* if user picked original client do nothing */
			Client *buff = selmon->altsnext[selmon->altTabN];
			if (selmon->altTabN > 1)
				for (int i = selmon->altTabN;i > 0;i--)
					selmon->altsnext[i] = selmon->altsnext[i - 1];
			else /* swap them if there are just 2 clients */
				selmon->altsnext[selmon->altTabN] = selmon->altsnext[0];
			selmon->altsnext[0] = buff;
		}

		/* restack clients */
		for (int i = selmon->nTabs - 1;i >= 0;i--) {
			focus(selmon->altsnext[i]);
			restack(selmon);
		}

		free(selmon->altsnext); /* free list of clients */
	}

	/* turn off/destroy the window */
	selmon->isAlt = 0;
	selmon->nTabs = 0;
	XUnmapWindow(dpy, selmon->tabwin);
	XDestroyWindow(dpy, selmon->tabwin);
}

void
drawTab(int nwins, int first, Monitor *m)
{
	/* little documentation of functions */
	/* void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int invert); */
	/* int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, const char *text, int invert); */
	/* void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h); */

	Client *c;
	int h;

	if (first) {
		Monitor *m = selmon;
		XSetWindowAttributes wa = {
			.override_redirect = True,
      .background_pixel = 0,
      .border_pixel = 0,
      .colormap = cmap,
			.event_mask = ButtonPressMask|ExposureMask
		};

		selmon->maxWTab = maxWTab;
		selmon->maxHTab = maxHTab;

		/* decide position of tabwin */
		int posX = selmon->mx;
		int posY = selmon->my;
		if (tabPosX == 0)
			posX += 0;
		if (tabPosX == 1)
			posX += (selmon->mw / 2) - (maxWTab / 2);
		if (tabPosX == 2)
			posX += selmon->mw - maxWTab;

		if (tabPosY == 0)
			posY += selmon->mh - maxHTab;
		if (tabPosY == 1)
			posY += (selmon->mh / 2) - (maxHTab / 2);
		if (tabPosY == 2)
			posY += 0;

		h = selmon->maxHTab;
		/* XCreateWindow(display, parent, x, y, width, height, border_width, depth, class, visual, valuemask, attributes); just reference */
		m->tabwin = XCreateWindow(dpy, root, posX, posY, selmon->maxWTab, selmon->maxHTab, 2, depth,
								CopyFromParent, visual,
								CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa); /* create tabwin */

		XDefineCursor(dpy, m->tabwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->tabwin);

	}

	h = selmon->maxHTab  / m->nTabs;

	int y = 0;
	int n = 0;
	for (int i = 0;i < m->nTabs;i++) { /* draw all clients into tabwin */
		c = m->altsnext[i];
		if(!ISVISIBLE(c)) continue;
		/* if (HIDDEN(c)) continue; uncomment if you're using awesomebar patch */

		n++;
		drw_setscheme(drw, scheme[(c == m->sel) ? SchemeSel : SchemeNorm]);
		drw_text(drw, 0, y, selmon->maxWTab, h, 0, c->name, 0);
		y += h;
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_map(drw, m->tabwin, 0, 0, selmon->maxWTab, selmon->maxHTab);
}

void
altTabStart(const Arg *arg)
{
  int cycle;
	selmon->altsnext = NULL;
	if (selmon->tabwin)
		altTabEnd();

	if (selmon->isAlt == 1) {
		altTabEnd();
	} else {
		selmon->isAlt = 1;
		selmon->altTabN = 0;

		Client *c;
		Monitor *m = selmon;

		m->nTabs = 0;
		for(c = m->clients; c; c = c->next) { /* count clients */
			if(!ISVISIBLE(c)) continue;
			/* if (HIDDEN(c)) continue; uncomment if you're using awesomebar patch */

			++m->nTabs;
		}

		if (m->nTabs > 0) {
			m->altsnext = (Client **) malloc(m->nTabs * sizeof(Client *));

			int listIndex = 0;
			for(c = m->stack; c; c = c->snext) { /* add clients to the list */
				if(!ISVISIBLE(c)) continue;
				/* if (HIDDEN(c)) continue; uncomment if you're using awesomebar patch */

				m->altsnext[listIndex++] = c;
			}

			drawTab(m->nTabs, 1, m);

			struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };

			/* grab keyboard (take all input from keyboard) */
			int grabbed = 1;
			for (int i = 0;i < 1000;i++) {
				if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
					break;
				nanosleep(&ts, NULL);
				if (i == 1000 - 1)
					grabbed = 0;
			}

			XEvent event;

      cycle = arg->i;

			altTab(cycle);
			if (grabbed == 0) {
				altTabEnd();
			} else {
				while (grabbed) {
					XNextEvent(dpy, &event);
					if (event.type == KeyPress || event.type == KeyRelease) {
						if (event.type == KeyRelease && event.xkey.keycode == tabModKey) { /* if super key is released break cycle */
							break;
						} if (event.type == KeyRelease && event.xkey.keycode == tabReverseKey) { /* if super key is released break cycle */
              cycle = -cycle;
						} else if (event.type == KeyPress) {
              if (event.xkey.keycode == tabReverseKey) {
                cycle = -cycle;
              } else if (event.xkey.keycode == tabCycleKey) {/* if XK_s is pressed move to the next window */
								altTab(cycle);
							}
						}
					}
				}

				c = selmon->sel;
				altTabEnd(); /* end the alt-tab functionality */
				/* XUngrabKeyboard(display, time); just a reference */
				XUngrabKeyboard(dpy, CurrentTime); /* stop taking all input from keyboard */
				focus(c);
				restack(selmon);
			}
		} else {
			altTabEnd(); /* end the alt-tab functionality */
		}
	}
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tagtonext(const Arg *arg)
{
	unsigned int tmp;

	if (selmon->sel == NULL)
		return;

	tmp = nexttag();
	tag(&(const Arg){.ui = tmp });
	view(&(const Arg){.ui = tmp });
}

void
tagtoprev(const Arg *arg)
{
	unsigned int tmp;

	if (selmon->sel == NULL)
		return;

	tmp = prevtag();
	tag(&(const Arg){.ui = tmp });
	view(&(const Arg){.ui = tmp });
}

void
togglebar(const Arg *arg)
{
	unsigned int i;
	selmon->showbar = !selmon->showbar;
	for(i=0; i<LENGTH(tags); ++i)
		if(selmon->tagset[selmon->seltags] & 1<<i)
			selmon->pertag->showbars[i+1] = selmon->showbar;

	if(selmon->pertag->curtag == 0)
	{
		selmon->pertag->showbars[0] = selmon->showbar;
	}
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx + sp, selmon->by + vp, selmon->ww - 2 * sp, bh);
	if (showsystray) {
		XWindowChanges wc;
		if (!selmon->showbar)
			wc.y = -bh;
		else if (selmon->showbar) {
			wc.y = vp;
			if (!selmon->topbar)
				wc.y = selmon->mh - bh + vp;
		}
		XConfigureWindow(dpy, systray->win, CWY, &wc);
	}
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen
	&&  !selmon->sel->isfakefullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		/*
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
			*/
		/*restore last known float dimensions*/
		resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
		       selmon->sel->sfw, selmon->sel->sfh, False);
	else {
		/*save last known float dimensions*/
		selmon->sel->sfx = selmon->sel->x;
		selmon->sel->sfy = selmon->sel->y;
		selmon->sel->sfw = selmon->sel->w;
		selmon->sel->sfh = selmon->sel->h;
	}
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
}

void
freeicon(Client *c)
{
	if (c->icon) {
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	/* Remove all swallow instances targeting client. */
	swalunreg(c);

	detach(c);
	detachstack(c);
	freeicon(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	if (scratchpad_last_showed == c)
		scratchpad_last_showed = NULL;
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;
	int type;

	type = wintoclient2(ev->window, &c, NULL);
	if (type && ev->send_event) {
		setclientstate(c, WithdrawnState);
		return;
	}

  /*
	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
      */
  if (type == ClientRegular)
		unmanage(c, 0);
  else if (type == ClientSwallowee) {
		swalstop(c, NULL);
		unmanage(c, 0);
  } else if (type == ClientSwallower) {
		/* Swallowers are never mapped. Nothing to do. */
	} else if (showsystray && (c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		updatesystray(1);
	}
}

void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		//m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, depth,
		m->barwin = XCreateWindow(dpy, root, m->wx + sp, m->by + vp, m->ww - 2 * sp, bh, 0, depth,
		                          InputOutput, visual,
		                          CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		if (showsystray && m == systraytomon(m))
			XMapRaised(dpy, systray->win);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh = m->wh - vertpad - bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh + vertpad;
		m->wy = m->topbar ? m->wy + bh + vp : m->wy;
	} else
		m->by = -bh - vp;
}

void
updateclientlist()
{
	Client *c, *d;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			for (d = c; d; d = d->swallowedby) {
				XChangeProperty(dpy, root, netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
			}
		}
	}
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) {
		strcpy(stext, "dwm-"VERSION);
		statusw = TEXTW(stext) - lrpad + 2;
	} else {
		char *text, *s, ch;
		statusw = 0;
		for (text = s = stext; *s; s++) {
			if ((unsigned char)(*s) < ' ') {
				ch = *s;
				*s = '\0';
				statusw += TEXTW(text) - lrpad;
				*s = ch;
				text = s + 1;
			}
		}
		statusw += TEXTW(text) - lrpad + 2;
	}
	drawbar(selmon);
}

void
updatesystray(int updatebar)
{
	XSetWindowAttributes wa;
	XWindowChanges wc;
	Client *i;
	Monitor *m = systraytomon(NULL);
	unsigned int x = m->mx + m->mw;
	unsigned int w = 1, xpad = 0, ypad = 0;
	xpad = sp;
	ypad = vp;

	if (!showsystray)
		return;
	if (!systray) {
		/* init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));

		wa.override_redirect = True;
		wa.event_mask = ButtonPressMask|ExposureMask;
		wa.background_pixel = 0;
		wa.border_pixel = 0;
		wa.colormap = cmap;
		systray->win = XCreateWindow(dpy, root, x - xpad, m->by + ypad, w, bh, 0, depth,
						InputOutput, visual,
						CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)&systrayorientation, 1);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayVisual], XA_VISUALID, 32,
				PropModeReplace, (unsigned char *)&visual->visualid, 1);
		XChangeProperty(dpy, systray->win, netatom[NetWMWindowType], XA_ATOM, 32,
				PropModeReplace, (unsigned char *)&netatom[NetWMWindowTypeDock], 1);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		}
		else {
			fprintf(stderr, "dwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}

	for (w = 0, i = systray->icons; i; i = i->next) {
		wa.background_pixel = 0;
		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		w += systrayspacing;
		i->x = w;
		XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
		w += i->w;
		if (i->mon != m)
			i->mon = m;
	}
	w = w ? w + systrayspacing : 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x - xpad, m->by + ypad, w, bh);
	wc.x = x - xpad;
	wc.y = m->by + ypad;
	wc.width = w;
	wc.height = bh;
	wc.stack_mode = Above; wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	XSync(dpy, False);

	if (updatebar)
		drawbar(m);
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
	if (i) {
		i->h = bh;
		if (w == h)
			i->w = bh;
		else if (h == bh)
			i->w = w;
		else
			i->w = (int) ((float)bh * ((float)w / (float)h));
		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
		/* force icons into the systray dimensions if they don't want to */
		if (i->h > bh) {
			if (i->w == i->h)
				i->w = bh;
			else
				i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
			i->h = bh;
		}
		if (i->w > 2*bh)
			i->w = bh;
	}
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
			!(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	}
	else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	}
	else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
			systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updateicon(Client *c)
{
	freeicon(c);
	c->icon = geticonprop(c->win, &c->icw, &c->ich);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	int i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);

	focus(NULL);
	arrange(selmon);
}

void
viewnext(const Arg *arg)
{
	view(&(const Arg){.ui = nexttag()});
}

void
viewprev(const Arg *arg)
{
	view(&(const Arg){.ui = prevtag()});
}
 
Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

/*
 * Writes client managing window 'w' into 'pc' and returns type of client. If
 * no client is found NULL is written to 'pc' and zero is returned. If a client
 * is found and is a swallower (ClientSwallower) and proot is not NULL the root
 * client of the swallow chain is written to 'proot'.
 */
int
wintoclient2(Window w, Client **pc, Client **proot)
{
	Monitor *m;
	Client *c, *d;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->win == w) {
				*pc = c;
				if (c->swallowedby)
					return ClientSwallowee;
				else
					return ClientRegular;
			}
			else {
				for (d = c->swallowedby; d; d = d->swallowedby) {
					if (d->win == w) {
						if (proot)
							*proot = c;
						*pc = d;
						return ClientSwallower;
					}
				}
			}
		}
	}
	*pc = NULL;
	return 0;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

Client *
wintosystrayicon(Window w) {
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next);
	return i;
}

/* Selects for the view of the focused window. The list of tags */
/* to be displayed is matched to the focused window tag list. */
void
winview(const Arg* arg){
	Window win, win_r, win_p, *win_c;
	unsigned nc;
	int unused;
	Client* c;
	Arg a;

	if (!XGetInputFocus(dpy, &win, &unused)) return;
	while(XQueryTree(dpy, win, &win_r, &win_p, &win_c, &nc)
	      && win_p != win_r) win = win_p;

	if (!(c = wintoclient(win))) return;

	a.ui = c->tags;
	view(&a);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
xinitvisual()
{
	XVisualInfo *infos;
	XRenderPictFormat *fmt;
	int nitems;
	int i;

	XVisualInfo tpl = {
		.screen = screen,
		.depth = 32,
		.class = TrueColor
	};
	long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

	infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
	visual = NULL;
	for(i = 0; i < nitems; i ++) {
		fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
		if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
			visual = infos[i].visual;
			depth = infos[i].depth;
			cmap = XCreateColormap(dpy, root, visual, AllocNone);
			useargb = 1;
			break;
		}
	}

	XFree(infos);

	if (! visual) {
		visual = DefaultVisual(dpy, screen);
		depth = DefaultDepth(dpy, screen);
		cmap = DefaultColormap(dpy, screen);
	}
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char *sdst = NULL;
	int *idst = NULL;
	float *fdst = NULL;

	sdst = dst;
	idst = dst;
	fdst = dst;

	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "dwm", name);
	fullname[sizeof(fullname) - 1] = '\0';

	XrmGetResource(db, fullname, "*", &type, &ret);
	if (!(ret.addr == NULL || strncmp("String", type, 64)))
	{
		switch (rtype) {
		case STRING:
			strcpy(sdst, ret.addr);
			break;
		case INTEGER:
			*idst = strtoul(ret.addr, NULL, 10);
			break;
		case FLOAT:
			*fdst = strtof(ret.addr, NULL);
			break;
		}
	}
}

void
load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	XrmInitialize();
	load_xresources();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	restoreSession();
	runautostart();
	run();
	if(restart) execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
