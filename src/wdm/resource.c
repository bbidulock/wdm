/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/*
 * wdm - WINGs Display Manager
 * Author:  Keith Packard, MIT X Consortium
 *
 * resource.c
 */

#include <dm.h>

#include <wdmlib.h>

#include <X11/Intrinsic.h>
#include <X11/Xmu/CharSet.h>

char *config;

char *servers;
int_resource request_port;
int_resource debugLevel;
char *errorLogFile;
char *syslogFacility;
int_resource useSyslog;
int_resource daemonMode;
char *pidFile;
int_resource lockPidFile;
int_resource sourceAddress;
char *authDir;
int_resource autoRescan;
int_resource removeDomainname;
char *keyFile;
char *accessFile;
char *accessFilePL;
slist_resource exportList;
char *randomFile;
char *greeterLib;
char *willing;
int_resource choiceTimeout;		/* chooser choice timeout */

/* wdm additions */
#define DEF_WDMLOGIN "/usr/X11R6/bin/wdmLogin"
char *wdmLogin;					/* full path to external program Login */
char *wdmWm;					/* list of optional window managers to start */
char *wdmLogo;					/* points to optional Logo pixmap */
char *wdmHelpFile;				/* points to optional help file */
char *wdmDefaultUser;			/* points to optional default user name */
char *wdmDefaultPasswd;			/* points to optional default passwd */
char *wdmBg;					/* pixmap or color(s) for background */
char *wdmReboot;				/* command for Reboot */
char *wdmHalt;					/* command to Halt */
int_resource wdmVerify;			/* if true, require name & password for Exit */
				/* reboot or halt */
int_resource wdmRoot;			/* if true only username=root and verified */
				/* password can do Exit, reboot or halt */
int_resource wdmAnimations;		/* if true, enable shake and rollup */
				/* animations */
				/* if false, disable animations */
char *wdmLocale;				/* this will be LANG value before starting */
				/* wdmLogin */
char *wdmLoginConfig;			/* this will be passed to wdmLogin with */
				/* -c option */
char *wdmCursorTheme;			/* this will be XCURSOR_THEME value before */
				/* starting wdmLogin */
int_resource wdmXineramaHead;	/* select xinerama head where to show login */
				/* panel this _should_ be display dependant, */
				/* but I make it later */
int_resource wdmSequentialXServerLaunch;	/* if true, launch multiple X servers */
				/* sequentially, (slower. but safer. see */
				/* http://www.altlinux.org/X11/DualSeat) */
				/* otherwise in parallel (default) */

#define DM_STRING	0
#define DM_INT		1
#define DM_BOOL	2
#define DM_ARGV	3

/*
 * the following constants are supposed to be set in the makefile from
 * parameters set util/imake.includes/site.def (or *.macros in that directory
 * if it is server-specific).  DO NOT CHANGE THESE DEFINITIONS!
 */
#ifndef DEF_SERVER_LINE
#define DEF_SERVER_LINE ":0 local /usr/bin/X11/X :0"
#endif
#ifndef XRDB_PROGRAM
#define XRDB_PROGRAM "/usr/bin/X11/xrdb"
#endif
#ifndef DEF_SESSION
#define DEF_SESSION "/usr/bin/X11/xterm -ls"
#endif
#ifndef DEF_USER_PATH
#define DEF_USER_PATH ":/bin:/usr/bin:/usr/bin/X11:/usr/ucb"
#endif
#ifndef DEF_SYSTEM_PATH
#define DEF_SYSTEM_PATH "/etc:/bin:/usr/bin:/usr/bin/X11:/usr/ucb"
#endif
#ifndef DEF_SYSTEM_SHELL
#define DEF_SYSTEM_SHELL "/bin/sh"
#endif
#ifndef DEF_FAILSAFE_CLIENT
#define DEF_FAILSAFE_CLIENT "/usr/bin/X11/xterm"
#endif
#ifndef DEF_WDM_CONFIG
#define DEF_WDM_CONFIG "/usr/lib/X11/wdm/wdm-config"
#endif
#ifndef DEF_CHOOSER
#define DEF_CHOOSER "/usr/lib/X11/wdm/chooser"
#endif
#ifndef DEF_AUTH_NAME
#define DEF_AUTH_NAME	"MIT-MAGIC-COOKIE-1"
#endif
#ifndef DEF_AUTH_DIR
#define DEF_AUTH_DIR "/usr/lib/X11/wdm"
#endif
#ifndef DEF_USER_AUTH_DIR
#define DEF_USER_AUTH_DIR	"/tmp"
#endif
#ifndef DEF_KEY_FILE
#define DEF_KEY_FILE	""
#endif
#ifndef DEF_ACCESS_FILE
#define DEF_ACCESS_FILE	""
#endif
#ifndef DEF_ACCESS_FILE_PL
#define DEF_ACCESS_FILE_PL	""
#endif
#ifndef DEF_RANDOM_FILE
#define DEF_RANDOM_FILE "/dev/mem"
#endif
#ifndef DEF_GREETER_LIB
#define DEF_GREETER_LIB "/X11/lib/X11/wdm/libWdmGreet.so"
#endif

