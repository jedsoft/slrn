dnl# -*- sh -*-
AC_INIT
AC_CONFIG_SRCDIR([src/slrn.c])
JD_INIT

AC_CONFIG_AUX_DIR(autoconf)
AC_CANONICAL_HOST

AC_PROG_MAKE_SET
AC_PROG_INSTALL

JD_ANSI_CC
JD_ELF_COMPILER

m4_include([./include/ax_lib_socket_nsl.m4])
AX_LIB_SOCKET_NSL

# determine X include and needed libs
dnl# AC_PATH_XTRA

#------------------------------------------------------------------------
# Useful macros
#------------------------------------------------------------------------

AC_DEFUN([CF_COMPILE_OPTION],[
 optionvalue=$4
 AC_ARG_ENABLE($1,[$2],[
  if test "x$enableval" = "xyes" ; then
   optionvalue=1
  else
   optionvalue=0
  fi
 ])
 AC_SUBST($3, $optionvalue)
])dnl


#---------------------------------------------------------------------------
# Configure options
#---------------------------------------------------------------------------
dnl# Unfortunately, autoconf lacks a --docdir option. This is a workaround:
AC_ARG_WITH(docdir,
    [  --with-docdir=DIR       Where to put the documentation],
    [ if test "x$withval" = xyes || test "x$withval" = xno; then
         docdir='${prefix}/share/doc/slrn'
      else
         docdir="$withval"
      fi
    ], [ docdir='${prefix}/share/doc/slrn' ])
AC_SUBST(docdir)

AC_ARG_WITH(slrnlib,
    [  --with-slrnlib=DIR       Place for the slrn library files])
if test -z "$with_slrnlib" || test "x$with_slrnlib" = xyes || test "x$with_slrnlib" = xno; then
	if test "x$prefix" = xNONE; then
		slrn_lib_dir="${ac_default_prefix}/share/slrn"
	else
		slrn_lib_dir="${prefix}/share/slrn"
	fi
else
	slrn_lib_dir="$with_slrnlib"
fi
AC_SUBST(SLRN_LIB_DIR,"$slrn_lib_dir")

dnl# directory where .mo (translation) files should be placed
AC_ARG_WITH(localedir,
[  --with-localedir=DIR    Where to put locale files])
if test -z "$with_localedir" || test "x$with_localedir" = xyes || test "x$with_localedir" = xno; then
    if test "x$prefix" = xNONE; then
	LOCALEDIR="${ac_default_prefix}/share/locale"
    else
        LOCALEDIR="${prefix}/share/locale"
    fi
else
    LOCALEDIR="$with_localedir"
fi		    
AC_SUBST(LOCALEDIR)

optionvalue=0
SLRNPULL_ROOT_DIR="/var/spool/news/slrnpull"
AC_ARG_WITH(slrnpull,
    [  --with-slrnpull=[[DIR]]        Build slrnpull and use DIR as the slrnpull root],
    [if test "x$withval" != "xno" ; then
      optionvalue=1
      SLRNPULL="slrnpull\$(EXEEXT)"
      MAN_SLRNPULL="../doc/slrnpull.1"
      if test "x$withval" != "xyes" -a "x$withval" != "x"; then
        SLRNPULL_ROOT_DIR="$withval"
      fi
     fi
    ])dnl
AC_SUBST(SLRN_HAS_PULL_SUPPORT, $optionvalue)
AC_SUBST(SLRNPULL)
AC_SUBST(SLRNPULL_ROOT_DIR)
AC_SUBST(MAN_SLRNPULL)

AC_ARG_WITH(server-name,
    AS_HELP_STRING([--with-server-name=HOST],
                   [Hostname of a default NNTP server]),
    [if test "x$withval" != "xno"; then
      if test "x$withval" = "xyes"; then
        AC_MSG_ERROR([you must pass the hostname of the server to the option --with-server-name])
      fi
      AC_DEFINE_UNQUOTED([NNTPSERVER_NAME], "$withval")
     fi
    ])
AC_ARG_WITH(server-file,
    AS_HELP_STRING([--with-server-file=FILE],
                   [File containing the hostname of an NNTP server]),
    [if test "x$withval" != "xno"; then
      if test "x$withval" = "xyes"; then
        AC_MSG_ERROR([you must pass the name of the file to the option --with-server-file])
      fi
      AC_DEFINE_UNQUOTED([NNTPSERVER_FILE], "$withval")
     fi
    ])


CF_COMPILE_OPTION(nntp,
    [  --disable-nntp          Disable NNTP support],
    [SLRN_HAS_NNTP_SUPPORT], 1)

