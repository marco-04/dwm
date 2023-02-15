/* See LICENSE file for copyright and license details. */

#define SESSION_FILE "/tmp/dwm-session"

#define ICONSIZE (bh - 4)   /* icon size */
#define ICONSPACING 5 /* space between icon and title */

/* appearance */
static unsigned int borderpx  = 5;        /* border pixel of windows */
static unsigned int snap      = 32;       /* snap pixel */
static unsigned int gappih    = 20;       /* horiz inner gap between windows */
static unsigned int gappiv    = 10;       /* vert inner gap between windows */
static unsigned int gappoh    = 10;       /* horiz outer gap between windows and screen edge */
static unsigned int gappov    = 30;       /* vert outer gap between windows and screen edge */
static int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static int showbar            = 1;        /* 0 means no bar */
static int topbar             = 1;        /* 0 means bottom bar */
static int vertpad            = 10;       /* vertical padding of bar */
static int sidepad            = 10;       /* horizontal padding of bar */
static int user_bh            = 23;        /* 0 means that dwm will calculate bar height, >= 1 means dwm will user_bh as bar height */
static unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static unsigned int systrayspacing = 2;   /* systray spacing */
static int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static int showsystray             = 1;   /* 0 means no systray */
static char font[]            = "monospace:size=10";
static char dmenufont[]       = "monospace:size=10";
static char *fonts[]          = { font };
static char normbgcolor[]     = "#222222";
static char normbordercolor[] = "#444444";
static char normfgcolor[]     = "#bbbbbb";
static char selfgcolor[]      = "#eeeeee";
static char selbordercolor[]  = "#005577";
static char selbgcolor[]      = "#005577";
static const unsigned int baralpha    = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static char *colors[][3]      = {
       /*               fg           bg           border   */
       [SchemeNorm] = { normfgcolor, normbgcolor, normbordercolor },
       [SchemeSel]  = { selfgcolor,  selbgcolor,  selbordercolor  },
};
static const unsigned int alphas[][3]      = {
	/*               fg      bg        border     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static char *tagsel[][2] = {
	{ "#ffffff", "#ff0000" },
	{ "#ffffff", "#ff7f00" },
	{ "#000000", "#ffff00" },
	{ "#000000", "#00ff00" },
	{ "#ffffff", "#0000ff" },
	{ "#ffffff", "#4b0082" },
	{ "#ffffff", "#9400d3" },
	{ "#000000", "#ffffff" },
	{ "#ffffff", "#000000" },
};

static const unsigned int tagalpha[] = { OPAQUE, baralpha };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor    float x,y,w,h  floatborderpx isfakefullscreen*/
//{ "Gimp",     NULL,       NULL,       0,            1,           -1,        50,50,500,500, 5,            0 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1,        50,50,500,500, 5,            0 },
};

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = "ﳺ";

/* layout(s) */
static float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster     = 1;    /* number of clients in master area */
static int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

#define FORCE_VSPLIT 1  /* nrowgrid layout: force two clients to always split vertically */
#include "vanitygaps.c"


static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "[M]",      monocle },
	{ "[\\]",     dwindle },
	{ "HHH",      grid },
	{ "###",      nrowgrid },
	{ "---",      horizgrid },
	{ ":::",      gaplessgrid },
	{ "|M|",      centeredmaster },
	{ ">M>",      centeredfloatingmaster },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ NULL,       NULL },
};

