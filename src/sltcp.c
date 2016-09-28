/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
 Copyright (c) 2001-2006  Thomas Schultz <tststs@gmx.de>

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "config.h"
#include "slrnfeat.h"

/*{{{ Include Files */

#include <stdio.h>
#include <string.h>

#include <stdlib.h>

#include <assert.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

#include <setjmp.h>
#include <signal.h>

#include <sys/types.h>

#ifdef HAVE_SOCKET_H
# include <socket.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#if defined(__NT__)
# include <winsock.h>
# define USE_WINSOCK_SLTCP	1
#else
# if defined(__WIN32__)
/* #  define Win32_Winsock */
#  define __USE_W32_SOCKETS
#  include <windows.h>
#  define USE_WINSOCK_SLTCP	1
# endif
#endif

#ifdef USE_WINSOCK_SLTCP
# define USE_WINSOCK_SLTCP	1
#else
# include <netdb.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if !defined(h_errno) && !defined(__CYGWIN__)
extern int h_errno;
#endif

/* For select system call */
#ifdef VMS
# include <unixio.h>
# include <socket.h>
# include <in.h>
# include <inet.h>
#else
# if !defined(USE_WINSOCK_SLTCP)
#  include <sys/time.h>
# endif
# if defined(__QNX__) || defined(__os2__)
#  include <sys/select.h>
# endif
# if defined (_AIX) && !defined (FD_SET)
#  include <sys/select.h>	/* for FD_ISSET, FD_SET, FD_ZERO */
# endif

# ifndef FD_SET
#  define FD_SET(fd, tthis) *(tthis) = 1 << (fd)
#  define FD_ZERO(tthis)    *(tthis) = 0
#  define FD_ISSET(fd, tthis) (*(tthis) & (1 << fd))
 typedef int fd_set;
# endif
#endif

#include <slang.h>

#if SLTCP_HAS_GNUTLS_SUPPORT
# include <gnutls/openssl.h>
#else
# if SLTCP_HAS_SSL_SUPPORT
#  if SLTCP_HAS_NSS_COMPAT
#   include <nss_compat_ossl.h>
#  else
#   include <openssl/ssl.h>
#   include <openssl/err.h>
#   include <openssl/rand.h>
#  endif
# endif
#endif

#include "sltcp.h"
#include "snprintf.h"
#include "util.h"

struct _SLTCP_Type
{
   int tcp_fd;
#define SLTCP_EOF_FLAG	0x1
   unsigned int tcp_flags;
   unsigned char *tcp_write_ptr;
   unsigned char *tcp_write_ptr_min;
   unsigned char *tcp_read_ptr_max;
   unsigned char *tcp_read_ptr;
#define SLTCP_BUF_SIZE 8192
   unsigned char tcp_read_buf [SLTCP_BUF_SIZE];
   unsigned char tcp_write_buf [SLTCP_BUF_SIZE];
   unsigned long bytes_out;
   unsigned long bytes_in;
#if SLTCP_HAS_SSL_SUPPORT
   SSL *ssl;
#endif
};

/*}}}*/

#if defined(__BEOS__) || defined(USE_WINSOCK_SLTCP)
# define SLTCP_CLOSE(x)		closesocket(x)
# define SLTCP_READ(x,y,z)	recv((x),(y),(z),0)
# define SLTCP_WRITE(x,y,z)	send((x),(y),(z),0)
#else
# define SLTCP_CLOSE(x)		close(x)
# define SLTCP_READ(x,y,z)	read((x),(y),(z))
# define SLTCP_WRITE(x,y,z)	write((x),(y),(z))
#endif

int (*SLTCP_Interrupt_Hook) (void);
int SLtcp_TimeOut_Secs = SLRN_SLTCP_TIMEOUT_SECS;

/* Turn debugging messages on / off. Note: I don't think these messages need
 * to get translated. */
static int TCP_Verbose_Reporting = 0;

void (*SLtcp_Verror_Hook) (char *, va_list);
static void print_error (char *fmt, ...) SLATTRIBUTE_PRINTF(1,2);
static void print_error (char *fmt, ...)
{
   va_list ap;

   va_start (ap, fmt);
   if (SLtcp_Verror_Hook != NULL)
     (*SLtcp_Verror_Hook) (fmt, ap);
   else
     {
	(void) vfprintf(stderr, fmt, ap);
	(void) fputc ('\n', stderr);
	fflush (stderr);
     }
   va_end (ap);
}

