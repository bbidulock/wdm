dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.53)
AC_INIT([wdm],[1.20.1],[voins@voins.program.ru])
AC_CONFIG_SRCDIR(install-sh)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_INSTALL
AC_PROG_CC


AC_ARG_VAR(XRDB_PATH, [Path to xrdb program from X])
AC_PATH_PROG(XRDB_PATH, xrdb)
AC_DEFINE_UNQUOTED(XRDB_PROGRAM, "$XRDB_PATH", [Value is a path for xrdb program])
AC_SUBST(XRDB_PATH)


AC_ARG_VAR(XCONSOLE, [Path to xconsole program])
AC_PATH_PROG(XCONSOLE, xconsole)
AC_SUBST(XCONSOLE)


AC_ARG_VAR(SHUTDOWN, [Path to shutdown program])
AC_PATH_PROG(SHUTDOWN,shutdown,,$PATH:/sbin:/usr/sbin)
AC_SUBST(SHUTDOWN)


AC_ARG_VAR(FAILSAFE_PATH, [Path to program, that is used as failsafe client])
AC_PATH_PROG(FAILSAFE_PATH, xterm)
AC_DEFINE_UNQUOTED(DEF_FAILSAFE_CLIENT, "$FAILSAFE_PATH", [Value is a path for program used as failsafe client])
AC_DEFINE_UNQUOTED(DEF_SESSION, "$FAILSAFE_PATH -ls", [Value is a command for default session])
AC_SUBST(FAILSAFE_PATH)


AC_ARG_VAR(DEF_SERVER, [Path to default X server])
AC_PATH_PROG(DEF_SERVER, X)
AC_DEFINE_UNQUOTED(DEF_SERVER_LINE, ":0 local $DEF_SERVER :0", [Value is default command to start X server])
AC_SUBST(DEF_SERVER)


AC_ARG_VAR(SYSTEM_SHELL, [Path to default shell])
AC_PATH_PROG(SYSTEM_SHELL, sh)
AC_PATH_PROG(SHELL_BASH,bash,,$PATH:/usr/local/bin)
if test "x$SYSTEM_SHELL" = "x"; then
	AC_MSG_ERROR(the program sh cannot be found)
fi
dnl hope this is correct:
if test "x$SHELL_BASH" = "x"; then
	SHELL_LOGIN="$SYSTEM_SHELL -"
else
	SHELL_LOGIN="$SHELL_BASH -login"
fi
AC_SUBST(SHELL_LOGIN)
AC_SUBST(SYSTEM_SHELL)



AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h fcntl.h sys/param.h syslog.h)

AC_CHECK_DECLS([sys_siglist])

AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_PID_T

dnl if there is mkstemp use it instead of mktemp
AC_CHECK_FUNC(mkstemp, 
	[AC_DEFINE(HAVE_MKSTEMP, 1,
		[Define if you have mkstemp function to use it instead of mktemp.])])

dnl Password processing options
AC_ARG_WITH(pamdir,
	AC_HELP_STRING([--with-pamdir],[specify where pam directory is]),
	[PAMDIR="$withval"],
	[PAMDIR="/etc/pam.d"])
AC_SUBST(PAMDIR)

AC_MSG_CHECKING(for pam being enabled)
AC_ARG_ENABLE(pam,
	AC_HELP_STRING([--enable-pam], [enable PAM]), 
	[
		AC_MSG_RESULT([yes])
		AC_DEFINE(USE_PAM, 1, [Define if you want wdm  to be compiiled with pam support])
		AC_CHECK_LIB(pam, main, [PASSWDLIB="-lpam -ldl"
			AC_DEFINE_UNQUOTED(HAVE_PAM_LIB, 1, [Define if libpam is installed])],
			[],-ldl)

		AC_MSG_CHECKING(for pam directory $PAMDIR)
		if test -d "$PAMDIR"; then
			AC_MSG_RESULT(exists)
		else
			AC_MSG_RESULT(does not exist)
			AC_MSG_WARN([this may not be a pam system <<--------------])
		fi
	],
	[
		AC_MSG_RESULT([no])
		AC_MSG_CHECKING(for shadow being enabled)
		AC_ARG_ENABLE(shadow,
			AC_HELP_STRING([--enable-shadow],[enable shadow passwords]),
			[
				AC_MSG_RESULT(yes)
				AC_DEFINE(USESHADOW, 1, [Define if you wand wdm to be compiled with shadow support])
				if test -z "$PASSWDLIB"; then
					AC_CHECK_LIB(shadow, main, [PASSWDLIB="-lshadow"
						AC_DEFINE_UNQUOTED(HAVE_SHADOW_LIB, 1, [Define if libshadow is installed])])
				fi
			],
			[
				AC_MSG_RESULT(no)
			])

	])