static const MonitorRule monrules[] = {
	/* monitor  tag  layout  mfact  nmaster  showbar  topbar */
	//{  1,       -1,  2,      -1,    -1,      -1,      -1     }, // use a different layout for the second monitor
	{  -1,      -1,  0,      -1,    -1,      -1,      -1     } // default (keep it last)
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(CHAIN,KEY,TAG) \
	{ MODKEY,                       CHAIN,    KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           CHAIN,    KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             CHAIN,    KEY,      combotag,       {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, CHAIN,    KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbordercolor, "-sf", selfgcolor, NULL };
static const char *applaunchercmd[] = { "mcdmsc-applauncher", NULL };
static const char *termcmd[]  = { "tabbed_st", NULL };
static const char *layoutmenu_cmd = "layoutmenu.sh";

/* commands spawned when clicking statusbar, the mouse button pressed is exported as BUTTON */
static const StatusCmd statuscmds[] = {
	{ "notify-send Mouse$BUTTON", 1 },
};
static const char *statuscmd[] = { "/bin/sh", "-c", NULL, NULL };

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
		{ "font",               STRING,  &font },
		{ "dmenufont",          STRING,  &dmenufont },
		{ "normbgcolor",        STRING,  &normbgcolor },
		{ "normbordercolor",    STRING,  &normbordercolor },
		{ "normfgcolor",        STRING,  &normfgcolor },
		{ "selbgcolor",         STRING,  &selbgcolor },
		{ "selbordercolor",     STRING,  &selbordercolor },
		{ "selfgcolor",         STRING,  &selfgcolor },
		{ "borderpx",          	INTEGER, &borderpx },
		{ "snap",          		  INTEGER, &snap },
		{ "showbar",          	INTEGER, &showbar },
		{ "topbar",            	INTEGER, &topbar },
		{ "nmaster",          	INTEGER, &nmaster },
		{ "resizehints",       	INTEGER, &resizehints },
		{ "mfact",      	 	    FLOAT,   &mfact },
};

#include "focusurgent.c"
#include "exitdwm.c"
static Key keys[] = {
	/* modifier                     chain key key        function        argument */
  // Layouts
	{ MODKEY,                       -1,       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       -1,       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       -1,       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ControlMask,		        -1,       XK_comma,  cyclelayout,    {.i = -1 } },
	{ MODKEY|ControlMask,           -1,       XK_period, cyclelayout,    {.i = +1 } },
	{ MODKEY,                       -1,       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             -1,       XK_space,  togglefloating, {0} },
  // mfact
	{ MODKEY,                       -1,       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       -1,       XK_l,      setmfact,       {.f = +0.05} },
  // cfact
	{ MODKEY|ShiftMask,             -1,       XK_h,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,             -1,       XK_l,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,             -1,       XK_o,      setcfact,       {.f =  0.00} },
  // nmaster
	{ MODKEY,                       -1,       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       -1,       XK_d,      incnmaster,     {.i = -1 } },

  // Gaps
	{ MODKEY|Mod1Mask,              -1,       XK_u,      incrgaps,       {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_u,      incrgaps,       {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_i,      incrigaps,      {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_i,      incrigaps,      {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_o,      incrogaps,      {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_o,      incrogaps,      {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_6,      incrihgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_6,      incrihgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_7,      incrivgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_7,      incrivgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_8,      incrohgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_8,      incrohgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_9,      incrovgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_9,      incrovgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,              -1,       XK_0,      togglegaps,     {0} },
	{ MODKEY|Mod1Mask|ShiftMask,    -1,       XK_0,      defaultgaps,    {0} },
	{ MODKEY,                       -1,       XK_b,      togglebar,      {0} },

  // Launchers
	{ MODKEY,                       -1,       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             -1,       XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       -1,       XK_space,  spawn,          {.v = applaunchercmd } },

  // Focus
	{ MODKEY,                       -1,       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       -1,       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             -1,       XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             -1,       XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       -1,       XK_u,      focusurgent,    {0} },

  // Windows
	{ MODKEY,                       -1,       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       -1,       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             -1,       XK_c,      killclient,     {0} },
	{ MODKEY,                       -1,       XK_Return, zoom,           {0} },

  // Tags
	{ MODKEY,                       -1,       XK_Tab,    view,           {0} },
	{ MODKEY,                       -1,       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY,                       -1,       XK_o,      winview,        {0} },
	{ MODKEY|ShiftMask,             -1,       XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY|ShiftMask,             -1,       XK_Right,  tagtonext,      {0} },
	{ MODKEY|ShiftMask,             -1,       XK_Left,   tagtoprev,      {0} },
	{ MODKEY,                       -1,       XK_Right,  viewnext,       {0} },
	{ MODKEY,                       -1,       XK_Left,   viewprev,       {0} },
	TAGKEYS(                        -1,       XK_1,                      0)
	TAGKEYS(                        -1,       XK_2,                      1)
	TAGKEYS(                        -1,       XK_3,                      2)
	TAGKEYS(                        -1,       XK_4,                      3)
	TAGKEYS(                        -1,       XK_5,                      4)
	TAGKEYS(                        -1,       XK_6,                      5)
	TAGKEYS(                        -1,       XK_7,                      6)
	TAGKEYS(                        -1,       XK_8,                      7)
	TAGKEYS(                        -1,       XK_9,                      8)

  // Scratchpad
	{ MODKEY,                       -1,       XK_minus,  scratchpad_show, {0} },
	{ MODKEY|ShiftMask,             -1,       XK_minus,  scratchpad_hide, {0} },
	{ MODKEY|ControlMask,           -1,       XK_minus,  scratchpad_remove,{0} },

  // Quit
	{ MODKEY|ShiftMask,             -1,       XK_q,      quit,           {0} },
	{ MODKEY|ShiftMask,             -1,       XK_e,      exitdwm,        {0} },
	{ MODKEY|ControlMask|ShiftMask, -1,       XK_q,      quit,           {1} }, 
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        layoutmenu,     {0} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button1,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button3,        spawn,          {.v = statuscmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         MODKEY|ShiftMask, Button1,      swalmouse,      {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signum>"` */
static Signal signals[] = {
	/* signum       function        argument  */
	{ 1,            setlayout,      {.v = 0} },
};
