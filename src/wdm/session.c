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
 * session.c
 */

#include <dm.h>
#include <dm_auth.h>
#include <greet.h>

#include <X11/Xlib.h>
#include <signal.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Error.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <grp.h>				/* for initgroups */
#include <sys/types.h>

#include <wdmlib.h>

static int runAndWait(char **args, char **environ);

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#ifdef WITH_SELINUX
#include <selinux/get_context_list.h>
#include <selinux/selinux.h>
#endif

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#include <pwd.h>
#endif

#ifdef WITH_CONSOLE_KIT
#include <consolekit.h>
#endif

#include <pwd.h>
#include <unistd.h>

#ifdef USE_PAM
pam_handle_t **thepamhp()
{
	static pam_handle_t *pamh = NULL;
	return &pamh;
}

pam_handle_t *thepamh()
{
	pam_handle_t **pamhp;

	pamhp = thepamhp();
	if (pamhp)
		return *pamhp;
	else
		return NULL;
}

#ifdef HAVE_LIBAUDIT
void log_to_audit_system(int success)
{
	struct passwd *pw;
	char buf[64], *hostname = NULL, *tty = NULL, *login = NULL;
	int audit_fd;
	pam_handle_t *pamh = thepamh();

	if (!pamh) {
		WDMDebug("pamh == NULL\n");
		return;
	}
	audit_fd = audit_open();
	pam_get_item(pamh, PAM_RHOST, (const void **)&hostname);
	pam_get_item(pamh, PAM_TTY, (const void **)&tty);
	pam_get_item(pamh, PAM_USER, (const void **)&login);
	if (login)
		pw = getpwnam(login);
	else {
		login = "unknown";
		pw = NULL;
	}
	WDMDebug("audit_log_acct_message(%d, %d, %s, %s, %s, %d, %s, %s, %s, %d);\n",
			 audit_fd, AUDIT_USER_LOGIN, NULL, "wdm", login, pw ? pw->pw_uid : -1, hostname, NULL, tty, success);
	audit_log_acct_message(audit_fd, AUDIT_USER_LOGIN, NULL, "wdm", login, pw ? pw->pw_uid : -1, hostname, NULL, tty, success);
	close(audit_fd);
}
#endif
#endif

static Bool StartClient(struct verify_info *verify, struct display *d, int *pidp, char *name, char *passwd
#ifdef WITH_CONSOLE_KIT
						, char *ck_session_cookie
#endif
	);

static int clientPid;
static struct greet_info greet;
static struct verify_info verify;

static Jmp_buf abortSession;

static void catchTerm(int n)
{
	Longjmp(abortSession, 1);
}

static Jmp_buf pingTime;

static void catchAlrm(int n)
{
	Longjmp(pingTime, 1);
}

static Jmp_buf tenaciousClient;

static void waitAbort(int n)
{
	Longjmp(tenaciousClient, 1);
}

static void AbortClient(int pid)
{
	int sig = SIGTERM;
	volatile int i;
	int retId;

	for (i = 0; i < 4; i++) {
		if (killpg(pid, sig) == -1) {
			switch (errno) {
			case EPERM:
				WDMError("wdm can't kill client\n");
			case EINVAL:
			case ESRCH:
				return;
			}
		}
		if (!Setjmp(tenaciousClient)) {
			(void)Signal(SIGALRM, waitAbort);
			(void)alarm((unsigned)10);
			retId = wait((waitType *) 0);
			(void)alarm((unsigned)0);
			(void)Signal(SIGALRM, SIG_DFL);
			if (retId == pid)
				break;
		} else
			(void)Signal(SIGALRM, SIG_DFL);
		sig = SIGKILL;
	}
}

void SessionPingFailed(struct display *d)
{
	if (clientPid > 1) {
		AbortClient(clientPid);
		source(verify.systemEnviron, d->reset);
	}
	SessionExit(d, RESERVER_DISPLAY, TRUE);
}