AM_CONDITIONAL(INSTALLPAM, test x$enable_pam = xyes)
AC_SUBST(PASSWDLIB)

LIBUTIL=
AC_CHECK_LIB(util, main, [LIBUTIL="-lutil"]) dnl for FreeBSD
AC_SUBST(LIBUTIL)

LIB_LIBS=
AC_CHECK_LIB(s, main, [LIB_LIBS="-ls"]) dnl for AIX
AC_SUBST(LIB_LIBS)

dnl crypt is a password and data encryption function
dnl the related header file is unistd.h

LIBCRYPT=
AC_CHECK_LIB(crypt, main, [LIBCRYPT="-lcrypt"]) dnl for BSD and RHL
AC_SUBST(LIBCRYPT)

LIBSOCKET=
AC_CHECK_FUNC(shmat, [],
	[
   		AC_CHECK_LIB(ipc, shmat, [LIBSOCKET="-lipc"]) dnl BSD??
	])
AC_SUBST(LIBSOCKET)

LIBUCB=
AC_CHECK_LIB(ucb, killpg, [LIBUCB="-lucb"]) dnl for Solaris2.4
AC_SUBST(LIBUCB)

LIB_ZLIB=
AC_CHECK_LIB(z, gzread, [LIBZLIB="-lz"])
AC_SUBST(LIB_ZLIB)

AC_PATH_XTRA
XCFLAGS="$CFLAGS $X_CFLAGS"
XLDFLAGS="$LDFLAGS $X_LIBS"
XLIBS="$LIBS $X_PRE_LIBS -lXext -lXt -lX11 -lm $X_EXTRA_LIBS"
AC_SUBST(XLIBS)
AC_SUBST(XCFLAGS)
AC_SUBST(XLDFLAGS)

GFXCFLAGS=
GFXLDFLAGS=
GFXLIBS=
supported_gfx=

dnl an option to handle /usr/local/{bin,include,lib}
AC_MSG_CHECKING([for using /usr/local/{bin,include,lib} paths])
AC_ARG_WITH(local,
	AC_HELP_STRING([--with-local],[set appropriate /usr/local/{bin,include,lib}]),
	[
		AC_MSG_RESULT(yes)
		GFXLDFLAGS="$GFXLDFLAGS -L/usr/local/lib"
		GFXCFLAGS="$GFXCFLAGS -I/usr/local/include"
		LOCAL_PATH="/usr/local/bin"
	],
	[
		AC_MSG_RESULT(no)
	])

dnl Specify paths to look for graphic format libraries
AC_MSG_CHECKING(for gfx-libs)
AC_ARG_WITH(gfx-libs,
	AC_HELP_STRING([--with-gfx-libs],[pass compiler flags to look for gfx libraries]),
	[
		AC_MSG_RESULT($withval)
		GFXLDFLAGS="$GFXLDFLAGS -L$withval"
	],
	[
		AC_MSG_RESULT([])
	]
)

AC_MSG_CHECKING(for gfx-incs)
AC_ARG_WITH(gfx-incs,
	AC_HELP_STRING([--with-gfx-incs],[pass compiler flags to look for gfx header files]),
	[
		AC_MSG_RESULT($withval)
		GFXCFLAGS="$GFXCFLAGS -I$withval"
	],
	[
		AC_MSG_RESULT([])
	])


AC_ARG_ENABLE(xpm,
	AC_HELP_STRING([--disable-xpm],[disable use of XPM pixmaps through libXpm]),
	xpm=$enableval, xpm=yes)
if test "$xpm" = yes; then
	AC_CHECK_LIB(Xpm, XpmCreatePixmapFromData,
	[
		GFXLDFLAGS="$GFXLDFLAGS -lXpm"
		supported_gfx="$supported_gfx XPM"
	],, $GFXCFLAGS $GFXLDFLAGS $XCFLAGS $XLDFLAGS $XLIBS)
