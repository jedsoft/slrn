dnl -*- sh -*-
dnl Copyright (c) 2001, 2002 Thomas Schultz <tststs@gmx.de>
dnl partly based on code by N. D. Bellamy

dnl Wrapper around AC_ARG_ENABLE to set compile time options.
dnl
dnl Parameters:
dnl $1 = option name
dnl $2 = help string
dnl $3 = variable to set in sysconf.h

AC_DEFUN(CF_COMPILE_OPTION,
[AC_ARG_ENABLE($1,[$2],[if test "x$enableval" = "xyes" ; then
AC_DEFINE($3, 1)
else
AC_DEFINE($3, 0)
fi])])dnl

dnl Originally called JD_ANSI_CC and written by John E. Davis:
AC_DEFUN(CF_ANSI_CC,
[
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_ISC_POSIX
AC_AIX

dnl #This stuff came from Yorick config script
dnl
dnl #Be sure we've found compiler that understands prototypes
dnl
AC_MSG_CHECKING(C compiler that understands ANSI prototypes)
AC_TRY_COMPILE([ ],[
 extern int silly (int);], [
 AC_MSG_RESULT($CC looks ok.  Good.)], [
 AC_MSG_RESULT($CC is not a good enough compiler)
 AC_MSG_ERROR(Set env variable CC to your ANSI compiler and rerun configure.)
 ])dnl
])dnl

dnl Originally called JD_TERMCAP and written by John E. Davis
AC_DEFUN(CF_TERMCAP,
[
AC_MSG_CHECKING(for Terminfo)

JD_Terminfo_Dirs="/usr/lib/terminfo \
                 /usr/share/terminfo \
                 /usr/share/lib/terminfo \
		 /usr/local/lib/terminfo \
		 $FINKPREFIX/share/terminfo"

TERMCAP=-ltermcap

AC_MSG_CHECKING(for Terminfo)
for terminfo_dir in $JD_Terminfo_Dirs
do
   if test -d $terminfo_dir 
   then
      AC_MSG_RESULT(yes)
      TERMCAP=""
      break
   fi
done
if test "$TERMCAP"; then
  AC_MSG_RESULT(no)
fi
AC_SUBST(TERMCAP)dnl
])

AC_DEFUN(CF_GCC_OPTIONS,
[
AC_ARG_ENABLE(warnings,
	      [  --enable-warnings       turn on compiler warnings (GCC only)],
	      [gcc_warnings=$enableval])
AC_ARG_ENABLE(profiling,
	      [  --enable-profiling      turn on profiling (GCC only)],
	      [gcc_profiling=$enableval])
if test -n "$GCC"
then

AC_MSG_CHECKING([for gcc 2.7.* (assume bug in strength reduction)])
AC_CACHE_VAL(ac_cv_gcc_optimizer_bug, [
    AC_EGREP_CPP(opt_bug,
    [#if __GNUC__ == 2 && __GNUC_MINOR__ == 7
	opt_bug
    #endif
    ], ac_cv_gcc_optimizer_bug=true, ac_cv_gcc_optimizer_bug=false)
])

  if test "x$ac_cv_gcc_optimizer_bug" = xtrue; then
    CFLAGS="$CFLAGS -fno-strength-reduce"
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

  if test "x$gcc_warnings" = xyes; then
    CFLAGS="$CFLAGS -Wall -W -pedantic -Winline -Wmissing-prototypes \
 -Wnested-externs -Wpointer-arith -Wcast-align -Wshadow -Wstrict-prototypes"
  fi
  
  if test "x$gcc_profiling" = xyes; then
    CFLAGS="$CFLAGS -pg"
    LDFLAGS="$LDFLAGS -pg"
  fi
  
  # Now trim excess whitespace
  CFLAGS=`echo $CFLAGS`
  LDFLAGS=`echo $LDFLAGS`
fi
])