#define DEF_UDP_PORT	"177"	/* registered XDMCP port, dont change */

struct dmResources {
	char *name, *class;
	int type;
	char **dm_value;
	char *default_value;
} DmResources[] = {
	{
	"servers", "Servers", DM_STRING, &servers, DEF_SERVER_LINE}, {
	"requestPort", "RequestPort", DM_INT, &request_port.c, DEF_UDP_PORT}, {
	"debugLevel", "DebugLevel", DM_INT, &debugLevel.c, "1"}, {
	"errorLogFile", "ErrorLogFile", DM_STRING, &errorLogFile, ""}, {
	"syslogFacility", "SyslogFacility", DM_STRING, &syslogFacility, ""}, {
	"useSyslog", "UseSyslog", DM_BOOL, &useSyslog.c, "false"}, {
	"daemonMode", "DaemonMode", DM_BOOL, &daemonMode.c, "true"}, {
	"pidFile", "PidFile", DM_STRING, &pidFile, ""}, {
	"lockPidFile", "LockPidFile", DM_BOOL, &lockPidFile.c, "true"}, {
	"authDir", "authDir", DM_STRING, &authDir, DEF_AUTH_DIR}, {
	"autoRescan", "AutoRescan", DM_BOOL, &autoRescan.c, "true"}, {
	"removeDomainname", "RemoveDomainname", DM_BOOL, &removeDomainname.c, "true"}, {
	"keyFile", "KeyFile", DM_STRING, &keyFile, DEF_KEY_FILE}, {
	"accessFile", "AccessFile", DM_STRING, &accessFile, DEF_ACCESS_FILE}, {
	"accessFilePL", "AccessFilePL", DM_STRING, &accessFilePL, DEF_ACCESS_FILE_PL}, {
	"exportList", "ExportList", DM_ARGV, &exportList.c, ""}, {
	"randomFile", "RandomFile", DM_STRING, &randomFile, DEF_RANDOM_FILE}, {
	"greeterLib", "GreeterLib", DM_STRING, &greeterLib, DEF_GREETER_LIB}, {
	"choiceTimeout", "ChoiceTimeout", DM_INT, &choiceTimeout.c, "15"}, {
	"sourceAddress", "SourceAddress", DM_BOOL, &sourceAddress.c, "false"}, {
	"willing", "Willing", DM_STRING, &willing, ""}, {
	"wdmLogin", "WdmLogin", DM_STRING, &wdmLogin, DEF_WDMLOGIN}, {
	"wdmReboot", "WdmReboot", DM_STRING, &wdmReboot, "reboot"}, {
	"wdmHalt", "WdmHalt", DM_STRING, &wdmHalt, "halt"}, {
	"wdmVerify", "WdmVerify", DM_BOOL, &wdmVerify.c, "false"}, {
"wdmRoot", "WdmRoot", DM_BOOL, &wdmRoot.c, "false"},};

#define NUM_DM_RESOURCES	(sizeof DmResources / sizeof DmResources[0])

#define boffset(f)	XtOffsetOf(struct display, f)

struct displayResource {
	char *name, *class;
	int type;
	int offset;
	char *default_value;
};

