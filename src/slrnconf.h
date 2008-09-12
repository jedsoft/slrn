#ifndef SLRN_NON_UNIX_CONFIG_H
#define SLRN_NON_UNIX_CONFIG_H
/* 
 * This file is for NON-Unix systems.
 * Use config.h (generated by configure) for Unix!!!
 * 
 * This file is used to indicate capabilities of the C compiler and
 * operating system.  Additionally, you can set optional features
 * here that are under autoconf control on Unix.
 * 
 * See also slrnfeat.h for the rest of slrn features.
 */

/* hostname of a default NNTP server to use */
/* #define NNTPSERVER_NAME	"my.server.name" */

/* file that contains the hostname of a default NNTP server */
/* #define NNTPSERVER_FILE	"/usr/local/lib/news/nntp_server" */

/* End of features section. See slrnfeat.h for the rest. */

/* Does your compiler support vsnprintf()? */
/* #define HAVE_VSNPRINTF 1 */

/* SLTCP_HAS_SSL_SUPPORT gets turned on whenever we use SSL
 * SLTCP_HAS_GNUTLS_SUPPORT gets turned on when we use GNU TLS for it. */
#define SLRN_HAS_SSL_SUPPORT	0
#define SLRN_HAS_GNUTLS_SUPPORT	0

#define SLTCP_HAS_SSL_SUPPORT (SLRN_HAS_SSL_SUPPORT || SLRN_HAS_GNUTLS_SUPPORT)
#define SLTCP_HAS_GNUTLS_SUPPORT SLRN_HAS_GNUTLS_SUPPORT
#define SLTCP_HAS_NSS_COMPAT	0
  
#if defined (__USE_SVID) || defined (__USE_XOPEN) || (defined (__MSVCRT__) && !defined (_NO_OLDNAMES))
# define HAVE_TIMEZONE 1
#endif

#ifdef __USE_BSD
# define HAVE_TM_GMTOFF 1
#endif

#ifdef VMS
# ifndef MAIL_PROTOCOL
#   if defined(UCX) || defined(MULTINET)
#     define MAIL_PROTOCOL "SMTP%"
#   else
#     define MAIL_PROTOCOL "IN%"
#   endif
# endif
#endif

#ifdef __GO32__
# ifdef REAL_UNIX_SYSTEM
#  undef REAL_UNIX_SYSTEM
# endif
# ifndef __DJGPP__
#  define __DJGPP__ 1
# endif
#endif

#if defined(__MSDOS__) || defined(__DOS__)
# ifndef __MSDOS__
#  define __MSDOS__
# endif
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#if defined(OS2) || defined(__OS2__)
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
# ifndef __os2__
#  define __os2__
# endif
#endif

#if defined(__CYGWIN32__) && !defined(__CYGWIN__)
# define __CYGWIN__
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__WATCOMC__)
# ifndef __WIN32__
#  define __WIN32__
# endif
#endif

#if defined(WIN32) || defined(__WIN32__)
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
# ifndef __WIN32__
#  define __WIN32__ 
# endif
#endif

#if defined(__MSDOS__) && !defined(__GO32__) && !defined(DOS386) && !defined(__WIN32__)
# ifndef __MSDOS_16BIT__
#  define __MSDOS_16BIT__	1
# endif
#endif

#if defined(__NT__)
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#if defined(IBMPC_SYSTEM) || defined (__DECC) || defined(VAXC)
# define HAVE_STDLIB_H 1
#else
# define HAVE_MALLOC_H 1
#endif

#if defined (__os2__)
# define HAVE_UNISTD_H 1
# define HAVE_MEMORY_H 1
# define HAVE_SYS_SOCKET_H 1
# define HAVE_NETINET_IN_H 1
# define HAVE_ARPA_INET_H 1
# define HAVE_DIRENT_H 1
# define HAVE_FCNTL_H 1
#endif