CF_COMPILE_OPTION(spool,
    [  --enable-spool          Enable support for reading directly from spool],
    [SLRN_HAS_SPOOL_SUPPORT], 0)

CF_COMPILE_OPTION(inews,
    [  --enable-inews          Enable inews posting support],
    [SLRN_HAS_INEWS_SUPPORT], 0)
if test "x$enable_inews" = xyes ; then
  dnl# find inews
  AC_PATH_PROG(INEWS, inews, no, $PATH:/usr/local/bin:/usr/bin)
  if test "x$ac_cv_path_INEWS" != xno; then
      dnl# AC_DEFINE_UNQUOTED(SLRN_INEWS_COMMAND, "$ac_cv_path_INEWS -S -h")
      AC_SUBST(SLRN_INEWS_COMMAND, "$ac_cv_path_INEWS -S -h")
  else
      AC_MSG_ERROR([
I cannnot find an inews executable.  When using --enable-inews, please make
sure inews can be found in your \$PATH.
])
  fi
else
  AC_SUBST(SLRN_INEWS_COMMAND, "/usr/local/bin/inews -S -h")
fi

CF_COMPILE_OPTION(force-inews,
    [  --enable-force-inews    Enforce the use of inews],
    [SLRN_FORCE_INEWS], 0)

CF_COMPILE_OPTION(strict-from,
    [  --enable-strict-from    Disallow custom From headers],
    [SLRN_HAS_STRICT_FROM], 0)

CF_COMPILE_OPTION(charmap,
    [  --disable-charmap       Disable mapping ISO-Latin to native charsets],
    [SLRN_HAS_CHARACTER_MAP], 1)

CF_COMPILE_OPTION(setgid-code,
    [  --enable-setgid-code    Allow running slrnpull as an unpriviledged user],
    [SLRNPULL_USE_SETGID_POSTS], 0)
CF_COMPILE_OPTION(spoilers,
    [  --disable-spoilers      Disable spoiler support],
    [SLRN_HAS_SPOILERS], 1)
CF_COMPILE_OPTION(emph-text,
    [  --disable-emph-text     Disable emphasized text support],
    [SLRN_HAS_EMPHASIZED_TEXT], 1)
CF_COMPILE_OPTION(gen-mid,
    [  --disable-gen-mid       Prevent slrn (and user) from generating Message-IDs],
    [SLRN_HAS_GEN_MSGID], 1)
CF_COMPILE_OPTION(mid-cache,
    [  --enable-mid-cache      Make slrn cache Message-IDs to eliminate cross-posts
                          (only needed when server has no xref headers)],
    [SLRN_HAS_MSGID_CACHE], 0)
CF_COMPILE_OPTION(fake-refs,
    [  --disable-fake-refs     Do not use In-Reply-To if References is not available],
    [SLRN_HAS_FAKE_REFS], 1)
CF_COMPILE_OPTION(grouplens,
    [  --enable-grouplens      Enable GroupLens support],
    [SLRN_HAS_GROUPLENS], 0)

jd_with_uu_library=no
JD_CHECK_FOR_LIBRARY(uu,uudeview.h)
if test $jd_with_uu_library = yes
then
  LINK_LIBUU="-luu"
  AC_DEFINE(HAVE_UUDEVIEW_H)
  optionvalue=1
else
  optionvalue=0
  LINK_LIBUU=""
fi
AC_SUBST(LINK_LIBUU)
AC_SUBST(SLRN_HAS_UUDEVIEW,$optionvalue)

jd_with_canlock_library=no
JD_CHECK_FOR_LIBRARY(canlock)
if test $jd_with_canlock_library = yes
then
  LINK_LIBCANLOCK="-lcanlock"
  AC_DEFINE(HAVE_CANLOCK_H)
  optionvalue=1
else
  optionvalue=0
  LINK_LIBCANLOCK=""
fi
AC_SUBST(LINK_LIBCANLOCK)
AC_SUBST(SLRN_HAS_CANLOCK,$optionvalue)


#---------------------------------------------------------------------------
# gettext
#---------------------------------------------------------------------------
m4_include([./include/gettext.m4])
m4_include([./include/iconv.m4])
m4_include([./include/nls.m4])
m4_include([./include/po.m4])
m4_include([./include/lib-ld.m4])
m4_include([./include/lib-link.m4])
m4_include([./include/lib-prefix.m4])
m4_include([./include/mkdirp.m4])
m4_include([./include/progtest.m4])
AM_ICONV
AM_GNU_GETTEXT(external)
AC_SUBST(INCICONV)dnl# AM_GNU_GETTEXT does not do this.  Why???