/* resources for managing the server */

struct displayResource serverResources[] = {
	{"serverAttempts", "ServerAttempts", DM_INT, boffset(serverAttempts),
	 "1"},
	{"openDelay", "OpenDelay", DM_INT, boffset(openDelay),
	 "15"},
	{"openRepeat", "OpenRepeat", DM_INT, boffset(openRepeat),
	 "5"},
	{"openTimeout", "OpenTimeout", DM_INT, boffset(openTimeout),
	 "120"},
	{"startAttempts", "StartAttempts", DM_INT, boffset(startAttempts),
	 "4"},
	{"pingInterval", "PingInterval", DM_INT, boffset(pingInterval),
	 "5"},
	{"pingTimeout", "PingTimeout", DM_INT, boffset(pingTimeout),
	 "5"},
	{"terminateServer", "TerminateServer", DM_BOOL, boffset(terminateServer),
	 "false"},
	{"grabServer", "GrabServer", DM_BOOL, boffset(grabServer),
	 "false"},
	{"grabTimeout", "GrabTimeout", DM_INT, boffset(grabTimeout),
	 "3"},
	{"resetSignal", "Signal", DM_INT, boffset(resetSignal),
	 "1"},						/* SIGHUP */
	{"termSignal", "Signal", DM_INT, boffset(termSignal),
	 "15"},						/* SIGTERM */
	{"resetForAuth", "ResetForAuth", DM_BOOL, boffset(resetForAuth),
	 "false"},
	{"authorize", "Authorize", DM_BOOL, boffset(authorize),
	 "true"},
	{"authComplain", "AuthComplain", DM_BOOL, boffset(authComplain),
	 "true"},
	{"authName", "AuthName", DM_ARGV, boffset(authNames),
	 DEF_AUTH_NAME},
	{"authFile", "AuthFile", DM_STRING, boffset(clientAuthFile),
	 ""},
};

#define NUM_SERVER_RESOURCES	(sizeof serverResources/\
				 sizeof serverResources[0])

/* resources which control the session behaviour */

struct displayResource sessionResources[] = {
	{"resources", "Resources", DM_STRING, boffset(resources),
	 ""},
	{"xrdb", "Xrdb", DM_STRING, boffset(xrdb),
	 XRDB_PROGRAM},
	{"setup", "Setup", DM_STRING, boffset(setup),
	 ""},
	{"startup", "Startup", DM_STRING, boffset(startup),
	 ""},
	{"reset", "Reset", DM_STRING, boffset(reset),
	 ""},
	{"session", "Session", DM_STRING, boffset(session),
	 DEF_SESSION},
	{"userPath", "Path", DM_STRING, boffset(userPath),
	 DEF_USER_PATH},
	{"systemPath", "Path", DM_STRING, boffset(systemPath),
	 DEF_SYSTEM_PATH},
	{"systemShell", "Shell", DM_STRING, boffset(systemShell),
	 DEF_SYSTEM_SHELL},
	{"failsafeClient", "FailsafeClient", DM_STRING, boffset(failsafeClient),
	 DEF_FAILSAFE_CLIENT},
	{"userAuthDir", "UserAuthDir", DM_STRING, boffset(userAuthDir),
	 DEF_USER_AUTH_DIR},
	{"chooser", "Chooser", DM_STRING, boffset(chooser),
	 DEF_CHOOSER},
};

#define NUM_SESSION_RESOURCES	(sizeof sessionResources/\
				 sizeof sessionResources[0])