#if defined(__NT__) || defined(__WIN32__)
# ifndef __CYGWIN__
#  define HAVE_UNISTD_H 1
# endif
# define HAVE_MEMORY_H 1
# define HAVE_FCNTL_H 1
# define HAVE_LOCALE_H 1
# define HAVE_SETLOCALE 1
# if defined(__CYGWIN__) || defined(__MINGW32__)
#  define HAVE_DIRENT_H 1
# else
#  define HAVE_DIRECT_H 1
# endif
# define popen _popen
# define pclose _pclose
#endif

#if defined(__WIN32__)
# if !defined(__CYGWIN__) && !defined(__MINGW32__)
#  define HAVE_WINSPOOL_H
# endif
#endif

#ifdef VMS
# if __VMS_VER >= 60200000
#  define HAVE_UNISTD_H
# endif
# if __VMS_VER >= 70000000
#  define HAVE_GETTIMEOFDAY
# endif
#endif

/* Does VMS have these?  Do *all* VMS version have them? */
#ifndef VMS
# define HAVE_ISALPHA 1
# define HAVE_ISSPACE 1
# define HAVE_ISDIGIT 1
# define HAVE_ISALNUM 1
# define HAVE_ISPUNCT 1
#endif

#if defined(VMS) || defined(__os2__)
# define USE_NOTEPAD_PRINT_CODE
#endif

#ifdef __WIN32__
/* # define USE_NOTEPAD_PRINT_CODE */
#endif

/* 
 * Basic C library functions.
 */

#if defined(__os2__) || defined(__NT__)
# define HAVE_PUTENV 1
#endif

#define HAVE_GETCWD 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMCHR 1

/* undefine if your sprintf does not return an int */
#define HAVE_ANSI_SPRINTF 1

#define SLRN_SERVER_ID_NNTP 1
#define SLRN_SERVER_ID_SPOOL 2

#define SLRN_POST_ID_NNTP 1
#define SLRN_POST_ID_INEWS 2
#define SLRN_POST_ID_PULL 3

#if defined(IBMPC_SYSTEM) && !defined(__CYGWIN__)
# define SLRN_PATH_SLASH_CHAR	'\\'
#endif

#ifndef SLRN_PATH_SLASH_CHAR
/* I have no idea whether or not this will work on VMS. 
 * Using ']' is probably better. 
 */
# define SLRN_PATH_SLASH_CHAR	'/'
#endif

#if defined(__os2__)
/* Uncomment for no FAT code */
# define SLRN_USE_OS2_FAT
#endif

/* gettext is currently only supported on unix */
#define _(a) (a)
#define N_(a) a

#define SIZEOF_SHORT		2
#define SIZEOF_INT		4
#define SIZEOF_LONG		4
#define SIZEOF_FLOAT		4
#define SIZEOF_DOUBLE		8
#define SIZEOF_LONG_LONG	8

#undef HAVE_LONG_LONG
#undef HAVE_ATOLL
#undef HAVE_STRTOLL

#if defined(HAVE_LONG_LONG) && (SIZEOF_LONG < SIZEOF_LONG_LONG)
typedef long long NNTP_Artnum_Type;
# define NNTP_FMT_ARTNUM "%lld"
# define NNTP_FMT_ARTNUM_2 "%lld %lld"
# define NNTP_FMT_ARTNUM_3 "%lld %lld %lld"
# define NNTP_FMT_ARTRANGE "%lld-%lld"
# ifdef HAVE_ATOLL
#  define NNTP_STR_TO_ARTNUM(x) atoll(x)
# else
#  define NNTP_STR_TO_ARTNUM(x) strtoll((x),NULL,10)
# endif
# define NNTP_ARTNUM_TYPE_MAX 9223372036854775807LL
#else
typedef long NNTP_Artnum_Type;
# define NNTP_FMT_ARTNUM "%ld"
# define NNTP_FMT_ARTNUM_2 "%ld %ld"
# define NNTP_FMT_ARTNUM_3 "%ld %ld %ld"
# define NNTP_FMT_ARTRANGE "%ld-%ld"
# define NNTP_STR_TO_ARTNUM atol
# define NNTP_ARTNUM_TYPE_MAX LONG_MAX
#endif

#endif				       /* SLRN_NON_UNIX_CONFIG_H */
