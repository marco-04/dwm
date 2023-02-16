/* See LICENSE file for copyright and license details. */

#define SESSION_FILE "/tmp/dwm-session"

/*
 * APPEARANCE AND BEHAVIOR
 */

/* --configurable with Xresources-- */
/* appearance */
static unsigned int borderpx  = 5;        /* border pixel of windows */
static unsigned int snap      = 32;       /* snap pixel */
static int showbar            = 1;        /* 0 means no bar */
static int topbar             = 1;        /* 0 means bottom bar */
static int vertpad            = 10;       /* vertical padding of bar */
static int sidepad            = 10;       /* horizontal padding of bar */
static int user_bh            = 23;        /* 0 means that dwm will calculate bar height, >= 1 means dwm will user_bh as bar height */
static char font[]            = "monospace:size=10";
static char dmenufont[]       = "monospace:size=10";
static char normbgcolor[]     = "#222222";
static char normbordercolor[] = "#444444";
static char normfgcolor[]     = "#bbbbbb";
static char selfgcolor[]      = "#eeeeee";
static char selbordercolor[]  = "#005577";
static char selbgcolor[]      = "#005577";
static char normmarkcolor[]   = "#775500";	/*border color for marked client*/
static char selmarkcolor[]    = "#775577";	/*border color for marked client on focus*/

/* tagging */
static char tagsyms[9][10] = { "", "󱃖", "", "󰎆", "󰊖", "󰍡", "", "", "󰈹" };
static char tagcolors[9][2][8] = {
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

/* layout(s) */
static float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster     = 1;    /* number of clients in master area */
static int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static int attachdirection = 4;    /* 0 default, 1 above, 2 aside, 3 below, 4 bottom, 5 top */

/* --constants-- */
#define ICONSIZE (bh - 4)   /* icon size */
#define ICONSPACING 5 /* space between icon and title */

/* alt-tab configuration */
static const unsigned int tabModKey 		= 0x40;	/* if this key is hold the alt-tab functionality stays acitve. This key must be the same as key that is used to active functin altTabStart `*/
static const unsigned int tabCycleKey 	= 0x17;	/* if this key is hit the alt-tab program moves one position forward in clients stack. This key must be the same as key that is used to active functin altTabStart */
static const unsigned int tabReverseKey	= 0x32;	/* reverses alttab cycle */
static const unsigned int tabPosY 			= 1;	/* tab position on Y axis, 0 = bottom, 1 = center, 2 = top */
static const unsigned int tabPosX 			= 1;	/* tab position on X axis, 0 = left, 1 = center, 2 = right */
static const unsigned int maxWTab 			= 600;	/* tab menu width */
static const unsigned int maxHTab 			= 200;	/* tab menu height */

/* appearance */
static char *fonts[]                    = { font };
static const unsigned int gappih        = 10;       /* horiz inner gap between windows */
static const unsigned int gappiv        = 10;       /* vert inner gap between windows */
static const unsigned int gappoh        = 10;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov        = 10;       /* vert outer gap between windows and screen edge */
static const int smartgaps              = 0;        /* 1 means no outer gap when there is only one window */
static const unsigned int baralpha      = 0xd0;
static const unsigned int borderalpha   = OPAQUE;

/* systray */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray             = 1;   /* 0 means no systray */

/* colors */
static const char *colors[][3]           = {
       /*                   fg           bg           border   */
       [SchemeNorm]     = { normfgcolor, normbgcolor, normbordercolor },
       [SchemeSel]      = { selfgcolor,  selbgcolor,  selbordercolor  },
       [SchemeNormMark] = { normfgcolor, normbgcolor, normmarkcolor   },
       [SchemeSelMark]  = { selfgcolor,  selbgcolor,  selmarkcolor    },
};

/* alphas */
static const unsigned int alphas[][3]    = {
	/*                   fg      bg        border     */
	[SchemeNorm]     = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]      = { OPAQUE, baralpha, borderalpha },
	[SchemeNormMark] = { OPAQUE, baralpha, borderalpha },
	[SchemeSelMark]  = { OPAQUE, baralpha, borderalpha },
};
static const unsigned int tagalpha[]     = { OPAQUE, baralpha };

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = "ﳺ";

/* focus */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