AC_ARG_WITH(non-gnu-iconv,
    [  --with-non-gnu-iconv    Local iconv is not GNU compatible],
    [use_non_gnu_iconv=$withval], [use_non_gnu_iconv=no])
case "x$use_non_gnu_iconv" in
x|xyes)
  AC_DEFINE(NON_GNU_ICONV)dnl
  ;;
esac

#---------------------------------------------------------------------------
# SSL stuff
#---------------------------------------------------------------------------
AC_ARG_WITH(nss-compat,
    [  --with-nss-compat       Use NSS compat library instead of OpenSSL],
    [use_nss_compat=$withval], [use_nss_compat=no])
AC_ARG_WITH(gnutls,
    AS_HELP_STRING([--with-gnutls[=path]],
                   [Use GNU TLS library instead of OpenSSL]),
    [use_gnutls=$withval; jd_cv_use_ssl=$withval],
    [use_gnutls=no])
AC_CACHE_CHECK(if you want SSL support,jd_cv_use_ssl,
[
  AC_ARG_WITH(ssl, 
  [  --with-ssl[[=path]]       for SSL support],
  [jd_cv_use_ssl=$withval], [jd_cv_use_ssl=no])
])
case "x$jd_cv_use_ssl" in
xno)
   SSLINC=
   SSLLIB=
  ;;
x|xyes)
   if test "x$use_nss_compat" != xno
   then
     SSLINC="-I/usr/local/include/nss_compat_ossl -I/usr/local/include/nss3 -I/usr/local/include/nspr4"
     SSLLIB="-L/usr/local/lib -lnss_compat_ossl"
   elif test "x$use_gnutls" != xno
   then
     SSLINC=
     SSLLIB="-lgnutls-openssl -lgnutls"
   else
     SSLINC="-I/usr/local/ssl/include"
     SSLLIB="-L/usr/local/ssl/lib -lssl -lcrypto"
   fi
;;
*)
   if test "x$use_nss_compat" != xno; then
     SSLINC="-I$jd_cv_use_ssl/include/nss_compat_ossl -I$jd_cv_use_ssl/include/nss3 -I$jd_cv_use_ssl/include/nspr4"
     SSLLIB="-L$jd_cv_use_ssl/lib -lnss_compat_ossl"
   elif test "x$use_gnutls" != xno
   then
     SSLINC="-I$jd_cv_use_ssl/include"
     # SSLLIB="-L$jd_cv_use_ssl/lib -lgnutls-extra -lgnutls -ltasn1 -lgcrypt -lgnutls-openssl"
     SSLLIB="-L$jd_cv_use_ssl/lib -lgnutls -lgnutls-openssl"
   else
     SSLINC="-I$jd_cv_use_ssl/include"
     SSLLIB="-L$jd_cv_use_ssl/lib -lssl -lcrypto"
   fi
esac
AC_SUBST(SSLINC)
AC_SUBST(SSLLIB)
if test "x$SSLINC$SSLLIB" != x; then
 AC_DEFINE(USE_SSL)
 if test "x$use_nss_compat" != xno; then
   AC_DEFINE(USE_NSS_COMPAT)
 fi
 if test "x$use_gnutls" != xno; then
   AC_DEFINE(USE_GNUTLS)
 fi
fi

#---------------------------------------------------------------------------
# Check header files
#---------------------------------------------------------------------------

AC_CHECK_HEADERS( \
direct.h \
stdlib.h \
unistd.h \
memory.h \
fcntl.h \
sys/fcntl.h \
sys/dir.h \
ndir.h \
sys/ndir.h \
malloc.h \
termios.h \
sys/socket.h \
sys/time.h \
socket.h \
sys/wait.h \
netinet/in.h \
arpa/inet.h \
sys/utsname.h \
locale.h langinfo.h \
)

#AC_CHECK_LIB(socket, socket, [LIBSOCKET="-lnsl -lsocket"]) dnl for Solaris
#AC_CHECK_LIB(bsd, gethostbyname, [LIBSOCKET="-lbsd"]) dnl for LynxOS
#AC_SUBST(LIBSOCKET)

AC_HEADER_DIRENT

AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_UID_T

#dnl Eventually, this should go away.  We don't need long doubles.  It is 
#dnl here for the snprintf code.
AC_CHECK_TYPE(long double)

AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(float, 4)
AC_CHECK_SIZEOF(double, 8)

JD_CHECK_LONG_LONG
JD_LARGE_FILE_SUPPORT

AC_CHECK_FUNCS(\
memset \
memcpy \
memcmp \
memchr \
putenv \
getcwd \
getdomainname \
getaddrinfo \
getnameinfo \
gettimeofday \
sigaction \
sigemptyset \
sigprocmask \
sigaddset \
siglongjmp \
vsnprintf \
uname \
setlocale \
isalpha isspace isdigit isalnum ispunct \
atoll strtoll \
)