/*
 * We need our own error handlers because we can't be sure what exit code Xlib
 * will use, and our Xlib does exit(1) which matches REMANAGE_DISPLAY, which
 * can cause a race condition leaving the display wedged.  We need to use
 * RESERVER_DISPLAY for IO errors, to ensure that the manager waits for the
 * server to terminate.  For other X errors, we should give up.
 */

static int IOErrorHandler(Display * dpy)
{
	WDMError("fatal IO error %d (%s)\n", errno, _SysErrorMsg(errno));
	exit(RESERVER_DISPLAY);
	 /*NOTREACHED*/ return 0;
}

static int ErrorHandler(Display * dpy, XErrorEvent * event)
{
	WDMError("X error\n");
	if (XmuPrintDefaultErrorMessage(dpy, event, stderr) == 0)
		return 0;
	exit(UNMANAGE_DISPLAY);
 /*NOTREACHED*/}

void ManageSession(struct display *d)
{
	static int pid = 0;
	Display *dpy;
	greet_user_rtn greet_stat;
#ifdef WITH_CONSOLE_KIT
	char *ck_session_cookie = NULL;
#endif

	WDMDebug("ManageSession %s\n", d->name);
	(void)XSetIOErrorHandler(IOErrorHandler);
	(void)XSetErrorHandler(ErrorHandler);
#ifndef HAS_SETPROCTITLE
	SetTitle(d->name, (char *)0);
#else
	setproctitle("%s", d->name);
#endif
	/*
	 * Load system default Resources
	 */
	LoadXloginResources(d);

	verify.version = 1;
	greet.version = 1;
	greet_stat = GreetUser(d, &dpy, &verify, &greet);

	if (greet_stat == Greet_Success) {
		clientPid = 0;
		if (!Setjmp(abortSession)) {
			(void)Signal(SIGTERM, catchTerm);
			/*
			 * Start the clients, changing uid/groups
			 *     setting up environment and running the session
			 */
#ifdef WITH_CONSOLE_KIT
			ck_session_cookie = open_ck_session(getpwnam(greet.name), d);
#endif
			if (StartClient(&verify, d, &clientPid, greet.name, greet.password
#ifdef WITH_CONSOLE_KIT
							, ck_session_cookie
#endif
				)) {
				WDMDebug("Client Started\n");

				/*
				 * Wait for session to end,
				 */
				for (;;) {
					if (d->pingInterval) {
						if (!Setjmp(pingTime)) {
							(void)Signal(SIGALRM, catchAlrm);
							(void)alarm(d->pingInterval * 60);
							pid = wait((waitType *) 0);
							(void)alarm(0);
						} else {
							(void)alarm(0);
							if (!PingServer(d, (Display *) NULL))
								SessionPingFailed(d);
						}
					} else {
						pid = wait((waitType *) 0);
					}
					if (pid == clientPid)
						break;
				}
			} else {
				WDMError("session start failed\n");
			}
		} else {
			/*
			 * when terminating the session, nuke
			 * the child and then run the reset script
			 */
			AbortClient(clientPid);
		}
	}
#ifdef WITH_CONSOLE_KIT
	if (ck_session_cookie != NULL) {
		close_ck_session(ck_session_cookie);
		free(ck_session_cookie);
	}
#endif

	/*
	 * run system-wide reset file
	 */
	WDMDebug("Source reset program %s\n", d->reset);
	source(verify.systemEnviron, d->reset);
	SessionExit(d, OBEYSESS_DISPLAY, TRUE);
}

void LoadXloginResources(struct display *d)
{
	char **args;
	char **env = 0;

	if (d->resources[0] && access(d->resources, 4) == 0) {
		env = systemEnv(d, (char *)0, (char *)0);
		args = parseArgs((char **)0, d->xrdb);
		args = parseArgs(args, d->resources);
		WDMDebug("Loading resource file: %s\n", d->resources);
		(void)runAndWait(args, env);
		freeArgs(args);
		WDMFreeEnv(env);
	}
}