/* tagging */
static const char *tags[] = { tagsyms[0], tagsyms[1], tagsyms[2], tagsyms[3], tagsyms[4], tagsyms[5], tagsyms[6], tagsyms[7], tagsyms[8] };
static const char *tagsel[][2] = {
	{ tagcolors[0][0], tagcolors[0][1] },
	{ tagcolors[1][0], tagcolors[1][1] },
	{ tagcolors[2][0], tagcolors[2][1] },
	{ tagcolors[3][0], tagcolors[3][1] },
	{ tagcolors[4][0], tagcolors[4][1] },
	{ tagcolors[5][0], tagcolors[5][1] },
	{ tagcolors[6][0], tagcolors[6][1] },
	{ tagcolors[7][0], tagcolors[7][1] },
	{ tagcolors[8][0], tagcolors[8][1] },
};

/* layout(s) */
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


/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
  //  fonts
		{ "font",               STRING,  &font },
		{ "dmenufont",          STRING,  &dmenufont },
  //  colors
		{ "normbgcolor",        STRING,  &normbgcolor },
		{ "normbordercolor",    STRING,  &normbordercolor },
		{ "normfgcolor",        STRING,  &normfgcolor },
		{ "selbgcolor",         STRING,  &selbgcolor },
		{ "selbordercolor",     STRING,  &selbordercolor },
		{ "selfgcolor",         STRING,  &selfgcolor },
		{ "normmarkcolor",      STRING,  &normmarkcolor },
		{ "selmarkcolor",       STRING,  &selmarkcolor },
  // tag colors
		{ "tag1fg",             STRING,  &tagcolors[0][0] },
		{ "tag1bg",             STRING,  &tagcolors[0][1] },
		{ "tag2fg",             STRING,  &tagcolors[1][0] },
		{ "tag2bg",             STRING,  &tagcolors[1][1] },
		{ "tag3fg",             STRING,  &tagcolors[2][0] },
		{ "tag3bg",             STRING,  &tagcolors[2][1] },
		{ "tag4fg",             STRING,  &tagcolors[3][0] },
		{ "tag4bg",             STRING,  &tagcolors[3][1] },
		{ "tag5fg",             STRING,  &tagcolors[4][0] },
		{ "tag5bg",             STRING,  &tagcolors[4][1] },
		{ "tag6fg",             STRING,  &tagcolors[5][0] },
		{ "tag6bg",             STRING,  &tagcolors[5][1] },
		{ "tag7fg",             STRING,  &tagcolors[6][0] },
		{ "tag7bg",             STRING,  &tagcolors[6][1] },
		{ "tag8fg",             STRING,  &tagcolors[7][0] },
		{ "tag8bg",             STRING,  &tagcolors[7][1] },
		{ "tag9fg",             STRING,  &tagcolors[8][0] },
		{ "tag9bg",             STRING,  &tagcolors[8][1] },
  //  tag icon
		{ "tag1sym",            STRING,  &tagsyms[0] },
		{ "tag2sym",            STRING,  &tagsyms[1] },
		{ "tag3sym",            STRING,  &tagsyms[2] },
		{ "tag4sym",            STRING,  &tagsyms[3] },
		{ "tag5sym",            STRING,  &tagsyms[4] },
		{ "tag6sym",            STRING,  &tagsyms[5] },
		{ "tag7sym",            STRING,  &tagsyms[6] },
		{ "tag8sym",            STRING,  &tagsyms[7] },
		{ "tag9sym",            STRING,  &tagsyms[8] },
  //  misc
		{ "borderpx",          	INTEGER, &borderpx },
		{ "barheight",        	INTEGER, &user_bh },
		{ "topbar",            	INTEGER, &topbar },
		{ "showbar",          	INTEGER, &showbar },
		{ "vertpad",          	INTEGER, &vertpad },
		{ "sidepad",          	INTEGER, &sidepad },
		{ "snap",          		  INTEGER, &snap },
		{ "resizehints",       	INTEGER, &resizehints },
		{ "nmaster",          	INTEGER, &nmaster },
		{ "attachdirection",   	INTEGER, &attachdirection },
		{ "mfact",      	 	    FLOAT,   &mfact },
};