fi

AC_ARG_ENABLE(png,
	AC_HELP_STRING([--disable-png],[disable PNG support through libpng]),
	png=$enableval, png=yes)

if test "$png" = yes; then
	AC_CHECK_LIB(png, png_get_valid,
	[
		GFXLIBS="$GFXLIBS -lpng $LIB_ZLIB"
		supported_gfx="$supported_gfx PNG"
	],, $GFXCFLAGS $GFXLDFLAGS $XCFLAGS $XLDFLAGS $XLIBS $LIB_ZLIB)
fi

AC_ARG_ENABLE(jpeg,
	AC_HELP_STRING([--disable-jpeg],[disable JPEG support through libjpeg]),
	jpeg=$enableval, jpeg=yes)

if test "$jpeg" = yes; then
	AC_CHECK_LIB(jpeg, jpeg_destroy_compress,
	[
		GFXLIBS="$GFXLIBS -ljpeg"
		supported_gfx="$supported_gfx JPEG"
	],, $GFXCFLAGS $GFXLDFLAGS $XCFLAGS $XLDFLAGS $XLIBS)
fi


dnl There are two implementations which support GIF: libgif and libungif.
dnl Both/either seem to work with wraster.
AC_ARG_ENABLE(gif,
	AC_HELP_STRING([--disable-gif],[disable JPEG support through libjpeg]),
	gif=$enableval, gif=yes)

if test "$gif" = yes; then
	AC_CHECK_LIB(ungif, EGifOpenFileName,
	[
		GFXLIBS="$GFXLIBS -lungif"
		supported_gfx="$supported_gfx GIF-ungif"
	],, $GFXCFLAGS $GFXLDFLAGS $XCFLAGS $XLDFLAGS $XLIBS)
	AC_CHECK_LIB(gif, DGifOpenFileName,
	[
		GFXLIBS="$GFXLIBS -lgif"
		supported_gfx="$supported_gfx GIF-gif"
	],, $GFXCFLAGS $GFXLDFLAGS $XCFLAGS $XLDFLAGS $XLIBS)
fi

tiflib=""
AC_ARG_WITH(tiff-libs,
	AC_HELP_STRING([--with-tiff-libs=LIB],[use libtiff with the specified extra libs]),
	tiflib=$withval, tiflib="")

AC_ARG_ENABLE(tiff,
	AC_HELP_STRING([--disable-tiff],[disable use of TIFF images through libtiff]),
	tif=$enableval, tif=yes)

dnl
dnl TIFF can optionally have JPEG and/or zlib support. Must find out
dnl when they are supported so that correct library flags are passed during
dnl detection and linkage
dnl
dnl

dnl for FreeBSD, check for libtiff34 also, should not hurt if both are found

if test "$tif" = yes; then
	AC_CHECK_LIB(tiff, TIFFGetVersion,
	[
		GFXLIBS="$GFXLIBS -ltiff $tiflib"
		supported_gfx="$supported_gfx TIFF"
	],,$GFXCFLAGS $GFXLDFLAGS $tiflib $XCFLAGS $XLDFLAGS $XLIBS)
	AC_CHECK_LIB(tiff34, TIFFGetVersion,
	[
		GFXLIBS="$GFXLIBS -ltiff34 $tiflib"
		supported_gfx="$supported_gfx TIFF34"
	],,$GFXCFLAGS $GFXLDFLAGS $tiflib $XCFLAGS $XLDFLAGS $XLIBS)
fi

dnl		WINGs support

dnl NOTE: Neither the naming convension nor the existance
dnl	  of debug enabled WINGs libs is standard.
dnl	  However, at this stage of WINGs development, we
dnl	  need something and this is it for now.
dnl using debug libs now controlled by --enable-debug

AC_ARG_ENABLE(debug,
	AC_HELP_STRING([--enable-debug],[enable debugging]),
	[
		enable_debug=yes
		CFLAGS="$CFLAGS -g -DDEBUG"
	], enable_debug=no)

LIBPROPLIST=
AC_CHECK_LIB(PropList, PLIsArray,
[
	LIBPROPLIST=-lPropList
],,$XCFLAGS $XLDFLAGS $XLIBS)