dnl evaluate $libdir and $includedir
dnl results are in $ev_libdir and $ev_includedir respecively
AC_DEFUN(CF_EVAL_VARS,
[
ev_prefix=$prefix
test "x$ev_prefix" = xNONE && ev_prefix=$ac_default_prefix
ev_exec_prefix=$exec_prefix
test "x$ev_exec_prefix" = xNONE && ev_exec_prefix=$ev_prefix

eval `sh <<+
prefix=$ev_prefix
exec_prefix=$ev_exec_prefix
libdir=$libdir
includedir=$includedir
bindir=$bindir
datadir=$datadir
docdir=$docdir
mandir=$mandir
sysconfdir=$sysconfdir

echo ev_libdir="\$libdir" ev_includedir="\$includedir"
echo ev_bindir="\$bindir" ev_datadir="\$datadir"
echo ev_docdir="\$docdir" ev_mandir="\$mandir"
echo ev_sysconfdir="\$sysconfdir"
+
`
])

AC_DEFUN(CF_HARDCODE_LIBS,
[
AC_ARG_ENABLE(hardcode-libs,
              [  --enable-hardcode-libs  Hardcode path of dynamic libs in binaries])

cf_rpath_option=""
if test "x$enable_hardcode_libs" = "xyes" ; then
  AC_MSG_CHECKING(if linker accepts -rpath)
  AC_CACHE_VAL(ac_cv_rpath, [
  save_ldflags="$LDFLAGS"
  LDFLAGS="$LDFLAGS -Wl,-rpath,/usr/lib"
  AC_TRY_LINK([], [],
	[ac_cv_rpath=true],
	[ac_cv_rpath=false])
  LDFLAGS="$save_ldflags"
  ])
  if test "x$ac_cv_rpath" = "xtrue" ; then
    AC_MSG_RESULT(yes)
    cf_rpath_option="-Wl,-rpath,"
  else
    AC_MSG_RESULT(no)
    cf_rpath_option="-Wl,-R,"
  fi
fi
])

AC_DEFUN(CF_PATH_SLANG_LIB,
[
  AC_REQUIRE([CF_EVAL_VARS])
  AC_MSG_CHECKING(for the slang library)
  
  ac_slang_library="no"
  
  AC_ARG_WITH(slang-library,
    [  --with-slang-library    Where the slang library is located ],
    [  ac_slang_library="$withval" ])

  dnl Did the user give --with-slang-library?
  
  if test "x$ac_slang_library" = xno || test "x$ac_slang_library" = xyes ; then

    dnl No they didn't, so let's look for them...
    
    if test "x$ac_cv_lib_slang" != "x" ; then
      ac_slang_library="$ac_cv_lib_slang"
    fi

    AC_CACHE_VAL(ac_cv_lib_slang, [
    
      dnl If you need to add extra directories to check, add them here.
      
      slang_library_dirs="\
        /usr/local/lib/slang \
	/usr/local/lib \
	/usr/lib/slang \
        /usr/lib \
	/usr/pkg/lib \
	$ev_libdir"
  
      for slang_dir in $slang_library_dirs; do
        if test -r "$slang_dir/libslang.a" || \
	   test -r "$slang_dir/libslang.so" ; then
          ac_slang_library="$slang_dir"
          break
        fi
      done

      ac_cv_lib_slang="$ac_slang_library"
    ])
  fi

  AC_MSG_RESULT([$ac_slang_library])

  if test "x$ac_slang_library" = xno; then
    AC_MSG_ERROR([

I can't find the slang library.

Install the slang library, or if you have it installed, override this check
with the --with-slang-library=DIR option, and I'll take your word for it.
])

  fi
  
  SLANG_LIB_DIR="$ac_slang_library"
  if test "x$enable_hardcode_libs" = "xyes" ; then
      SLANG_LIB="-L$ac_slang_library $cf_rpath_option$ac_slang_library"
  else
      SLANG_LIB="-L$ac_slang_library"
  fi
  
# gcc under solaris is often not installed correctly.  Avoid specifying
# -L/usr/lib.
#if test "$SLANG_LIB_DIR" = "/usr/lib"
#then
#    SLANG_LIB=""
#fi

AC_SUBST(SLANG_LIB)dnl
AC_SUBST(SLANG_LIB_DIR)dnl
])