/* void (*SLtcp_Error_Hook) (char *, ...) SLATTRIBUTE_PRINTF(1,2); */

static int sys_call_interrupted_hook (void) /*{{{*/
{
   if (SLTCP_Interrupt_Hook == NULL)
     return 0;

   return (*SLTCP_Interrupt_Hook) ();
}

/*}}}*/

/* This function attempts to make a connection to a specified port on an
 * internet host.  It returns a socket descriptor upon success or -1
 * upon failure.
 */
static int get_tcp_socket_1 (char *host, int port) /*{{{*/
{
#ifdef HAVE_GETADDRINFO
   /* Be AF-independent, so IPv6 works as well */
   int fd;
   int connected = 0;
   struct addrinfo *res, *ai;
   int tries = 1;
   int r;
   char portstr[6]; /* To pass a port number as a string */

   /* We need to give a hint to getaddrinfo to get it to resolve a
    * numerical port number */
   struct addrinfo hint;

   hint.ai_flags = 0;
   hint.ai_family = PF_UNSPEC;
   hint.ai_socktype = SOCK_STREAM;
   hint.ai_protocol = IPPROTO_IP;
   hint.ai_addrlen = 0;
   hint.ai_addr = NULL;
   hint.ai_canonname = NULL;
   hint.ai_next = NULL;

   do {
      snprintf(portstr, 6, "%i", port);
      if ((r = getaddrinfo(host, portstr, &hint, &res)) != 0) {
	 if (TCP_Verbose_Reporting) {
	    print_error ("Error resolving %s (port %s): %s\n", host, portstr, gai_strerror(r));
	 }
	 if (r == EAI_AGAIN) {
	    slrn_sleep (1);
	 };
      }
   } while (r && (tries++ <= 3));

   if (r) {
      print_error (_("Failed to resolve %s\n"), host);
      return -1;
   }
   if (TCP_Verbose_Reporting) {
      print_error ("Successfully resolved %s\n", host);
   }

   ai = res;
   do { /* Loop over all the list of struct addrinfo returned and try
	 * to get a socket connection */
      if (TCP_Verbose_Reporting) {
	 struct sockaddr *a = ai->ai_addr;
# ifdef HAVE_GETNAMEINFO
	 static char buf[NI_MAXHOST];
	 int l;
	 if (a->sa_family == AF_INET) {
	    l = sizeof(struct sockaddr_in);
	    print_error ("Address family: AF_INET\n");
	 } else {
	    assert(a->sa_family == AF_INET6);
	    l = sizeof(struct sockaddr_in6);
	    print_error ("Address family: AF_INET6\n");
	 }
	 if (!getnameinfo(a, l, buf, NI_MAXHOST-1, NULL, 0, NI_NUMERICHOST)) {
	    print_error ("Will try with address %s", buf);
	 } else {
	    print_error ("getnameinfo failed: %s\n", strerror(errno));
	 }
# else
	 if (a->sa_family == AF_INET) {
	    print_error ("Address family: AF_INET\n");
	 } else {
	    assert(a->sa_family == AF_INET6);
	    print_error ("Address family: AF_INET6\n");
	 }
# endif /* HAVE_GETNAMEINFO */
      }

      if ((fd = socket(ai->ai_family, SOCK_STREAM, 0)) == -1) {
	 if (TCP_Verbose_Reporting) {
	    print_error ("Error creating socket: %s\n", strerror(errno));
	 }
      } else {
	 if (TCP_Verbose_Reporting) {
	    print_error ("Created socket; descriptor is = %i\n", fd);
	 }
	 if ((r = connect(fd, ai->ai_addr, ai->ai_addrlen)) == 0) {
	    if (TCP_Verbose_Reporting) {
	       print_error ("Successfully connected\n");
	    }
	    connected = 1;
	 } else {
	    if (TCP_Verbose_Reporting) {
	       print_error ("Error connecting: %i, %s\n", errno, strerror(errno));
	    }
	    (void) SLTCP_CLOSE (fd);
	 }
      }
      ai = ai->ai_next;
   } while ((!connected) && (ai != NULL));

   freeaddrinfo(res);

   if (!connected)
     print_error(_("Unable to make connection. Giving up.\n"));

   return connected ? fd : -1;
#else
   /* IPv4-only implementation */

   char **h_addr_list;
   /* h_addr_list is NULL terminated if h_addr is defined.  If h_addr
    * is not defined, h_addr is the only element in the list.  When
    * h_addr is defined, its value is h_addr_list[0].
    */
   int h_length;
   int h_addr_type;
   char *fake_h_addr_list[2];
   unsigned long fake_addr;
   struct sockaddr_in s_in;
   int s;
   int not_connected;

   /* If it does not look like a numerical address, use nameserver */
   if (!isdigit(*host) || (-1L == (long)(fake_addr = inet_addr (host))))
     {
	struct hostent *hp;
	unsigned int max_retries = 3;

	while (NULL == (hp = gethostbyname (host)))
	  {
# ifdef TRY_AGAIN
	     max_retries--;
	     if (max_retries && (h_errno == TRY_AGAIN))
	       {
		  slrn_sleep (1);
		  continue;
	       }
# endif
	     print_error(_("%s: Unknown host.\n"), host);
	     return -1;
	  }

# ifdef h_addr
	h_addr_list = hp->h_addr_list;
# else
	h_addr_list = fake_h_addr_list;
	h_addr_list [0] = hp->h_addr;
	h_addr_list [1] = NULL;
# endif
	h_length = hp->h_length;
	h_addr_type = hp->h_addrtype;
     }
   else
     {
	h_addr_list = fake_h_addr_list;
	h_addr_list [0] = (char *) &fake_addr;
	h_addr_list [1] = NULL;

	h_length = sizeof(struct in_addr);
	h_addr_type = AF_INET;
     }

   memset ((char *) &s_in, 0, sizeof(s_in));
   s_in.sin_family = h_addr_type;
   s_in.sin_port = htons((unsigned short) port);

   if (-1 == (s = socket (h_addr_type, SOCK_STREAM, 0)))
     {
	perror("socket");
	return -1;
     }

   not_connected = -1;

   while (not_connected
	  && (h_addr_list != NULL)
	  && (*h_addr_list != NULL))
     {
	char *this_host;

	memcpy ((char *) &s_in.sin_addr, *h_addr_list, h_length);

	this_host = (char *) inet_ntoa (s_in.sin_addr);

	if (TCP_Verbose_Reporting) print_error ("trying %s\n", this_host);

	not_connected = connect (s, (struct sockaddr *)&s_in, sizeof (s_in));

	if (not_connected == -1)
	  {
# ifdef EINTR
	     if (errno == EINTR) /* If interrupted, try again. */
	       {
		  if (0 == sys_call_interrupted_hook ())
		    continue;
	       }
# endif
	     print_error (_("connection to %s, port %d:"),
		      (char *) this_host, port);
	     perror ("");
	  }
	h_addr_list++;
     }

   if (not_connected)
     {
	print_error(_("Unable to make connection. Giving up.\n"));
	(void) SLTCP_CLOSE (s);
	return -1;
     }
   return s;
#endif /* ! HAVE_GETADDRINFO -> IPv4 only code*/
}