XFTLIBS=
XFTCFLAGS=
AC_CHECK_LIB(Xft2, XftDrawCreate,
[
	XFTLIBS=`xft-config --libs`
	XFTCFLAGS=`xft-config --cflags`
],[
	AC_CHECK_LIB(Xft, XftDrawCreate,
	[
		XFTLIBS=-lXft
	],,$XCFLAGS $XLDFLAGS $XLIBS)
],$XCFLAGS $XLDFLAGS $XLIBS)


WMLIBS=
AC_CHECK_LIB(wraster, RCreateImage,,
[
	AC_MSG_ERROR([wraster lib required and not found])
],$GFXLDFLAGS $XLDFLAGS $XLIBS $GFXLIBS $LIBPROPLIST $XFTLIBS)

AC_CHECK_LIB(wrasterg, RCreateImage,
[
	if test "$enable_debug" = yes; then
		WMLIBS="$WMLIBS -lwrasterg"
	else
		WMLIBS="$WMLIBS -lwraster"
	fi
],[
	WMLIBS="$WMLIBS -lwraster"
],$GFXLDFLAGS $XLDFLAGS $XLIBS $GFXLIBS $LIBPROPLIST $XFTLIBS)

AC_CHECK_LIB(WINGs, WMCreateScreen,,
[
	AC_MSG_ERROR([WINGs lib required and not found])
],$GFXLDFLAGS $XLDFLAGS $WMLIBS $XLIBS $GFXLIBS $LIBPROPLIST $XFTLIBS)

AC_CHECK_LIB(WINGsg, WMCreateScreen,
[
	if test "$enable_debug" = yes; then
		WMLIBS="$WMLIBS -lWINGsg"
	else
		WMLIBS="$WMLIBS -lWINGs"
	fi
],[
	WMLIBS="$WMLIBS -lWINGs"
],$GFXLDFLAGS $XLDFLAGS $WMLIBS $XLIBS $GFXLIBS $LIBPROPLIST $XFTLIBS)

if ! test "x$LIBPROPLIST" = "x"; then
	AC_MSG_CHECKING([if we really need libPropList])
	tmpLDFLAGS=$LDFLAGS
	tmpLIBS=$LIBS
	LDFLAGS="$GFXLDFLAGS $XLDFLAGS"
	LIBS="$WMLIBS $XLIBS $GFXLIBS $XFTLIBS"
	AC_TRY_LINK_FUNC(WMCreateScreen,
	[
		AC_MSG_RESULT(no)
		LIBPROPLIST=
	],[
		AC_MSG_RESULT(yes)
	])
	LDFLAGS=$tmpLDFLAGS
	LIBS=$tmpLIBS
fi

if ! test "x$XFTLIBS" = "x"; then
	AC_MSG_CHECKING([if we really need libXft])
	tmpLDFLAGS=$LDFLAGS
	tmpLIBS=$LIBS
	LDFLAGS="$GFXLDFLAGS $XLDFLAGS"
	LIBS="$WMLIBS $XLIBS $GFXLIBS $LIBPROPLIST"
	AC_TRY_LINK_FUNC(WMCreateScreen,
	[
		AC_MSG_RESULT(no)
		XFTLIBS=
		XFTCFLAGS=
	],[
		AC_MSG_RESULT(yes)
	])
	LDFLAGS=$tmpLDFLAGS
	LIBS=$tmpLIBS
fi
GFXLIBS="$WMLIBS $XFTLIBS $LIBPROPLIST $GFXLIBS"
GFXCFLAGS="$GFXCFLAGS $XFTCFLAGS"

AC_SUBST(GFXCFLAGS)
AC_SUBST(GFXLDFLAGS)
AC_SUBST(GFXLIBS)


dnl ===============================================
dnl prefix
dnl	handle this here so we can use it
dnl ===============================================

dnl First, check for prefix:
AC_MSG_CHECKING(for prefix)
if test "$prefix" = "NONE"; then
	prefix="/usr/local"
fi
AC_MSG_RESULT($prefix)

AC_MSG_CHECKING(for exec_prefix)
if test "$exec_prefix" = "NONE"; then
	exec_prefix=$prefix
fi
AC_MSG_RESULT($exec_prefix)