AC_DEFUN(CF_PATH_SLANG_INC,
[
  AC_REQUIRE([CF_EVAL_VARS])
  AC_MSG_CHECKING(for the slang includes)
  
  ac_slang_includes="no"
  
  AC_ARG_WITH(slang-includes,
    [  --with-slang-includes   Where the slang headers are located ],
    [  ac_slang_includes="$withval" ])

  dnl Did the user give --with-slang-includes?
  
  if test "x$ac_slang_includes" = xno || \
     test "x$ac_slang_includes" = xyes ; then

    dnl No they didn't, so lets look for them...
    
    if test "x$ac_cv_header_slanginc" != "x" ; then
      ac_slang_includes="$ac_cv_header_slanginc"
    fi

    AC_CACHE_VAL(ac_cv_header_slanginc, [

      dnl If you need to add extra directories to check, add them here.
      
      slang_include_dirs="\
        /usr/local/include/slang \
	/usr/local/include \
	/usr/include/slang \
        /usr/include \
	/usr/pkg/include \
	$ev_includedir"
      
      for slang_dir in $slang_include_dirs; do
        if test -r "$slang_dir/slang.h"; then
          ac_slang_includes="$slang_dir"
          break
        fi
      done

      ac_cv_header_slanginc="$ac_slang_includes"
    ])
  fi

  AC_MSG_RESULT([$ac_slang_includes])

  if test "x$ac_slang_includes" = xno; then
    AC_MSG_ERROR([

I can't find the slang header files.

Install the slang development package, or if you have it installed, override
this check with the --with-slang-includes=DIR option, and I'll take your
word for it.
])

  fi

  SLANG_INCLUDE="$ac_slang_includes"
  SLANG_INC="-I$ac_slang_includes"
  
# gcc under solaris is often not installed correctly.  Avoid specifying
# -I/usr/include.
if test "$SLANG_INC" = "-I/usr/include"
then
    SLANG_INC=""
fi

AC_SUBST(SLANG_INC)dnl
AC_SUBST(SLANG_INCLUDE)dnl
])

