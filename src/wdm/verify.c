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
 * verify.c
 *
 * typical unix verification routine.
 */

#include	<dm.h>

#include	<pwd.h>
#ifdef USE_PAM
#include	<security/pam_appl.h>
#include	<stdlib.h>
#else
#ifdef HAVE_SHADOW_H
#include	<shadow.h>
#include	<errno.h>
#endif
#endif

#include	<greet.h>

#include <wdmlib.h>

static char *envvars[] = {
	"TZ",						/* SYSV and SVR4, but never hurts */
	NULL
};

#ifdef KERBEROS
#include <sys/param.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>
static char krbtkfile[MAXPATHLEN];
#endif

static char **userEnv(struct display *d, int useSystemPath, char *user, char *home, char *shell)
{
	char **env;
	char **envvar;
	char *str;

	env = defaultEnv();
	env = WDMSetEnv(env, "DISPLAY", d->name);
	env = WDMSetEnv(env, "HOME", home);
	env = WDMSetEnv(env, "LOGNAME", user);	/* POSIX, System V */
	env = WDMSetEnv(env, "USER", user);	/* BSD */
	env = WDMSetEnv(env, "PATH", useSystemPath ? d->systemPath : d->userPath);
	env = WDMSetEnv(env, "SHELL", shell);
#ifdef KERBEROS
	if (krbtkfile[0] != '\0')
		env = WDMSetEnv(env, "KRBTKFILE", krbtkfile);
#endif
	for (envvar = envvars; *envvar; envvar++) {
		str = getenv(*envvar);
		if (str)
			env = WDMSetEnv(env, *envvar, str);
	}
	return env;
}

#ifdef USE_PAM
static char *PAM_password;
static int pam_error;

static int PAM_conv(int num_msg,
					const struct pam_message **msg,
					struct pam_response **resp, void *appdata_ptr)
{
	int count = 0, replies = 0;
	struct pam_response *reply = NULL;

#define PAM_RESPONSE_SIZE	sizeof(struct pam_response)
	size_t size = PAM_RESPONSE_SIZE;

#define COPY_STRING(s) (s) ? strdup(s) : (char*)NULL

	for (count = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			/* user name given to PAM already */
			return PAM_CONV_ERR;
		case PAM_PROMPT_ECHO_OFF:
			/* wants password */
			if (reply) {
				reply = realloc(reply, size);
				bzero(reply + size - PAM_RESPONSE_SIZE, PAM_RESPONSE_SIZE);
			} else {
				reply = (struct pam_response *)malloc(size);
				bzero(reply, size);
			}

			if (!reply)
				return PAM_CONV_ERR;

			size += PAM_RESPONSE_SIZE;

			reply[replies].resp_retcode = PAM_SUCCESS;
			reply[replies].resp = COPY_STRING(PAM_password);
			/* PAM frees resp */
			break;
		case PAM_TEXT_INFO:
			/* ignore the informational mesage */
			break;
		default:
			/* unknown or PAM_ERROR_MSG */
			if (reply)
				free(reply);
			return PAM_CONV_ERR;
		}
	}

#undef COPY_STRING
	if (reply)
		*resp = reply;
	return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation = {
	PAM_conv,
	NULL
};
#endif							/* USE_PAM */