/*}}}*/

#if HAVE_SIGLONGJMP
/*{{{ get_tcp_socket routines with sigsetjmp/siglongjmp */

static void restore_sigint_handler (void (*f)(int), int call_it) /*{{{*/
{
   if (f == SIG_ERR)
     return;

   (void) SLsignal_intr (SIGINT, f);

   if (call_it)
     kill (getpid (), SIGINT);
}

/*}}}*/

static sigjmp_buf Sigint_Jmp_Buf;

static void (*Old_Sigint_Handler) (int);
static volatile int Jump_In_Progress;

static void sigint_handler (int sig) /*{{{*/
{
   (void) sig;

   if (Jump_In_Progress) return;
   Jump_In_Progress = 1;

   siglongjmp (Sigint_Jmp_Buf, 1);
}

/*}}}*/

static int get_tcp_socket (char *host, int port) /*{{{*/
{
   int fd;

   Old_Sigint_Handler = SIG_ERR;

   Jump_In_Progress = 1;	       /* dont allow jump yet */
   if (0 != sigsetjmp (Sigint_Jmp_Buf, 1))   /* save signal mask */
     {
	restore_sigint_handler (Old_Sigint_Handler, 1);
	sys_call_interrupted_hook ();
	return -1;
     }

   Old_Sigint_Handler = SLsignal_intr (SIGINT, sigint_handler);

   if ((Old_Sigint_Handler == SIG_IGN)
       || (Old_Sigint_Handler == SIG_DFL))
     {
	restore_sigint_handler (Old_Sigint_Handler, 0);
	Old_Sigint_Handler = SIG_ERR;
     }

   Jump_In_Progress = 0;	       /* now allow the jump */
   fd = get_tcp_socket_1 (host, port);
   Jump_In_Progress = 1;	       /* don't allow jump */

   restore_sigint_handler (Old_Sigint_Handler, 0);

   return fd;
}

