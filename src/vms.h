#ifdef __DECC
# include <unixio.h>
# include <unixlib.h>
#endif

#include <signal.h>
#include <dvidef.h>
#include <descrip.h>
#include <ssdef.h>

#include <stat.h>
#include <types.h>
#include <socket.h>

#include <lib$routines.h>

#ifdef __GNUC__
# include <sys$routines.h>
#else
# include <starlet.h>
#endif

extern char *slrn_vms_fix_fullname (char *);
extern char *slrn_vms_get_uaf_fullname (void);
extern char *slrn_vms_getlogin (void);
extern int vms_send_mail(char *, char *, char *);

#if __VMS_VER < 70000000
extern int pclose(FILE *);
extern FILE *popen(char *, char *);
#endif