struct dmResources wdmResources[] = {
	{"wdmWm", "WdmWm", DM_STRING, &wdmWm,
	 ""},
	{"wdmLogo", "WdmLogo", DM_STRING, &wdmLogo,
	 ""},
	{"wdmHelpFile", "WdmHelpFile", DM_STRING, &wdmHelpFile,
	 ""},
	{"wdmBg", "WdmBg", DM_STRING, &wdmBg,
	 ""},
	{"wdmDefaultUser", "WdmDefaultUser", DM_STRING, &wdmDefaultUser,
	 ""},
	{"wdmDefaultPasswd", "WdmDefaultPasswd", DM_STRING, &wdmDefaultPasswd,
	 ""},
	{"wdmAnimations", "WdmAnimations", DM_BOOL, &wdmAnimations.c,
	 "true"},
	{"wdmLocale", "WdmLocale", DM_STRING, &wdmLocale,
	 ""},
	{"wdmLoginConfig", "WdmLoginConfig", DM_STRING, &wdmLoginConfig,
	 DEF_WDMLOGIN_CONFIG},
	{"wdmCursorTheme", "WdmCursorTheme", DM_STRING, &wdmCursorTheme,
	 ""},
	{"wdmXineramaHead", "WdmXineramaHead", DM_INT, &wdmXineramaHead.c,
	 "0"},
	{"wdmSequentialXServerLaunch", "WdmSequentialXServerLaunch", DM_BOOL, &wdmSequentialXServerLaunch.c,
	 "false"},
};

#define NUM_WDM_RESOURCES	(sizeof wdmResources/\
				 sizeof wdmResources[0])

XrmDatabase DmResourceDB;

static void GetResource(char *name, char *class, int valueType, char **valuep, char *default_value)
{
	char *type;
	XrmValue value;
	char *string, *new_string;
	char str_buf[50];
	int len;

	if (DmResourceDB && XrmGetResource(DmResourceDB, name, class, &type, &value)) {
		string = value.addr;
		len = value.size;
	} else {
		string = default_value;
		len = strlen(string);
	}

	WDMDebug("%s/%s value %*.*s\n", name, class, len, len, string);

	if (valueType == DM_STRING && *valuep) {
		if (strlen(*valuep) == len && !strncmp(*valuep, string, len))
			return;
		else
			free(*valuep);
	}

	switch (valueType) {
	case DM_STRING:
		new_string = malloc((unsigned)(len + 1));
		if (!new_string) {
			WDMError("GetResource: out of memory");
			return;
		}
		strncpy(new_string, string, len);
		new_string[len] = '\0';
		*(valuep) = new_string;
		break;
	case DM_INT:
		strncpy(str_buf, string, sizeof(str_buf));
		str_buf[sizeof(str_buf) - 1] = '\0';
		*((int *)valuep) = atoi(str_buf);
		break;
	case DM_BOOL:
		strncpy(str_buf, string, sizeof(str_buf));
		str_buf[sizeof(str_buf) - 1] = '\0';
		XmuCopyISOLatin1Lowered(str_buf, str_buf);
		if (!strcmp(str_buf, "true") || !strcmp(str_buf, "on") || !strcmp(str_buf, "yes"))
			*((int *)valuep) = 1;
		else if (!strcmp(str_buf, "false") || !strcmp(str_buf, "off") || !strcmp(str_buf, "no"))
			*((int *)valuep) = 0;
		break;
	case DM_ARGV:
		freeArgs(*(char ***)valuep);
		*((char ***)valuep) = parseArgs((char **)0, string);
		break;
	}
}

XrmOptionDescRec configTable[] = {
	{"-server", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-udpPort", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-error", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-resources", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-session", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-debug", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-xrm", NULL, XrmoptionSkipArg, (caddr_t) NULL}
	,
	{"-config", ".configFile", XrmoptionSepArg, (caddr_t) NULL}
};

XrmOptionDescRec optionTable[] = {
	{"-server", ".servers", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-udpPort", ".requestPort", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-error", ".errorLogFile", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-resources", "*resources", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-session", "*session", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-debug", "*debugLevel", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-xrm", NULL, XrmoptionResArg, (caddr_t) NULL}
	,
	{"-daemon", ".daemonMode", XrmoptionNoArg, "true"}
	,
	{"-nodaemon", ".daemonMode", XrmoptionNoArg, "false"}
	,
	{"-syslog", ".syslogFacility", XrmoptionSepArg, (caddr_t) NULL}
	,
	{"-usesyslog", ".useSyslog", XrmoptionNoArg, "true"}
	,
	{"-useerrfile", ".useSyslog", XrmoptionNoArg, "false"}
};

static int originalArgc;
static char **originalArgv;