/*}}}*/

/*}}}*/
#else
static int get_tcp_socket (char *host, int port) /*{{{*/
{
   return get_tcp_socket_1 (host, port);
}

/*}}}*/
#endif	   			       /* HAVE_SIGLONGJMP */

#if SLTCP_HAS_SSL_SUPPORT
static SSL_CTX *This_SSL_Ctx;
static void deinit_ssl (void)
{
   if (This_SSL_Ctx == NULL)
     return;
   SSL_CTX_free (This_SSL_Ctx);
   This_SSL_Ctx = NULL;
}

static void dump_ssl_error_0 (void)
{
   int err;

   while (0 != (err = ERR_get_error()))
     print_error ("%s\n", ERR_error_string(err, 0));
}

#if !SLTCP_HAS_NSS_COMPAT
static unsigned long Fast_Random;
static unsigned long fast_random (void)
{
   return (Fast_Random = Fast_Random * 69069U + 1013904243U);
}
#endif

static int init_ssl_random (void)
{
#if !SLTCP_HAS_NSS_COMPAT
   time_t t;
   pid_t pid;
   unsigned int count;

   if (RAND_status ())
     return 0;

   time (&t);
   pid = getpid ();
   RAND_seed ((unsigned char *) &pid, sizeof (time_t));
   RAND_seed ((unsigned char *) &t, sizeof (pid_t));

   RAND_bytes ((unsigned char *) &Fast_Random, sizeof(long));

   count = 0;
   while ((count < 10000)
	  && (0 == RAND_status ()))
     {
	unsigned long r = fast_random ();
	RAND_seed (&r, sizeof (unsigned long));
     }
#endif				       /* !SLTCP_HAS_NSS_COMPAT */
   if (RAND_status ())
     return 0;

   print_error (_("Unable to generate enough entropy\n"));
   return -1;
}

#if SLTCP_HAS_GNUTLS_SUPPORT
static void tls_log_func (int level, const char *str)
{
   print_error ("tls: level=%d: %s\n", level, str);
}
#endif

static SSL *alloc_ssl (void)
{
   SSL *ssl;

   if (This_SSL_Ctx == NULL)
     {
	SSL_CTX *c;

	SSLeay_add_ssl_algorithms ();
	c = SSL_CTX_new(SSLv23_client_method());
	/* c = SSL_CTX_new(SSLv3_client_method()); */
	SSL_load_error_strings ();
	if (c == NULL)
	  {
	     dump_ssl_error_0 ();
	     print_error (_("SSL_CTX_new failed.\n"));
	     return NULL;
	  }

#if !SLTCP_HAS_GNUTLS_SUPPORT
	/* SSLv3 is vulnerable to POODLE attacks.  Do not use it. */
	SSL_CTX_set_options (c, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);
#endif
	This_SSL_Ctx = c;
	atexit (deinit_ssl);

	if (-1 == init_ssl_random ())
	  return NULL;
     }

#if SLTCP_HAS_GNUTLS_SUPPORT
   gnutls_global_set_log_function(tls_log_func);
   gnutls_global_set_log_level(0);
#endif
   ssl = SSL_new (This_SSL_Ctx);
   if (ssl != NULL)
     return ssl;

   dump_ssl_error_0 ();
   print_error (_("SSL_new failed\n"));
   return NULL;
}

static void dealloc_ssl (SSL *ssl)
{
   if (ssl != NULL)
     SSL_free (ssl);
}