/*
 * RULES
 */
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor    float x,y,w,h  isfakefullscreen*/
//{ "Gimp",     NULL,       NULL,       0,            1,           -1,        50,50,500,500, 0 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1,        50,50,500,500, 0 },
};
static const MonitorRule monrules[] = {
	/* monitor  tag  layout  mfact  nmaster  showbar  topbar */
	//{  1,       -1,  2,      -1,    -1,      -1,      -1     }, // use a different layout for the second monitor
	{  -1,      -1,  0,      -1,    -1,      -1,      -1     } // default (keep it last)
};


/*
 * KEYBINDINGS
 */

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|Mod1Mask,              KEY,      combotag,       {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbordercolor, "-sf", selfgcolor, NULL };
static const char *termcmd[]  = { "tabbed_st", NULL };
static const char *filemanager[]  = { "thunar", NULL };
static const char *browser[]  = { "firefox", NULL };
static const char *layoutmenu_cmd = "layoutmenu.sh";

static const char *applaunchercmd[] = { "mcdmsc-applauncher", NULL };
static const char *powermenu[] = { "mcdmsc-powermenu", NULL };

static const char *volume[]  = { "mcdmsc-volume", NULL };
static const char *togglemute[]  = { "mcdmsc-volume", "tg", NULL };
static const char *volup[]  = { "mcdmsc-volume", "up", "5", NULL };
static const char *volminup[]  = { "mcdmsc-volume", "up", "1", NULL };
static const char *voldown[]  = { "mcdmsc-volume", "down", "5", NULL };
static const char *volmindown[]  = { "mcdmsc-volume", "down", "1", NULL };

static const char *lumup[]  = { "brightness.sh", "up", "5", NULL };
static const char *luminup[]  = { "brightness.sh", "up", "1", NULL };
static const char *lumdown[]  = { "brightness.sh", "down", "5", NULL };
static const char *lumindown[]  = { "brightness.sh", "down", "1", NULL };

/* commands spawned when clicking statusbar, the mouse button pressed is exported as BUTTON */
static const StatusCmd statuscmds[] = {
	{ "notify-send Mouse$BUTTON", 1 },
};
static const char *statuscmd[] = { "/bin/sh", "-c", NULL, NULL };