AC_DEFUN(CF_SSL,
[
  SSLLIB=""
  SSLINC=""
  
  AC_ARG_WITH(ssl,
    [  --with-ssl[=DIR]        For SSL support],
    [  ac_ssl_home="$withval" ], [ ac_ssl_home=no ])
  
  AC_ARG_WITH(ssl-library,
    [  --with-ssl-library      Where the OpenSSL library is located ],
    [  ac_ssl_library="$withval" ], [ ac_ssl_library=no ])

  AC_ARG_WITH(ssl-includes,
    [  --with-ssl-includes     Where the OpenSSL headers are located ],
    [  ac_ssl_includes="$withval" ], [ ac_ssl_includes=no ])

  if test "x$ac_ssl_home" != xno || test "x$ac_ssl_library" != xno || \
     test "x$ac_ssl_includes" != xno ; then
  
    dnl We want SSL support
  
    AC_MSG_CHECKING(for the OpenSSL library)
    
    if test "x$ac_ssl_library" = xno || test "x$ac_ssl_library" = xyes ; then
  
    if test "x$ac_cv_lib_ssl" != "x" ; then
      ac_ssl_library="$ac_cv_lib_ssl"
    fi

    AC_CACHE_VAL(ac_cv_lib_ssl, [
    
      dnl If you need to add extra directories to check, add them here.
      
      ssl_library_dirs="\
        /usr/local/ssl/lib \
	/usr/local/lib \
  	/usr/ssl/lib \
        /usr/lib \
	/usr/pkg/lib"
  	
      if test "x$ac_ssl_home" != xno && test "x$ac_ssl_home" != xyes ; then
        ssl_library_dirs="$ac_ssl_home $ac_ssl_home/lib $ssl_library_dirs"
      fi
  
      for ssl_dir in $ssl_library_dirs; do
        if test -r "$ssl_dir/libssl.a" || \
	   test -r "$ssl_dir/libssl.so" ; then
          ac_ssl_library="$ssl_dir"
          break
        fi
      done

      ac_cv_lib_ssl="$ac_ssl_library"
    ])
    
    fi
    
    AC_MSG_RESULT([$ac_ssl_library])
    
    if test "x$ac_ssl_library" = xno || test "x$ac_ssl_library" = xyes; then
    AC_MSG_ERROR([

Please install the OpenSSL library.  If you already did so, point this script
to the right directory with the --with-ssl-library=DIR option.
])
    fi
    
    # gcc under solaris is often not installed correctly.  Avoid specifying
    # -L/usr/lib.
    if test "x$ac_ssl_library" = "x/usr/lib" ; then
        SSLLIB="-lssl -lcrypto"
    else
        if test "x$enable_hardcode_libs" = "xyes" ; then
            SSLLIB="-L$ac_ssl_library $cf_rpath_option$ac_ssl_library -lssl -lcrypto"
        else
            SSLLIB="-L$ac_ssl_library -lssl -lcrypto"
        fi
    fi
    
    AC_MSG_CHECKING(for the OpenSSL includes)
    
    if test "x$ac_ssl_includes" = xno || test "x$ac_ssl_includes" = xyes ; then
    
    if test "x$ac_cv_header_ssl" != "x" ; then
      ac_ssl_includes="$ac_cv_header_ssl"
    fi

    AC_CACHE_VAL(ac_cv_header_ssl, [

      dnl If you need to add extra directories to check, add them here.
      
      ssl_include_dirs="\
        /usr/local/ssl/include \
	/usr/local/include \
	/usr/ssl/include \
        /usr/include \
	/usr/pkg/include"
	
      if test "x$ac_ssl_home" != xno && test "x$ac_ssl_home" != xyes ; then
        ssl_include_dirs="$ac_ssl_home $ac_ssl_home/include $ssl_include_dirs"
      fi
      
      for ssl_dir in $ssl_include_dirs; do
        if test -r "$ssl_dir/openssl/ssl.h"; then
          ac_ssl_includes="$ssl_dir"
          break
        fi
      done

      ac_cv_header_ssl="$ac_ssl_includes"
    ])
    
    fi
    
    AC_MSG_RESULT([$ac_ssl_includes])

    if test "x$ac_ssl_includes" = xno || test "x$ac_ssl_includes" = xyes; then
    AC_MSG_ERROR([

Please install the OpenSSL header files.  If you already did so, point this
script to the right directory with the --with-ssl-includes=DIR option.
])
    fi

    # gcc under solaris is often not installed correctly.  Avoid specifying
    # -I/usr/include.
    if test "x$ac_ssl_includes" = "x/usr/include" ; then
        SSLINC=""
    else
        SSLINC="-I$ac_ssl_includes"
    fi

    AC_DEFINE(SLRN_HAS_SSL_SUPPORT, 1)
  else
    AC_DEFINE(SLRN_HAS_SSL_SUPPORT, 0)
  fi
  
    AC_SUBST(SSLINC)
    AC_SUBST(SSLLIB)dnl
])