void SetupDisplay(struct display *d)
{
	char **env = 0;

	if (d->setup && d->setup[0]) {
		env = systemEnv(d, (char *)0, (char *)0);
		(void)source(env, d->setup);
		WDMFreeEnv(env);
	}
}

void DeleteXloginResources(struct display *d, Display * dpy)
{
	int i;
	Atom prop = XInternAtom(dpy, "SCREEN_RESOURCES", True);

	XDeleteProperty(dpy, RootWindow(dpy, 0), XA_RESOURCE_MANAGER);
	if (prop) {
		for (i = ScreenCount(dpy); --i >= 0;)
			XDeleteProperty(dpy, RootWindow(dpy, i), prop);
	}
}

static Jmp_buf syncJump;

static void syncTimeout(int n)
{
	Longjmp(syncJump, 1);
}

void SecureDisplay(struct display *d, Display * dpy)
{
	WDMDebug("SecureDisplay %s\n", d->name);
	(void)Signal(SIGALRM, syncTimeout);
	if (Setjmp(syncJump)) {
		WDMError("WARNING: display %s could not be secured\n", d->name);
		SessionExit(d, RESERVER_DISPLAY, FALSE);
	}
	(void)alarm((unsigned)d->grabTimeout);
	WDMDebug("Before XGrabServer %s\n", d->name);
	XGrabServer(dpy);
	if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) {
		(void)alarm(0);
		(void)Signal(SIGALRM, SIG_DFL);
		WDMError("WARNING: keyboard on display %s could not be secured\n", d->name);
		SessionExit(d, RESERVER_DISPLAY, FALSE);
	}
	WDMDebug("XGrabKeyboard succeeded %s\n", d->name);
	(void)alarm(0);
	(void)Signal(SIGALRM, SIG_DFL);
	pseudoReset(dpy);
	if (!d->grabServer) {
		XUngrabServer(dpy);
		XSync(dpy, 0);
	}
	WDMDebug("done secure %s\n", d->name);
}

void UnsecureDisplay(struct display *d, Display * dpy)
{
	WDMDebug("Unsecure display %s\n", d->name);
	if (d->grabServer) {
		XUngrabServer(dpy);
		XSync(dpy, 0);
	}
}

void SessionExit(struct display *d, int status, int removeAuth)
{
#ifdef USE_PAM
	pam_handle_t *pamh = thepamh();
	if (pamh) {
		/* shutdown PAM session */
		if (pam_setcred(pamh, PAM_DELETE_CRED) != PAM_SUCCESS)
			WDMError("pam_setcred(DELETE_CRED) failed, errno=%d", errno);
		pam_close_session(pamh, 0);
		pam_end(pamh, PAM_SUCCESS);
		pamh = NULL;
	}
#endif

	/* make sure the server gets reset after the session is over */
	if (d->serverPid >= 2 && d->resetSignal)
		kill(d->serverPid, d->resetSignal);
	else
		ResetServer(d);
	if (removeAuth) {
		setgid(verify.gid);
		setuid(verify.uid);
		RemoveUserAuthorization(d, &verify);
	}
	WDMDebug("Display %s exiting with status %d\n", d->name, status);
	exit(status);
}