static Key keys[] = {
	/* modifier                      key        function        argument */
  // Layouts
	{ MODKEY,                        XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                        XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                        XK_f,      setlayout,      {.v = &layouts[9]} },
	{ MODKEY|ControlMask,            XK_period, cyclelayout,    {.i = +1 } },
	{ MODKEY|ControlMask,		         XK_comma,  cyclelayout,    {.i = -1 } },
  // mfact
	{ MODKEY,                        XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                        XK_l,      setmfact,       {.f = +0.05} },
  // cfact
	{ MODKEY|ShiftMask,              XK_h,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,              XK_l,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,              XK_o,      setcfact,       {.f =  0.00} },
  // nmaster
	{ MODKEY,                        XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                        XK_d,      incnmaster,     {.i = -1 } },

  // Gaps
	{ MODKEY|Mod1Mask,               XK_u,      incrgaps,       {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_u,      incrgaps,       {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_i,      incrigaps,      {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_i,      incrigaps,      {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_o,      incrogaps,      {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_o,      incrogaps,      {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_6,      incrihgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_6,      incrihgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_7,      incrivgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_7,      incrivgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_8,      incrohgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_8,      incrohgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_9,      incrovgaps,     {.i = +1 } },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_9,      incrovgaps,     {.i = -1 } },
	{ MODKEY|Mod1Mask,               XK_0,      togglegaps,     {0} },
	{ MODKEY|Mod1Mask|ShiftMask,     XK_0,      defaultgaps,    {0} },
	{ MODKEY,                        XK_b,      togglebar,      {0} },

  // Launchers
	{ MODKEY,                        XK_space,  spawn,          {.v = applaunchercmd } },
	{ MODKEY|ControlMask,            XK_space,  spawn,          {.v = dmenucmd } },
	{ MODKEY|ControlMask,            XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY|ControlMask,            XK_f,      spawn,          {.v = filemanager } },
	{ MODKEY|ControlMask,            XK_b,      spawn,          {.v = browser } },
  // Volume
	{ MODKEY|ControlMask,            XK_p,      spawn,          {.v = volume } },
	{ MODKEY|ControlMask,            XK_m,      spawn,          {.v = togglemute } },
	{ MODKEY|ControlMask,            XK_Up,     spawn,          {.v = volup } },
	{ MODKEY|ControlMask,            XK_Down,   spawn,          {.v = voldown } },
	{ MODKEY|ShiftMask|ControlMask,  XK_Up,     spawn,          {.v = volminup } },
	{ MODKEY|ShiftMask|ControlMask,  XK_Down,   spawn,          {.v = volmindown } },
  // Brightness
	{ MODKEY|Mod1Mask|ControlMask,   XK_Up,     spawn,          {.v = lumup } },
	{ MODKEY|Mod1Mask|ControlMask,   XK_Down,   spawn,          {.v = lumdown } },
	{ MODKEY|Mod1Mask|ShiftMask|ControlMask,XK_Up,    spawn,    {.v = luminup } },
	{ MODKEY|Mod1Mask|ShiftMask|ControlMask,XK_Down,  spawn,    {.v = lumindown } },


  // Focus
	{ MODKEY,                        XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                        XK_period, focusmon,       {.i = +1 } },
  { MODKEY,                        XK_u,      swapclient,     {0} },
	{ MODKEY|ShiftMask,              XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,              XK_period, tagmon,         {.i = +1 } },
  { MODKEY|ShiftMask,              XK_m,      togglemark,     {0} },
  { MODKEY|ShiftMask|ControlMask,  XK_o,      swapfocus,      {0} },

  // Windows
	{ MODKEY,                        XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                        XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                        XK_Return, zoom,           {0} },
	{ Mod1Mask,                      XK_Tab,    altTabStart,    {.i = +1 } },
	{ Mod1Mask|ShiftMask,            XK_Tab,    altTabStart,    {.i = -1 } },
	{ MODKEY|ControlMask,            XK_j,      pushdown,       {0} },
	{ MODKEY|ControlMask,            XK_k,      pushup,         {0} },
	{ MODKEY|ControlMask,            XK_c,      killclient,     {0} },
	{ MODKEY|Mod1Mask,               XK_k,      killclient,     {0} },
	{ MODKEY|ShiftMask,              XK_space,  togglefloating, {0} },

  // Tags
	{ MODKEY,                        XK_0,      view,           {.ui = ~0 } },
	{ MODKEY,                        XK_o,      winview,        {0} },
	{ MODKEY,                        XK_Right,  viewnext,       {0} },
	{ MODKEY,                        XK_Left,   viewprev,       {0} },
	{ MODKEY|ControlMask,            XK_l,      viewnext,       {0} },
	{ MODKEY|ControlMask,            XK_h,      viewprev,       {0} },
	{ MODKEY|ShiftMask,              XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY|Mod1Mask,               XK_Tab,    view,           {0} },
	{ MODKEY|Mod1Mask,               XK_Right,  tagtonext,      {0} },
	{ MODKEY|Mod1Mask,               XK_Left,   tagtoprev,      {0} },
	{ MODKEY|ControlMask|Mod1Mask,   XK_l,      tagtonext,      {0} },
	{ MODKEY|ControlMask|Mod1Mask,   XK_h,      tagtoprev,      {0} },
	TAGKEYS(                         XK_1,                      0)
	TAGKEYS(                         XK_2,                      1)
	TAGKEYS(                         XK_3,                      2)
	TAGKEYS(                         XK_4,                      3)
	TAGKEYS(                         XK_5,                      4)
	TAGKEYS(                         XK_6,                      5)
	TAGKEYS(                         XK_7,                      6)
	TAGKEYS(                         XK_8,                      7)
	TAGKEYS(                         XK_9,                      8)

  // Scratchpad
	{ MODKEY|ControlMask|ShiftMask,  XK_Tab,    scratchpad_show, {0} },
	{ MODKEY|ControlMask|ShiftMask,  XK_h,      scratchpad_hide, {0} },
	{ MODKEY|ControlMask|ShiftMask,  XK_c,      scratchpad_remove,{0} },

  // Quit
	{ MODKEY,                        XK_p,      spawn,          {.v = powermenu } },
	{ MODKEY|ShiftMask,              XK_q,      quit,           {1} }, 
	{ MODKEY|ControlMask|ShiftMask,  XK_q,      quit,           {0} },
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


/*
 * SIGNALS
 */

/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signum>"` */
static Signal signals[] = {
	/* signum       function        argument  */
	{ 1,            setlayout,      {.v = 0} },
};