AC_DEFUN(CF_MTA,
[
  AC_ARG_WITH(mta,
    [  --with-mta[=PATHNAME]   To use an alternate mail transport agent],
    [  ac_mta_path="$withval" ], [ ac_mta_path=no ])
  
  if test "x$ac_mta_path" == xno || test "x$ac_mta_path" == xyes || \
     test "x$ac_mta_path" == "x"; then
    dnl We need to find sendmail ourself
    
    AC_PATH_PROG(SENDMAIL, sendmail, no,
      $PATH:/usr/local/sbin:/usr/sbin:/usr/local/lib:/usr/lib)
    if test "x$ac_cv_path_SENDMAIL" != xno; then
      AC_DEFINE_UNQUOTED(SLRN_SENDMAIL_COMMAND, "$ac_cv_path_SENDMAIL -oi -t -oem -odb")
    else
      AC_MSG_ERROR([

I can't find a sendmail executable.  slrn requires a mail transport agent
for sending e-mail.  Please make sure sendmail is in your \$PATH or use
the --with-mta option to point configure to a different MTA (giving the full
pathname and all needed command line arguments).
])
    fi
  else
    AC_DEFINE_UNQUOTED(SLRN_SENDMAIL_COMMAND, "$ac_mta_path")
  fi
])

AC_DEFUN(CF_UUDEVIEW,
[
  UULIB=""
  UUINC=""
  
  AC_ARG_WITH(uudeview,
    [  --with-uudeview[=DIR]   For uudeview library support],
    [  ac_uu_home="$withval" ], [ ac_uu_home=no ])

  if test "x$ac_uu_home" != xno ; then
  
    dnl We want uudeview support
  
    AC_MSG_CHECKING(for the uudeview library)
    
    if test "x$ac_cv_lib_uu" != "x" ; then
      ac_uu_library="$ac_cv_lib_uu"
    else
      ac_uu_library=no
    fi

    AC_CACHE_VAL(ac_cv_lib_uu, [
    
      dnl If you need to add extra directories to check, add them here.
      
      uu_library_dirs="\
	/usr/local/lib \
        /usr/lib \
	/usr/pkg/lib"
  	
      if test "x$ac_uu_home" != xno && test "x$ac_uu_home" != xyes ; then
        uu_library_dirs="$ac_uu_home $ac_uu_home/lib $uu_library_dirs"
      fi
  
      for uu_dir in $uu_library_dirs; do
        if test -r "$uu_dir/libuu.a" ; then
          ac_uu_library="$uu_dir"
          break
        fi
      done

      if test "x$ac_uu_library" != xno; then
        ac_cv_lib_uu="$ac_uu_library"
      fi
    ])
    
    AC_MSG_RESULT([$ac_uu_library])
    
    if test "x$ac_uu_library" = xno ; then
    AC_MSG_ERROR([

Please install the uudeview library.  If you already did so, point this script
to the right directory with the --with-uudeview=DIR option.
])
    fi
    
    # gcc under solaris is often not installed correctly.  Avoid specifying
    # -L/usr/lib.
    if test "x$ac_uu_library" = "x/usr/lib" ; then
        UULIB="-luu"
    else
        if test "x$enable_hardcode_libs" = "xyes" ; then
            UULIB="-L$ac_uu_library $cf_rpath_option$ac_uu_library -luu"
        else
            UULIB="-L$ac_uu_library -luu"
        fi
    fi
    
    AC_MSG_CHECKING(for the uudeview includes)
    
    if test "x$ac_cv_header_uu" != "x" ; then
      ac_uu_includes="$ac_cv_header_uu"
    else
      ac_uu_includes=no
    fi

    AC_CACHE_VAL(ac_cv_header_uu, [

      dnl If you need to add extra directories to check, add them here.
      
      uu_include_dirs="\
	/usr/local/include \
        /usr/include \
	/usr/pkg/include"
	
      if test "x$ac_uu_home" != xno && test "x$ac_uu_home" != xyes ; then
        uu_include_dirs="$ac_uu_home $ac_uu_home/include $uu_include_dirs"
      fi
      
      for uu_dir in $uu_include_dirs; do
        if test -r "$uu_dir/uudeview.h"; then
          ac_uu_includes="$uu_dir"
          break
        fi
      done

      if test "x$ac_uu_includes" != xno; then
        ac_cv_header_uu="$ac_uu_includes"
      fi
    ])
    
    AC_MSG_RESULT([$ac_uu_includes])

    if test "x$ac_uu_includes" = xno ; then
    AC_MSG_ERROR([

Please install the uudeview header files.  If you already did so, point this
script to the right directory with the --with-uudeview=DIR option.
])
    fi

    # gcc under solaris is often not installed correctly.  Avoid specifying
    # -I/usr/include.
    if test "x$ac_uu_includes" = "x/usr/include" ; then
        UUINC=""
    else
        UUINC="-I$ac_uu_includes"
    fi

    AC_DEFINE(SLRN_HAS_UUDEVIEW, 1)
  else
    AC_DEFINE(SLRN_HAS_UUDEVIEW, 0)
  fi
  
    AC_SUBST(UUINC)
    AC_SUBST(UULIB)dnl
])