static void dump_ssl_error (SSL *ssl, char *msg, int status)
{
   switch (SSL_get_error (ssl, status))
     {
      case SSL_ERROR_NONE:
	break;
      case SSL_ERROR_ZERO_RETURN:
	print_error (_("Unexpected error: SSL connection closed\n"));
	break;
      case SSL_ERROR_SYSCALL:
	print_error (_("System call failed: errno = %d\n"), errno);
	break;
      case SSL_ERROR_SSL:
	print_error (_("Possible protocol error\n"));
	break;
     }
   dump_ssl_error_0 ();
   if (msg != NULL) print_error (_("%s failed\n"), msg);
}
#endif				       /* SLTCP_HAS_SSL_SUPPORT */

SLTCP_Type *sltcp_open_connection (char *host, int port, int use_ssl) /*{{{*/
{
   int fd;
   SLTCP_Type *tcp;
#if SLTCP_HAS_SSL_SUPPORT
   SSL *ssl = NULL;
   int status;
#endif

   if (use_ssl)
     {
#if SLTCP_HAS_SSL_SUPPORT
	if (NULL == (ssl = alloc_ssl ()))
	  return NULL;
#else
	print_error (_("\n\n*** This program does not support SSL\n"));
	return NULL;
#endif
     }

   tcp = (SLTCP_Type *) SLMALLOC (sizeof (SLTCP_Type));
   if (tcp == NULL)
     {
	print_error (_("Memory Allocation Failure.\n"));
#if SLTCP_HAS_SSL_SUPPORT
	dealloc_ssl (ssl);
#endif
	return NULL;
     }
   memset ((char *) tcp, 0, sizeof (SLTCP_Type));

   tcp->tcp_fd = -1;
   tcp->tcp_read_ptr = tcp->tcp_read_ptr_max = tcp->tcp_read_buf;
   tcp->tcp_write_ptr = tcp->tcp_write_ptr_min = tcp->tcp_write_buf;

   fd = get_tcp_socket (host, port);
   if (fd == -1)
     {
	SLFREE (tcp);
#if SLTCP_HAS_SSL_SUPPORT
	dealloc_ssl (ssl);
#endif
	return NULL;
     }

   tcp->tcp_fd = fd;

#if SLTCP_HAS_SSL_SUPPORT
   if (ssl == NULL)
     return tcp;
   tcp->ssl = ssl;

   if (1 != SSL_set_fd (ssl, fd))
     {
	/* Yuk.  SSL_set_fd returns 0 upon failure */
	print_error (_("SSL_set_fd failed\n"));
	sltcp_close (tcp);
	return NULL;
     }

   status = SSL_connect (ssl);
   if (status == 1)
     return tcp;

   dump_ssl_error (ssl, _("SSL_connect"), status);
   sltcp_close (tcp);
   return NULL;
#else
   return tcp;
#endif
}

/*}}}*/

static int check_errno (void)
{
#ifdef EAGAIN
   if (errno == EAGAIN)
     {
	slrn_sleep (1);
	return 0;
     }
#endif
#ifdef EWOULDBLOCK
   if (errno == EWOULDBLOCK)
     {
	slrn_sleep (1);
	return 0;
     }
#endif
#ifdef EINTR
   if (errno == EINTR)
     {
	if (-1 == sys_call_interrupted_hook ())
	  return -1;
	return 0;
     }
#endif
   return -1;
}

#if SLTCP_HAS_SSL_SUPPORT
static unsigned int do_ssl_write (SLTCP_Type *tcp, unsigned char *buf, unsigned int len)
{
   unsigned int total;
   SSL *ssl = tcp->ssl;

   total = 0;
   ssl = tcp->ssl;

   while (total != len)
     {
	int nwrite;

	errno = 0;
	nwrite = SSL_write (ssl, (char *) buf, (len - total));

	if (nwrite > 0)
	  {
	     total += (unsigned int) nwrite;
	     tcp->bytes_out += total;
	     continue;
	  }

	switch (SSL_get_error (ssl, nwrite))
	  {
	   default:
	   case SSL_ERROR_ZERO_RETURN: /* Connection closed */
	     return total;

	   case SSL_ERROR_WANT_READ:
	   case SSL_ERROR_WANT_WRITE:
	     if (-1 == sys_call_interrupted_hook ())
	       return total;
	     break;		       /* try again */

	   case SSL_ERROR_SYSCALL:
	     if (-1 == check_errno ())
	       return total;
	     break;
	  }
     }

   return total;
}
#endif