#ifdef USE_BSDAUTH
int Verify(struct display *d, struct greet_info *greet, struct verify_info *verify)
{
	struct passwd *p;
	login_cap_t *lc;
	auth_session_t *as;
	char *style, *shell, *home, *s, **argv;
	char path[MAXPATHLEN];
	int authok;

	/* User may have specified an authentication style. */
	if ((style = strchr(greet->name, ':')) != NULL)
		*style++ = '\0';

	Debug("Verify %s, style %s ...\n", greet->name, style ? style : "default");

	p = getpwnam(greet->name);
	endpwent();

	if (!p || strlen(greet->name) == 0) {
		Debug("getpwnam() failed.\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}

	if ((lc = login_getclass(p->pw_class)) == NULL) {
		Debug("login_getclass() failed.\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}
	if ((style = login_getstyle(lc, style, "wdm")) == NULL) {
		Debug("login_getstyle() failed.\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}
	if ((as = auth_open()) == NULL) {
		Debug("auth_open() failed.\n");
		login_close(lc);
		bzero(greet->password, strlen(greet->password));
		return 0;
	}
	if (auth_setoption(as, "login", "yes") == -1) {
		Debug("auth_setoption() failed.\n");
		login_close(lc);
		bzero(greet->password, strlen(greet->password));
		return 0;
	}

	/* Set up state for no challenge, just check a response. */
	auth_setstate(as, 0);
	auth_setdata(as, "", 1);
	auth_setdata(as, greet->password, strlen(greet->password) + 1);

	/* Build path of the auth script and call it */
	snprintf(path, sizeof(path), _PATH_AUTHPROG "%s", style);
	auth_call(as, path, style, "-s", "response", greet->name, NULL);
	authok = auth_getstate(as);

	if ((authok & AUTH_ALLOW) == 0) {
		Debug("password verify failed\n");
		bzero(greet->password, strlen(greet->password));
		auth_close(as);
		login_close(lc);
		return 0;
	}
	/* Run the approval script */
	if (!auth_approval(as, lc, greet->name, "auth-wdm")) {
		Debug("login not approved\n");
		bzero(greet->password, strlen(greet->password));
		auth_close(as);
		login_close(lc);
		return 0;
	}
	auth_close(as);
	login_close(lc);
	/* Check empty passwords against allowNullPasswd */
	if (!greet->allow_null_passwd && strlen(greet->password) == 0) {
		Debug("empty password not allowed\n");
		return 0;
	}
	/* Only accept root logins if allowRootLogin resource is set */
	if (p->pw_uid == 0 && !greet->allow_root_login) {
		Debug("root logins not allowed\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}

	/*
	 * Shell must be in /etc/shells 
	 */
	for (;;) {
		s = getusershell();
		if (s == NULL) {
			/* did not found the shell in /etc/shells 
			   -> failure */
			Debug("shell not in /etc/shells\n");
			bzero(greet->password, strlen(greet->password));
			endusershell();
			return 0;
		}
		if (strcmp(s, p->pw_shell) == 0) {
			/* found the shell in /etc/shells */
			endusershell();
			break;
		}
	}
#else							/* !USE_BSDAUTH */
int Verify(struct display *d, struct greet_info *greet, struct verify_info *verify)
{
	struct passwd *p;
#ifdef USE_PAM
	pam_handle_t **pamhp = thepamhp();
#else
#ifdef HAVE_SHADOW_H
	struct spwd *sp;
#endif
	char *user_pass = NULL;
#endif
#ifdef __OpenBSD__
	char *s;
	struct timeval tp;
#endif
	char *shell, *home;
	char **argv;

	WDMDebug("Verify %s ...\n", greet->name);
#ifndef USE_PAM
	p = getpwnam(greet->name);
	endpwent();

	if (!p || strlen(greet->name) == 0) {
		WDMDebug("getpwnam() failed.\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	} else {
#ifdef linux
		if (!strcmp(p->pw_passwd, "!") || !strcmp(p->pw_passwd, "*")) {
			WDMDebug("The account is locked, no login allowed.\n");
			bzero(greet->password, strlen(greet->password));
			return 0;
		}
#endif
		user_pass = p->pw_passwd;
	}
#endif
#ifdef KERBEROS
	if (strcmp(greet->name, "root") != 0) {
		char name[ANAME_SZ];
		char realm[REALM_SZ];
		char *q;
		int ret;

		if (krb_get_lrealm(realm, 1)) {
			WDMDebug("Can't get Kerberos realm.\n");
		} else {

			sprintf(krbtkfile, "%s.%s", TKT_ROOT, d->name);
			krb_set_tkt_string(krbtkfile);
			unlink(krbtkfile);

			ret = krb_verify_user(greet->name, "", realm, greet->password, 1, "rcmd");

			if (ret == KSUCCESS) {
				chown(krbtkfile, p->pw_uid, p->pw_gid);
				WDMDebug("kerberos verify succeeded\n");
				if (k_hasafs()) {
					if (k_setpag() == -1)
						WDMError("setpag() failed for %s\n", greet->name);

					if ((ret = k_afsklog(NULL, NULL)) != KSUCCESS)
						WDMError("Warning %s\n", krb_get_err_text(ret));
				}
				goto done;
			} else if (ret != KDC_PR_UNKNOWN && ret != SKDC_CANT) {
				/* failure */
				WDMDebug("kerberos verify failure %d\n", ret);
				krbtkfile[0] = '\0';
			}
		}
	}
#endif
#ifndef USE_PAM
#ifdef HAVE_SHADOW_H
	errno = 0;
	sp = getspnam(greet->name);
	if (sp == NULL) {
		WDMDebug("getspnam() failed, errno=%d.  Are you root?\n", errno);
	} else {
		user_pass = sp->sp_pwdp;
	}
#endif
	if (strcmp(crypt(greet->password, user_pass), user_pass))
	{
		if (!greet->allow_null_passwd || strlen(p->pw_passwd) > 0) {
			WDMDebug("password verify failed\n");
			bzero(greet->password, strlen(greet->password));
			return 0;
		}						/* else: null passwd okay */
	}
#ifdef KERBEROS
 done:
#endif
#ifdef __OpenBSD__
	/*
	 * Only accept root logins if allowRootLogin resource is set
	 */
	if ((p->pw_uid == 0) && !greet->allow_root_login) {
		WDMDebug("root logins not allowed\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}
	/*
	 * Shell must be in /etc/shells 
	 */
	for (;;) {
		s = getusershell();
		if (s == NULL) {
			/* did not found the shell in /etc/shells 
			   -> failure */
			WDMDebug("shell not in /etc/shells\n");
			bzero(greet->password, strlen(greet->password));
			endusershell();
			return 0;
		}
		if (strcmp(s, p->pw_shell) == 0) {
			/* found the shell in /etc/shells */
			endusershell();
			break;
		}
	}
	/*
	 * Test for expired password
	 */
	if (p->pw_change || p->pw_expire)
		(void)gettimeofday(&tp, (struct timezone *)NULL);
	if (p->pw_change) {
		if (tp.tv_sec >= p->pw_change) {
			WDMDebug("Password has expired.\n");
			bzero(greet->password, strlen(greet->password));
			return 0;
		}
	}
	if (p->pw_expire) {
		if (tp.tv_sec >= p->pw_expire) {
			WDMDebug("account has expired.\n");
			bzero(greet->password, strlen(greet->password));
			return 0;
		}
	}
#endif							/* __OpenBSD__ */
	bzero(user_pass, strlen(user_pass));	/* in case shadow password */

#else							/* USE_PAM */
#define PAM_BAIL	\
	if (pam_error != PAM_SUCCESS) goto pam_failed;

	PAM_password = greet->password;
	pam_error = pam_start("wdm", greet->name, &PAM_conversation, pamhp);
	PAM_BAIL;
	pam_error = pam_set_item(*pamhp, PAM_TTY, d->name);
	PAM_BAIL;
	if (d->name[0] != ':') {	/* Displaying to remote host */
		char *hostname = strdup(d->name);

		if (hostname == NULL) {
			WDMError("Out of memory!\n");
		} else {
			char *colon = strrchr(hostname, ':');

			if (colon != NULL)
				*colon = '\0';

			pam_error = pam_set_item(*pamhp, PAM_RHOST, hostname);
			free(hostname);
		}
	} else
		pam_error = pam_set_item(*pamhp, PAM_RHOST, "localhost");
	PAM_BAIL;
	pam_error = pam_authenticate(*pamhp, 0);
	PAM_BAIL;
	pam_error = pam_acct_mgmt(*pamhp, 0);
	/* really should do password changing, but it doesn't fit well */
	PAM_BAIL;
	p = getpwnam(greet->name);
	endpwent();

	if (!p || strlen(greet->name) == 0) {
		WDMDebug("getpwnam() failed.\n");
		bzero(greet->password, strlen(greet->password));
		return 0;
	}

	if (pam_error != PAM_SUCCESS) {
 pam_failed:
		log_to_audit_system(0);
		pam_end(*pamhp, PAM_SUCCESS);
		*pamhp = NULL;
		return 0;
	}
#undef PAM_BAIL
#endif							/* USE_PAM */
#endif							/* USE_BSDAUTH */

	WDMDebug("verify succeeded\n");
	/* The password is passed to StartClient() for use by user-based
	   authorization schemes.  It is zeroed there. */
	verify->uid = p->pw_uid;
	verify->gid = p->pw_gid;
	home = p->pw_dir;
	shell = p->pw_shell;
	argv = 0;
	if (d->session)
		argv = parseArgs(argv, d->session);
	if (greet->string)
		argv = parseArgs(argv, greet->string);
	if (!argv)
		argv = parseArgs(argv, "xsession");
	verify->argv = argv;
	verify->userEnviron = userEnv(d, p->pw_uid == 0, greet->name, home, shell);
	WDMDebug("user environment:\n");
	WDMPrintEnv(verify->userEnviron);
	verify->systemEnviron = systemEnv(d, greet->name, home);
	WDMDebug("system environment:\n");
	WDMPrintEnv(verify->systemEnviron);
	WDMDebug("end of environments\n");
	return 1;
}
