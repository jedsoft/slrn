/* -*- C -*- */
/* Process this file through autoheader to generate config.h.in */

/* define if you want to force the use of inews */
#define SLRN_FORCE_INEWS		0

/* define if you want slrn to map between ISO-Latin and native charsets */
#define SLRN_HAS_CHARACTER_MAP		1

/* define if you want emphasized text support */
#define SLRN_HAS_EMPHASIZED_TEXT	1

/* define if slrn should use In-Reply-To if no References are available */
#define SLRN_HAS_FAKE_REFS		1

/* define if you want slrn to generate Message-IDs */
#define SLRN_HAS_GEN_MSGID		1

/* define if you want grouplens (R.I.P.) support */
#define SLRN_HAS_GROUPLENS		0

/* define if you want support for posting via inews */
#define SLRN_HAS_INEWS_SUPPORT		0

/* define if you want MIME support */
#define SLRN_HAS_MIME			1

/* define to make slrn cache Message-IDs to eliminate cross-posts */
#define SLRN_HAS_MSGID_CACHE		0

/* define if you want NNTP support */
#define SLRN_HAS_NNTP_SUPPORT		1

/* define if you want slrnpull support */
#define SLRN_HAS_PULL_SUPPORT		0

/* define if you want slang interpreter support */
#define SLRN_HAS_SLANG			1

/* define if you want spoiler support */
#define SLRN_HAS_SPOILERS		1

/* define if you want support for reading from a news spool */
#define SLRN_HAS_SPOOL_SUPPORT		0

/* define if you want SSL support */
#define SLRN_HAS_SSL_SUPPORT		0

/* define if you want to disallow custom From headers */
#define SLRN_HAS_STRICT_FROM		0

/* define if you want uudeview support */
#define SLRN_HAS_UUDEVIEW		0

/* inews command */
#undef SLRN_INEWS_COMMAND

/* directory for global slrn.rc and newsgroups.dsc */
#undef SLRN_LIB_DIR

/* sendmail command */
#undef SLRN_SENDMAIL_COMMAND

/* hostname of a default NNTP server to use */
#undef NNTPSERVER_NAME

/* file that contains the hostname of a default NNTP server */
#undef NNTPSERVER_FILE

/* Whether you want the "setgid" patch to work (docs/slrnpull/setgid.txt) */
#define SLRNPULL_USE_SETGID_POSTS	0

/* Define if NLS is requested */
#undef ENABLE_NLS

/* Define the directory where your locales are */
#undef LOCALEDIR

/* define if your sprintf returns an int (as required by ANSI C) */
#undef HAVE_ANSI_SPRINTF

/* define if you have a "timezone" variable in time.h */
#undef HAVE_TIMEZONE

/* define if you have "tm_gmtoff" in struct tm */
#undef HAVE_TM_GMTOFF

/* define if you have va_copy() in stdarg.h */
#undef VA_COPY

/* define if va_lists can't be copied by value */
#undef VA_COPY_AS_ARRAY