AC_DEFUN(CF_VA_COPY,
[
 dnl va_copy checks taken from glib 1.2.8
 dnl
 dnl we currently check for all three va_copy possibilities, so we get
 dnl all results in config.log for bug reports.
AC_MSG_CHECKING(for an implementation of va_copy())
AC_CACHE_VAL(slrn_cv_va_copy,[
	AC_TRY_RUN([
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
	}],
	slrn_cv_va_copy=yes
	,
	slrn_cv_va_copy=no
	,)
])
AC_MSG_RESULT($slrn_cv_va_copy)
AC_MSG_CHECKING(for an implementation of __va_copy())
AC_CACHE_VAL(slrn_cv___va_copy,[
	AC_TRY_RUN([
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
	}],
	slrn_cv___va_copy=yes
	,
	slrn_cv___va_copy=no
	,)
])
AC_MSG_RESULT($slrn_cv___va_copy)
AC_MSG_CHECKING(whether va_lists can be copied by value)
AC_CACHE_VAL(slrn_cv_va_val_copy,[
	AC_TRY_RUN([
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
	}],
	slrn_cv_va_val_copy=yes
	,
	slrn_cv_va_val_copy=no
	,)
])
if test "x$slrn_cv_va_copy" = "xyes"; then
  AC_DEFINE(VA_COPY, va_copy)
else if test "x$slrn_cv___va_copy" = "xyes"; then
  AC_DEFINE(VA_COPY, __va_copy)
fi
fi
if test "x$slrn_cv_va_val_copy" = "xno"; then
  AC_DEFINE(VA_COPY_AS_ARRAY)
fi
AC_MSG_RESULT($slrn_cv_va_val_copy)
])

AC_DEFUN(CF_SUMMARY,
[
  AC_REQUIRE([CF_EVAL_VARS])

echo ""
echo "slrn will be installed to the following directories:"
echo "Binaries      : $ev_bindir"
echo "Documentation : $ev_docdir"
echo "Manual pages  : $ev_mandir"
echo "Macros        : ${ev_datadir}/slrn"
echo ""
echo "System-wide configuration is read from $ev_sysconfdir"

if test "x$enable_hardcode_libs" != xyes && test ! -x /sbin/ldconfig ; then
AC_MSG_WARN([
/sbin/ldconfig not found.
It\'s probably a good idea to specify --enable-hardcode-libs])
fi

dnl There seems to be (currently unused) free software for this ...

dnl if test "x$enable_grouplens" != x && test "x$enable_grouplens" != xno ; then
dnl AC_MSG_WARN([
dnl You enabled GroupLens support.
dnl 
dnl As far as I could find out, GroupLens is dead for years, so I consider
dnl removing the code from slrn.  Please contact me at tststs@gmx.de to let
dnl me know you still need it.])
dnl fi

echo ""
echo "Look at doc/INSTALL.unix for a list of configure options."
echo "For some less common compile time options, edit \"src/slrnfeat.h\"."
echo "Then, type \"make all\" to start compilation."
echo ""
])