AC_MSG_CHECKING(for timezone in time.h)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[long int tz; tz = timezone % 1]])],[AC_DEFINE(HAVE_TIMEZONE) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for tm_gmtoff in struct tm)
AH_TEMPLATE([HAVE_TM_GMTOFF],
[define if you have "tm_gmtoff" in struct tm])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[struct tm t; t.tm_gmtoff = 0]])],[AC_DEFINE(HAVE_TM_GMTOFF) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])

dnl # AC_CACHE_CHECK(for domain in /etc/resolv.conf, slrn_cv_domain,
dnl # [if test -f /etc/resolv.conf; then
dnl # 	slrn_cv_domain=`(grep "^domain" /etc/resolv.conf ||
dnl # 	                 grep "^search" /etc/resolv.conf) |
dnl # 			awk '{ print [$]2 }' | head -1`
dnl #  fi
dnl #  if test -z "$slrn_cv_domain"; then
dnl # 	slrn_cv_domain=no
dnl #  fi
dnl # ])

dnl # if test "X$slrn_cv_domain" != Xno; then
dnl # 	AC_DEFINE(USE_DOMAIN_NAME)
dnl # 	AC_DEFINE_UNQUOTED(MY_DOMAIN_NAME, "${slrn_cv_domain}")
dnl # fi

AC_DEFUN(CF_VA_COPY,
[
 dnl# va_copy checks taken from glib 1.2.8
 dnl#
 dnl# we currently check for all three va_copy possibilities, so we get
 dnl# all results in config.log for bug reports.
AC_MSG_CHECKING(for an implementation of va_copy())
AC_CACHE_VAL(slrn_cv_va_copy,[
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <stdarg.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	va_copy (args2, args1);
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],[slrn_cv_va_copy=yes
	],[slrn_cv_va_copy=no
	],[])
])
AC_MSG_RESULT($slrn_cv_va_copy)
AC_MSG_CHECKING(for an implementation of __va_copy())
AC_CACHE_VAL(slrn_cv___va_copy,[
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <stdarg.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	__va_copy (args2, args1);
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],[slrn_cv___va_copy=yes
	],[slrn_cv___va_copy=no
	],[])
])
AC_MSG_RESULT($slrn_cv___va_copy)
AC_MSG_CHECKING(whether va_lists can be copied by value)
AC_CACHE_VAL(slrn_cv_va_val_copy,[
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <stdarg.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	args2 = args1;
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],[slrn_cv_va_val_copy=yes
	],[slrn_cv_va_val_copy=no
	],[])
])

if test "x$slrn_cv_va_copy" = "xyes"
then
  AC_DEFINE(VA_COPY, va_copy)
else 
  if test "x$slrn_cv___va_copy" = "xyes"
  then
    AC_DEFINE(VA_COPY, __va_copy)
  fi
fi

if test "x$slrn_cv_va_val_copy" = "xno"
then
  AC_DEFINE(VA_COPY_AS_ARRAY)
fi
AC_MSG_RESULT($slrn_cv_va_val_copy)
])

CF_VA_COPY

JD_SET_OBJ_SRC_DIR
JD_TERMCAP
JD_WITH_LIBRARY(slang)
JD_GCC_WARNINGS
JD_GET_MODULES(src/modules.lis)
JD_CREATE_MODULE_ORULES

AC_CONFIG_HEADER(src/sysconf.h:src/config.hin)
AC_CONFIG_FILES(po/Makefile.in)

AC_CONFIG_FILES([src/slrnfeat.h:src/slrnfeat.hin])
AC_OUTPUT
AC_CONFIG_FILES([Makefile:autoconf/Makefile.in src/Makefile])
AC_OUTPUT
AC_CONFIG_FILES([po/Makefile])
AC_OUTPUT

JD_CREATE_ORULE(chkslang)

JD_CREATE_ORULE(sltcp)
JD_CREATE_ORULE(nntplib)
JD_CREATE_EXEC_RULE(slrnpull)

JD_APPEND_RULES(src/Makefile)

dnl #if test "$slrn_cv_domain" != no; then
dnl #  echo ""
dnl #  echo "Your DOMAIN appears to be: $slrn_cv_domain"
dnl #  echo "If this is not correct, edit src/sysconf.h!!!"
dnl #fi

echo ""
echo "For some INSTALLATION options,  edit src/Makefile."
echo "For INEWS, LOCAL SPOOL, GROUPLENS, SLANG, ... support, edit src/slrnfeat.h"
echo ""