static unsigned int do_write (SLTCP_Type *tcp, unsigned char *buf, unsigned int len) /*{{{*/
{
   unsigned int total;
   int nwrite;
   int fd;

#if SLTCP_HAS_SSL_SUPPORT
   if (tcp->ssl != NULL)
     return do_ssl_write (tcp, buf, len);
#endif
   total = 0;
   fd = tcp->tcp_fd;

   while (total != len)
     {
	nwrite = SLTCP_WRITE (fd, (char *) buf, (len - total));

	if (nwrite == -1)
	  {
	     if (-1 == check_errno ())
	       break;
	     continue;
	  }

	total += (unsigned int) nwrite;
     }

   tcp->bytes_out += total;

   return total;
}

/*}}}*/

int sltcp_flush_output (SLTCP_Type *tcp) /*{{{*/
{
   int fd;
   unsigned char *buf;
   unsigned int nwrite;
   unsigned int n;

   errno = 0;

   if ((tcp == NULL) || (-1 == (fd = tcp->tcp_fd)))
     return -1;

   buf = tcp->tcp_write_ptr_min;
   n = (unsigned int) (tcp->tcp_write_ptr - buf);

   nwrite = do_write (tcp, buf, n);

   if (nwrite != n)
     {
	tcp->tcp_write_ptr_min += nwrite;
	return -1;
     }

   tcp->tcp_write_ptr_min = tcp->tcp_write_ptr = tcp->tcp_write_buf;
   return 0;
}

/*}}}*/

int sltcp_close_socket (SLTCP_Type *tcp)
{
   int status;

   if ((tcp == NULL)
       || (tcp->tcp_fd == -1))
     return 0;
#if SLTCP_HAS_SSL_SUPPORT
   dealloc_ssl (tcp->ssl);
   tcp->ssl = NULL;
#endif
   status = 0;
   while (-1 == SLTCP_CLOSE (tcp->tcp_fd))
     {
	if (errno != EINTR)
	  {
	     status = -1;
	     break;
	  }

	if (-1 == sys_call_interrupted_hook ())
	  return -1;
     }
   tcp->tcp_write_ptr = tcp->tcp_write_ptr_min = tcp->tcp_write_buf;
   tcp->tcp_read_ptr_max = tcp->tcp_read_ptr = tcp->tcp_read_buf;
   tcp->tcp_fd = -1;
   return status;
}

int sltcp_close (SLTCP_Type *tcp) /*{{{*/
{
   errno = 0;

   if (tcp == NULL) return -1;

   if (-1 != tcp->tcp_fd)
     {
	if (-1 == sltcp_flush_output (tcp))
	  return -1;

	if (-1 == sltcp_close_socket (tcp))
	  return -1;
     }

   SLFREE (tcp);
   return 0;
}

/*}}}*/

#if !defined(VMS) && !defined(USE_WINSOCK_SLTCP)
static int wait_for_input (SLTCP_Type *tcp) /*{{{*/
{
   fd_set fds;
   struct timeval tv;
   int fd = tcp->tcp_fd;

   if (fd == -1)
     return -1;

#if SLTCP_HAS_SSL_SUPPORT
   if (tcp->ssl != NULL && SSL_pending(tcp->ssl) > 0)
     return 0;
#endif

   while (1)
     {
	int ret;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = SLtcp_TimeOut_Secs;
	tv.tv_usec = 0;

	ret = select(fd + 1, &fds, NULL, NULL, &tv);

	if (ret > 0)
	  return 0;

	if (ret == 0)
	  return -1;		       /* timed out */

	if (errno == EINTR)
	  {
	     if (-1 == sys_call_interrupted_hook ())
	       return -1;

	     continue;
	  }

	return -1;
     }
}
/*}}}*/
#endif