static Bool StartClient(struct verify_info *verify, struct display *d, int *pidp, char *name, char *passwd
#ifdef WITH_CONSOLE_KIT
						, char *ck_session_cookie
#endif
	)
{
	char **f;
	const char *home;
	char *failsafeArgv[2];
	int pid;
#ifdef HAS_SETUSERCONTEXT
	struct passwd *pwd;
#endif
#ifdef USE_PAM
	pam_handle_t *pamh = thepamh();
#endif

	if (verify->argv) {
		WDMDebug("StartSession %s: ", verify->argv[0]);
		for (f = verify->argv; *f; f++)
			WDMDebug("%s ", *f);
		WDMDebug("; ");
	}
	if (verify->userEnviron) {
		for (f = verify->userEnviron; *f; f++)
			WDMDebug("%s ", *f);
		WDMDebug("\n");
	}

	/* Do system-dependent login setup here */

	if (setgid(verify->gid) < 0) {
		WDMError("setgid %d (user \"%s\") failed, errno=%d\n", verify->gid, name, errno);
		return (0);
	}
#if defined(BSD) && (BSD >= 199103)
	if (setlogin(name) < 0) {
		WDMError("setlogin for \"%s\" failed, errno=%d", name, errno);
		return (0);
	}
#endif
	if (initgroups(name, verify->gid) < 0) {
		WDMError("initgroups for \"%s\" failed, errno=%d\n", name, errno);
		return (0);
	}
#ifdef USE_PAM
	if (pamh) {
		if (pam_setcred(pamh, PAM_ESTABLISH_CRED) != PAM_SUCCESS) {
			WDMError("pam_setcred failed, errno=%d\n", errno);
			log_to_audit_system(0);
			pam_end(pamh, PAM_SUCCESS);
			pamh = NULL;
			return 0;
		}

		pam_open_session(pamh, 0);
		log_to_audit_system(1);

		/* pass in environment variables set by libpam and modules it called */
		{
			long i;
			char **pam_env = pam_getenvlist(pamh);
			for (i = 0; pam_env && pam_env[i]; i++) {
				verify->userEnviron = WDMPutEnv(verify->userEnviron, pam_env[i]);
			}
		}
	}
#endif
	switch (pid = fork()) {
	case 0:
		CleanUpChild();
#ifdef XDMCP
		/* The chooser socket is not closed by CleanUpChild() */
		DestroyWellKnownSockets();
#endif

		if (setuid(verify->uid) < 0) {
			WDMError("setuid %d (user \"%s\") failed, errno=%d\n", verify->uid, name, errno);
			return (0);
		}

		/*
		 * for Security Enhanced Linux,
		 * set the default security context for this user.
		 */
#ifdef WITH_SELINUX
		if (is_selinux_enabled()) {
			security_context_t scontext;
			if (get_default_context(name, NULL, &scontext))
				WDMError("Failed to get default security context" " for %s.", name);
			WDMDebug("setting security context to %s", scontext);
			if (setexeccon(scontext)) {
				freecon(scontext);
				WDMError("Failed to set exec security context %s " "for %s.", scontext, name);
			}
			freecon(scontext);
		}
#endif
		/*
		 * for user-based authorization schemes,
		 * use the password to get the user's credentials.
		 */
		bzero(passwd, strlen(passwd));
#ifdef WITH_CONSOLE_KIT
		if (ck_session_cookie != NULL) {
			verify->userEnviron = WDMSetEnv(verify->userEnviron, "XDG_SESSION_COOKIE", ck_session_cookie);
		}
#endif
		SetUserAuthorization(d, verify);
		home = WDMGetEnv(verify->userEnviron, "HOME");
		if (home)
			if (chdir(home) == -1) {
				WDMError("user \"%s\": cannot chdir to home \"%s\" (err %d), using \"/\"\n",
						 WDMGetEnv(verify->userEnviron, "USER"), home, errno);
				chdir("/");
				verify->userEnviron = WDMSetEnv(verify->userEnviron, "HOME", "/");
			}
		if (verify->argv) {
			WDMDebug("executing session %s\n", verify->argv[0]);
			execute(verify->argv, verify->userEnviron);
			WDMError("Session \"%s\" execution failed (err %d)\n", verify->argv[0], errno);
		} else {
			WDMError("Session has no command/arguments\n");
		}
		failsafeArgv[0] = d->failsafeClient;
		failsafeArgv[1] = 0;
		execute(failsafeArgv, verify->userEnviron);
		exit(1);
	case -1:
		bzero(passwd, strlen(passwd));
		WDMDebug("StartSession, fork failed\n");
		WDMError("can't start session on \"%s\", fork failed, errno=%d\n", d->name, errno);
		return 0;
	default:
		bzero(passwd, strlen(passwd));
		WDMDebug("StartSession, fork succeeded %d\n", pid);
		*pidp = pid;
		return 1;
	}
}