dnl ===============================================
dnl specify some WITH options for these settings
dnl ===============================================

AC_MSG_CHECKING(for logdir)
AC_ARG_WITH(logdir,
	AC_HELP_STRING([--with-logdir=PATH],[specify directory where logfile goes]))
if test "x$with_logdir" != "x"; then
	LOGDIR=$with_logdir
else
	LOGDIR="/var/log"
fi
AC_SUBST(LOGDIR)
AC_MSG_RESULT($LOGDIR)

AC_MSG_CHECKING(for runlockdir)
AC_ARG_WITH(runlockdir,
	AC_HELP_STRING([--with-runlockdir=PATH],[specify directory where run lock goes]))
if test "x$with_runlockdir" != "x"; then
	RUNDIR=$with_runlockdir
else
	RUNDIR="/var/run"
fi
AC_SUBST(RUNDIR)
AC_MSG_RESULT($RUNDIR)

AC_MSG_CHECKING(for defuserpath)
AC_ARG_WITH(defuserpath, 
	AC_HELP_STRING([--with-defuserpath=PATHlist],[specify list of paths for user sessions]))
if test "x$with_defuserpath" != "x"; then
	AC_DEFINE_UNQUOTED(DEF_USER_PATH,"$with_defuserpath", "Value is a list of paths for user sessions")
	AC_MSG_RESULT($with_defuserpath)
else
	AC_DEFINE(DEF_USER_PATH,"/usr/local/bin:/bin:/usr/bin")
	AC_MSG_RESULT([using default: /usr/local/bin:/bin:/usr/bin])
fi

AC_MSG_CHECKING(for defsystempath)
AC_ARG_WITH(defsystempath,
	AC_HELP_STRING([--with-defsystempath=PATHlist],[specify list of paths for superuser sessions]))
if test "x$with_defsystempath" != "x"; then
	AC_DEFINE_UNQUOTED(DEF_SYSTEM_PATH,"$with_defsystempath", "Value is a list of paths for superuser sessions")
	AC_MSG_RESULT($with_defsystempath)
else
	AC_DEFINE(DEF_SYSTEM_PATH,"/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin")
	AC_MSG_RESULT([using default: /sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin])
fi

dnl user must use --with-wdmdir= rather than --sysconfigdir= to put it
dnl somewhere other than $prefix/etc
dnl
AC_MSG_CHECKING(if  wdmdir has been specified)
AC_ARG_WITH(wdmdir,
	AC_HELP_STRING([--with-wdmdir=PATH],[specify path for wdm directory]))
if test "x$with_wdmdir" != "x"; then
	DMDIR="$with_wdmdir"
	AC_MSG_RESULT([yes, using $DMDIR])
else
	DMDIR="$prefix/etc/wdm"
	AC_MSG_RESULT([no, using $DMDIR])
fi
AC_DEFINE_UNQUOTED(WDMDIR, "$DMDIR", [Value is a path to wdm directory])
AC_DEFINE_UNQUOTED(DEF_XDM_CONFIG, "$DMDIR/wdm-config", [Value is a path to default wdm config file])
AC_DEFINE_UNQUOTED(DEF_CHOOSER, "$DMDIR/chooser", [Value is a path to default chooser program])
AC_DEFINE_UNQUOTED(DEF_AUTH_DIR, "$DMDIR/authdir", [Value is a path to default authdir])

AC_SUBST(DMDIR)

AC_MSG_CHECKING(if  Logo pixmap specified)
AC_ARG_WITH(Logo,
	AC_HELP_STRING([--with-Logo=PIXMAP_FILENAME],[specify file in wdm/pixmaps for Logo]))
if test "x$with_Logo" != "x"; then
	PIXMAPFILE="$with_Logo"
	PIXMAPCOMMENT=""
	AC_MSG_RESULT($with_Logo)
else
	PIXMAPFILE="Eagle.xpm"
	PIXMAPCOMMENT="! "
	AC_MSG_RESULT(no)
fi
AC_SUBST(PIXMAPFILE)
AC_SUBST(PIXMAPCOMMENT)



AC_ARG_VAR(WMAKER_PATH, [Path to wmaker program])
AC_PATH_PROG(WMAKER_PATH,wmaker,,$PATH:/usr/local/bin)
if test "x$WMAKER_PATH" != "x"; then
	WMLIST=WindowMaker