#if SLTCP_HAS_SSL_SUPPORT
static int do_ssl_read (SSL *ssl, char *buf, unsigned int len)
{
   int nread;
   while ((nread = SSL_read (ssl, buf, len)) <= 0)
     {
	switch (SSL_get_error (ssl, nread))
	  {
	   default:
	   case SSL_ERROR_ZERO_RETURN: /* Connection closed */
	     return -1;

	   case SSL_ERROR_WANT_READ:
	   case SSL_ERROR_WANT_WRITE:
	     if (-1 == sys_call_interrupted_hook ())
	       return -1;
	     break;

	   case SSL_ERROR_SYSCALL:
	     if (-1 == check_errno ())
	       return -1;
	     break;
	  }

     }

   return nread;
}
#endif
static int do_read (SLTCP_Type *tcp) /*{{{*/
{
   int nread, fd;

   if (tcp->tcp_flags & SLTCP_EOF_FLAG)
     return -1;

   fd = tcp->tcp_fd;

#if !defined(VMS) && !defined(USE_WINSOCK_SLTCP)
   /* The wait_for_input function call is probably not necessary.  The reason
    * that I am using it is that select will return EINTR if interrupted by
    * a signal and most implementations will not restart it.  In any case,
    * slrn attempts to set signals to NOT restart system calls.  In such a
    * case, the read below can be interrupted.
    */
   if (-1 == wait_for_input (tcp))
     return -1;
#endif

#if SLTCP_HAS_SSL_SUPPORT
   if (tcp->ssl != NULL)
     nread = do_ssl_read (tcp->ssl, (char *)tcp->tcp_read_ptr, SLTCP_BUF_SIZE);
   else
#endif
     while (-1 == (nread = SLTCP_READ (fd, (char *)tcp->tcp_read_ptr, SLTCP_BUF_SIZE)))
       {
	  if (-1 == check_errno ())
	    return -1;
       }

   if (nread == 0)
     tcp->tcp_flags |= SLTCP_EOF_FLAG;

   tcp->tcp_read_ptr_max += (unsigned int) nread;
   tcp->bytes_in += (unsigned int) nread;

   return nread;
}

/*}}}*/

unsigned int sltcp_read (SLTCP_Type *tcp, char *s, unsigned int len) /*{{{*/
{
   int fd;
   unsigned char *buf, *b;
   unsigned int blen;
   unsigned int total_len;

   if ((tcp == NULL) || (-1 == (fd = tcp->tcp_fd)))
     return 0;

   total_len = 0;

   while (1)
     {
	buf = tcp->tcp_read_ptr;
	b = tcp->tcp_read_ptr_max;

	blen = (unsigned int) (b - buf);
	if (blen >= len)
	  {
	     memcpy (s, (char *) buf, len);
	     tcp->tcp_read_ptr += len;
	     total_len += len;
	     return total_len;
	  }

	if (blen)
	  {
	     memcpy (s, (char *) buf, blen);
	     total_len += blen;
	     s += blen;
	     len -= blen;
	  }

	tcp->tcp_read_ptr = tcp->tcp_read_ptr_max = tcp->tcp_read_buf;

	if (-1 == do_read (tcp))
	  return total_len;
     }
}

/*}}}*/

unsigned int sltcp_write (SLTCP_Type *tcp, char *s, unsigned int len) /*{{{*/
{
   int fd;
   unsigned int blen;
   unsigned char *b;

   if ((tcp == NULL) || (-1 == (fd = tcp->tcp_fd)))
     return 0;

   b = tcp->tcp_write_ptr;
   blen = (unsigned int) ((tcp->tcp_write_buf + SLTCP_BUF_SIZE) - b);
   if (len <= blen)
     {
	memcpy ((char *) b, s, len);
	tcp->tcp_write_ptr += len;

	return len;
     }

   if (-1 == sltcp_flush_output (tcp))
     return 0;

   return do_write (tcp, (unsigned char *) s, len);
}

/*}}}*/

int sltcp_map_service_to_port (char *service) /*{{{*/
{
   struct servent *sv;

   sv = getservbyname (service, "tcp");
   if (sv == NULL) return -1;

   return (int) ntohs (sv->s_port);
}

/*}}}*/

int sltcp_fputs (SLTCP_Type *tcp, char *s) /*{{{*/
{
   unsigned int len;

   if (s == NULL)
     return -1;

   len = strlen (s);
   if (len != sltcp_write (tcp, s, len))
     return -1;

   return 0;
}

/*}}}*/