int source(char **environ, char *file)
{
	char **args, *args_safe[2];
	int ret;

	if (file && file[0]) {
		WDMDebug("source %s\n", file);
		args = parseArgs((char **)0, file);
		if (!args) {
			args = args_safe;
			args[0] = file;
			args[1] = NULL;
		}
		ret = runAndWait(args, environ);
		freeArgs(args);
		return ret;
	}
	return 0;
}

static int runAndWait(char **args, char **environ)
{
	int pid;
	waitType result;

	switch (pid = fork()) {
	case 0:
		CleanUpChild();
		execute(args, environ);
		WDMError("can't execute \"%s\" (err %d)\n", args[0], errno);
		exit(1);
	case -1:
		WDMDebug("fork failed\n");
		WDMError("can't fork to execute \"%s\" (err %d)\n", args[0], errno);
		return 1;
	default:
		while (wait(&result) != pid)
			/* SUPPRESS 530 */
			;
		break;
	}
	return waitVal(result);
}

void execute(char **argv, char **environ)
{
	/* give /dev/null as stdin */
	(void)close(0);
	open("/dev/null", O_RDONLY);
	/* make stdout follow stderr to the log file */
	dup2(2, 1);
	execve(argv[0], argv, environ);
	/*
	 * In case this is a shell script which hasn't been
	 * made executable (or this is a SYSV box), do
	 * a reasonable thing
	 */
	if (errno != ENOENT) {
		char program[1024], *e, *p, *optarg;
		FILE *f;
		char **newargv, **av;
		int argc;

		/*
		 * emulate BSD kernel behaviour -- read
		 * the first line; check if it starts
		 * with "#!", in which case it uses
		 * the rest of the line as the name of
		 * program to run.  Else use "/bin/sh".
		 */
		f = fopen(argv[0], "r");
		if (!f)
			return;
		if (fgets(program, sizeof(program) - 1, f) == NULL) {
			fclose(f);
			return;
		}
		fclose(f);
		e = program + strlen(program) - 1;
		if (*e == '\n')
			*e = '\0';
		if (!strncmp(program, "#!", 2)) {
			p = program + 2;
			while (*p && isspace(*p))
				++p;
			optarg = p;
			while (*optarg && !isspace(*optarg))
				++optarg;
			if (*optarg) {
				*optarg = '\0';
				do
					++optarg;
				while (*optarg && isspace(*optarg));
			} else
				optarg = 0;
		} else {
			p = "/bin/sh";
			optarg = 0;
		}
		WDMDebug("Shell script execution: %s (optarg %s)\n", p, optarg ? optarg : "(null)");
		for (av = argv, argc = 0; *av; av++, argc++)
			/* SUPPRESS 530 */
			;
		newargv = (char **)malloc((argc + (optarg ? 3 : 2)) * sizeof(char *));
		if (!newargv)
			return;
		av = newargv;
		*av++ = p;
		if (optarg)
			*av++ = optarg;
		/* SUPPRESS 560 */
		while ((*av++ = *argv++))
			/* SUPPRESS 530 */
			;
		execve(newargv[0], newargv, environ);
	}
}

char **defaultEnv(void)
{
	char **env, **exp, *value;

	env = 0;
	for (exp = exportList.l; exp && *exp; ++exp) {
		value = getenv(*exp);
		if (value)
			env = WDMSetEnv(env, *exp, value);
	}
	return env;
}

char **systemEnv(struct display *d, char *user, char *home)
{
	char **env;

	env = defaultEnv();
	env = WDMSetEnv(env, "DISPLAY", d->name);
	if (home)
		env = WDMSetEnv(env, "HOME", home);
	if (user) {
		env = WDMSetEnv(env, "USER", user);
		env = WDMSetEnv(env, "LOGNAME", user);
	}
	env = WDMSetEnv(env, "PATH", d->systemPath);
	env = WDMSetEnv(env, "SHELL", d->systemShell);
	if (d->authFile)
		env = WDMSetEnv(env, "XAUTHORITY", d->authFile);
	return env;
}