void InitResources(int argc, char **argv)
{
	XrmInitialize();
	originalArgc = argc;
	originalArgv = argv;
	ReinitResources();
}

void ReinitResources(void)
{
	int argc;
	char **a;
	char **argv;
	XrmDatabase newDB;

	argv = (char **)malloc((originalArgc + 1) * sizeof(char *));
	if (!argv)
		WDMError("no space for argument realloc\n");
	for (argc = 0; argc < originalArgc; argc++)
		argv[argc] = originalArgv[argc];
	argv[argc] = 0;
	if (DmResourceDB)
		XrmDestroyDatabase(DmResourceDB);
	DmResourceDB = XrmGetStringDatabase("");
	/* pre-parse the command line to get the -config option, if any */
	XrmParseCommand(&DmResourceDB, configTable, sizeof(configTable) / sizeof(configTable[0]), "DisplayManager", &argc, argv);
	GetResource("DisplayManager.configFile", "DisplayManager.ConfigFile", DM_STRING, &config, DEF_WDM_CONFIG);
	newDB = XrmGetFileDatabase(config);
	if (newDB) {
		if (DmResourceDB)
			XrmDestroyDatabase(DmResourceDB);
		DmResourceDB = newDB;
	} else if (argc != originalArgc)
		WDMError("Can't open configuration file %s\n", config);
	XrmParseCommand(&DmResourceDB, optionTable, sizeof(optionTable) / sizeof(optionTable[0]), "DisplayManager", &argc, argv);
	if (argc > 1) {
		WDMError("extra arguments on command line:");
		for (a = argv + 1; *a; a++)
			WDMError(" \"%s\"", *a);
		WDMError("\n");
	}
	free(argv);
}

void LoadDMResources(void)
{
	int i;
	char name[1024], class[1024];

	for (i = 0; i < NUM_DM_RESOURCES; i++) {
		sprintf(name, "DisplayManager.%s", DmResources[i].name);
		sprintf(class, "DisplayManager.%s", DmResources[i].class);
		GetResource(name, class, DmResources[i].type, (char **)DmResources[i].dm_value, DmResources[i].default_value);
	}
}

static void CleanUpName(char *src, char *dst, int len)
{
	while (*src) {
		if (--len <= 0)
			break;
		switch (*src) {
		case ':':
		case '.':
			*dst++ = '_';
			break;
		default:
			*dst++ = *src;
		}
		++src;
	}
	*dst = '\0';
}

static void LoadDisplayResources(struct display *d, struct displayResource *resources, int numResources)
{
	int i;
	char name[1024], class[1024];
	char dpyName[512], dpyClass[512];

	CleanUpName(d->name, dpyName, sizeof(dpyName));
	CleanUpName(d->class ? d->class : d->name, dpyClass, sizeof(dpyClass));
	for (i = 0; i < numResources; i++) {
		sprintf(name, "DisplayManager.%s.%s", dpyName, resources[i].name);
		sprintf(class, "DisplayManager.%s.%s", dpyClass, resources[i].class);
		GetResource(name, class, resources[i].type, (char **)(((char *)d) + resources[i].offset), resources[i].default_value);
	}
}

static void LoadWdmResources(struct display *d)
{
	int i;
	char name[1024], class[1024];
	char dpyName[512], dpyClass[512];

	CleanUpName(d->name, dpyName, sizeof(dpyName));
	CleanUpName(d->class ? d->class : d->name, dpyClass, sizeof(dpyClass));
	for (i = 0; i < NUM_WDM_RESOURCES; i++) {
		sprintf(name, "DisplayManager.%s.%s", dpyName, wdmResources[i].name);
		sprintf(class, "DisplayManager.%s.%s", dpyClass, wdmResources[i].class);
		GetResource(name, class, wdmResources[i].type, (char **)wdmResources[i].dm_value, wdmResources[i].default_value);
	}
}

void LoadServerResources(struct display *d)
{
	LoadDisplayResources(d, serverResources, NUM_SERVER_RESOURCES);
	LoadWdmResources(d);
}

void LoadSessionResources(struct display *d)
{
	LoadDisplayResources(d, sessionResources, NUM_SESSION_RESOURCES);
}