else
	WMAKER_PATH=wmaker
fi
AC_SUBST(WMAKER_PATH)


AC_ARG_VAR(AFTERSTEP_PATH, [Path to afterstep program])
AC_PATH_PROG(AFTERSTEP_PATH,afterstep,,$PATH:/usr/local/bin)
if test "x$AFTERSTEP_PATH" != "x"; then
	WMLIST=$WMLIST:AfterStep
else
	AFTERSTEP_PATH=afterstep
fi
AC_SUBST(AFTERSTEP_PATH)


AC_ARG_VAR(BLACLBOX_PATH, [Path to blackbox program])
AC_PATH_PROG(BLACKBOX_PATH,blackbox,,$PATH:/usr/local/bin)
if test "x$BLACKBOX_PATH" != "x"; then
	WMLIST=$WMLIST:BlackBox
else
	BLACKBOX_PATH=blackbox
fi
AC_SUBST(BLACKBOX_PATH)


AC_ARG_VAR(ICEWM_PATH, [Path to icewm program])
AC_PATH_PROG(ICEWM_PATH,icewm,,$PATH:/usr/local/bin)
if test "x$ICEWM_PATH" != "x"; then
	WMLIST=$WMLIST:IceWm
else
	ICEWM_PATH=icewm
fi
AC_SUBST(ICEWM_PATH)


AC_ARG_VAR(XSESSION_PATH, [Path to xsession program])
AC_PATH_PROG(XSESSION_PATH,xsession,,$PATH:/usr/local/bin)
if test "x$XSESSION_PATH" != "x"; then
	WMLIST=$WMLIST:Xsession
else
	XSESSION_PATH=xsession
fi
AC_SUBST(XSESSION_PATH)


AC_PATH_PROG(STARTKDE_PATH,startkde,,$PATH:/usr/local/bin)
if test "x$STARTKDE_PATH" != "x"; then
	WMLIST=$WMLIST:KDE
else
	STARTKDE_PATH=startkde
fi
AC_SUBST(STARTKDE_PATH)


AC_ARG_WITH(winmgr,
	AC_HELP_STRING([--with-winmgr=NAME_OF_WM], [specify an arbitrary window manager]))
if test "x$with_winmgr" != "x"; then
	WINMGR_NAME=$with_winmgr
	AC_PATH_PROG(WINMGR_PATH,$WINMGR_NAME,,$PATH:/usr/local/bin)
	if test "x$WINMGR_PATH" = "x"; then
	AC_ARG_WITH(winmgr_path,
		AC_HELP_STRING([--with-winmgr_path=WINMGR_PATH], [specify path to your window manager]))
		if test "x$with_winmgr_path" != "x"; then
			WINMGR_PATH=$with_winmgr_path
		fi
	fi
	AC_MSG_CHECKING(for winmgr_path)
	if test "x$WINMGR_PATH" = "x"; then
		AC_MSG_ERROR([unable to locate $WINMGR_NAME, use --with-winmgr_path])
	fi
	AC_MSG_RESULT($WINMGR_PATH)
	WMLIST=$WMLIST:$WINMGR_NAME
fi
AC_SUBST(WINMGR_PATH)
AC_SUBST(WINMGR_NAME)


AC_MSG_CHECKING(if  window manager list specified)
AC_ARG_WITH(wmlist,
	AC_HELP_STRING([--with-wmlist=LIST_OF_WM],[specify list of window managers]))
if test "x$with_wmlist" != "x"; then
	WMLIST="$with_wmlist"
	AC_MSG_RESULT($with_wmlist)
	AC_MSG_WARN(make sure Xsession and Xclients support the above list)
else
	AC_MSG_RESULT([no, default to $WMLIST])
fi
AC_SUBST(WMLIST)






dnl ==========================================================
AC_MSG_RESULT(full gfx include paths and flags are $XCFLAGS $GFXFLAGS)
AC_MSG_RESULT(full gfx paths and libraries are $XLDFLAGS $GFXLIBS)
AC_MSG_RESULT(gfx library formats: $supported_gfx)
dnl ==========================================================



AC_CONFIG_FILES([Makefile
	  doc/Makefile
	  src/Makefile
	  src/pixmaps/Makefile])
AC_OUTPUT