int sltcp_vfprintf (SLTCP_Type *tcp, char *fmt, va_list ap) /*{{{*/
{
   char buf [SLTCP_BUF_SIZE];

   if ((tcp == NULL) || (-1 == tcp->tcp_fd))
     return -1;

   (void) SLvsnprintf (buf, sizeof (buf), fmt, ap);

   return sltcp_fputs (tcp, buf);
}

/*}}}*/

int sltcp_fgets (SLTCP_Type *tcp, char *buf, unsigned int len) /*{{{*/
{
   unsigned char *r, *rmax;
   char *buf_max;
   int fd;

   if ((tcp == NULL) || (-1 == (fd = tcp->tcp_fd)) || (len == 0))
     return -1;

   buf_max = buf + (len - 1);	       /* allow room for \0 */

   while (1)
     {
	r = tcp->tcp_read_ptr;
	rmax = tcp->tcp_read_ptr_max;

	while (r < rmax)
	  {
	     unsigned char ch;

	     if (buf == buf_max)
	       {
		  *buf = 0;
		  tcp->tcp_read_ptr = r;
		  return 0;
	       }

	     ch = *r++;
	     *buf++ = ch;

	     if (ch == '\n')
	       {
		  *buf = 0;
		  tcp->tcp_read_ptr = r;
		  return 0;
	       }
	  }

	tcp->tcp_read_ptr_max = tcp->tcp_read_ptr = tcp->tcp_read_buf;

	if (-1 == do_read (tcp))
	  return -1;
     }
}

/*}}}*/

/* Before any of the above routines in this file may be used, sltcp_open_sltcp
 * must be called.
 */
#if defined(USE_WINSOCK_SLTCP)
static int Winsock_Started = 0;
static void w32_close_sltcp (void)
{
   (void) sltcp_close_sltcp ();
}
#endif

int sltcp_open_sltcp (void)
{
#if defined(USE_WINSOCK_SLTCP)
   WSADATA wsaData;
   WORD wVerReq = MAKEWORD(1, 1);      /* version 1.1, for 2.0, use (2,0) */

   if (Winsock_Started)
     return 0;

   if (WSAStartup(wVerReq, &wsaData) != 0)
     return -1;

   Winsock_Started = 1;
   atexit (w32_close_sltcp);
#endif
   return 0;
}

int sltcp_close_sltcp (void)
{
#if defined(USE_WINSOCK_SLTCP)
   if (Winsock_Started == 0)
     return 0;
   WSACancelBlockingCall();	       /* cancel any blocking calls */
   WSACleanup ();
   Winsock_Started = 0;
#endif

   return 0;
}

unsigned int sltcp_get_num_input_bytes (SLTCP_Type *tcp)
{
   if (tcp == NULL)
     return 0;
   return tcp->bytes_in;
}

unsigned int sltcp_get_num_output_bytes (SLTCP_Type *tcp)
{
   if (tcp == NULL)
     return 0;
   return tcp->bytes_out;
}

int sltcp_reset_num_io_bytes (SLTCP_Type *tcp)
{
   if (tcp == NULL)
     return -1;
   tcp->bytes_in = tcp->bytes_out = 0;
   return 0;
}

int sltcp_get_fd (SLTCP_Type *tcp)
{
   if (tcp == NULL)
     return -1;
   return tcp->tcp_fd;
}

#if 0 && defined(USE_WINSOCK_SLTCP)
/* Some winsock notes */

/* To interrupt a read/write on a socket, it may be useful to set a handler
 * control C, e.g., define something like
 *
 *    SetConsoleCtrlHandler (kbd_int_handler, TRUE);
 *
 * where:
 */
static BOOL WINAPI kbd_int_handler (DWORD type)
{
   BOOL ret = TRUE;

   switch (type)
     {
      case CTRL_C_EVENT:                     /* ctrl+c hit */
      case CTRL_BREAK_EVENT:                 /* ctrl+break hit */
	ret = FALSE;
	break;
      case CTRL_CLOSE_EVENT:                 /* window closing */
      case CTRL_LOGOFF_EVENT:                /* user logoff */
      case CTRL_SHUTDOWN_EVENT:              /*system shutdown */
      default:
	ret = TRUE;
	break;
     }
   (void) w32_close_sltcp ();
   return ret;
}

#endif
