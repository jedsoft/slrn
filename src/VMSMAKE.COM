$ SLRN_VERSION = "0.9.8.0"
$ SLANG_VERSION =  10003
$! Modified VMSMAKE for SLRN - Andy Harper, Kings College London
$!                             Martin Zinser, GSI Darmstadt
$! MPJZ - Create temp.mms from FDL to avoid file type problems  [21-DEC-1996]
$! AH   - Modified default for SLRN_LIB_DIR			[20-DEC-1996]
$! MPJZ - P3 to enforce link only with DCL build                [04-NOV-1996]
$! MPJZ - Automatic MMS/MMK detection                           [19-OCT-1996]
$! MPJZ - New options for slrn_lib_dir location and debug       [19-OCT-1996]
$! AH   - Use logicals to define SLANG location, if defined	[16-MAY-1996]
$! AH   - A few bug fixes!					[16-MAY-1996]
$! AH   - Auto-detect TCP stack and C compiler type	        [14-NOV-1995]
$! AH   - Override auto-selection of TCP stack via P1	        [14-NOV-1995]
$!
$! Parameters
$! 
$! P1 : Enforce TCP stack (Multinet, UCX, Netlib)
$! P2 : Location of slrn_lib_dir
$! P3 : D = Build debug image
$!      L = For DCL build only perform a link
$!
$ SLANGLIB = "[-.-.slang.src]"
$ SLANGOBJ = SLANGLIB + "SLANG.OLB"				! AH 16-MAY-1996
$
$ Make = ""
$!
$! If logical names defining the location of the slang libraries and header
$! files exist, use them in preference to the above definition of SLANGLIB
$!	-  SLANG_ROOT:[SRC] = location of header files (slang.h)
$!	-  SLANG_LIBRARY    = (if defined) location of SLANG.OLB
$!			      (if not defined) SLANG_ROOT:[SRC]SLANG.OLB used
$
$ if f$trnlnm("SLANG_ROOT") .nes. ""			! AH 16-MAY-1996
$   then						! AH 16-MAY-1996
$     SLANGLIB := "SLANG_ROOT:[SRC]"			! AH 16-MAY-1996
$     if f$trnlnm("SLANG_LIBRARY") .nes. ""		! AH 16-MAY-1996
$       then						! AH 16-MAY-1996
$         SLANGOBJ = "SLANG_LIBRARY"			! AH 16-MAY-1996
$       else						! AH 16-MAY-1996
$         SLANGOBJ = SLANGLIB  +  "SLANG.OLB"		! AH 16-MAY-1996
$     endif						! AH 16-MAY-1996
$ endif							! AH 16-MAY-1996
$!
$!      Build the option-file
$!
$ open/write optf slrn.opt
$ open/write aux  aux.opt
$ write optf "art.obj"
$ write optf "chmap.obj"
$ write optf "editscore.obj"
$ write optf "group.obj"
$ write optf "hash.obj"
$ write optf "help.obj"
$ write optf "interp.obj"
$ write optf "menu.obj"
$ write optf "mime.obj"
$ write optf "misc.obj"
$ write optf "post.obj"
$ write optf "ranges.obj"
$ write optf "score.obj"
$ write optf "server.obj"
$ write optf "sortdate.obj"
$ write optf "slrn.obj"
$ write optf "startup.obj"
$ write optf "ttymsg.obj"
$ write optf "util.obj"
$ write optf "decode.obj"
$ write optf "vms.obj"
$ write optf "vmsmail.obj"
$ write optf "xover.obj"
$ write optf "version.obj"
$ write optf "art_misc.obj"
$ write optf "vfile.obj"
$ write optf "snprintf.obj"
$ write optf "print.obj"
$ write optf "slrndir.obj"
$ write optf "grplens.obj"
$ write optf "Identification=""Slrn ''SLRN_VERSION'"""
$ write optf "''SLANGOBJ'" + "/LIBR"
$!
$!
$! Determine TCP stack availability (NOTE - order of test is important!)
$ TCP = "NONE"
$ if f$trnlnm("UCX$INET_HOST","LNM$SYSTEM") .nes. "" then $ TCP = "UCX"
$ if f$trnlnm("MULTINET",     "LNM$SYSTEM") .nes. "" then $ TCP = "MULTINET"
$ if f$trnlnm("NETLIB_DIR",   "LNM$SYSTEM") .nes. "" then $ TCP = "NETLIB"
$!
$ if P1 .nes. "" then $ TCP = P1	! Override via user parameter
$!
$! Have a look for slrn_lib_dir location
$!
$ if P2 .nes. "" 
$ then
$   SLRNLIB = "''p2'"
$ else
$!   SLRNLIB = "LIB$:[NEWS]"
$   SLRNLIB = "SLRN_LIB_DIR:"					! [AH 20-DEC-1996]
$ endif
$!
$! Check if we should build a debug version
$!
$ if f$locate("D",P3) .ne. f$length(p3)
$ then
$   CDEB = "/noopt/deb"
$   LFLAGS = "/debug"
$ else
$   CDEB = ""
$   LFLAGS = "/nodeb"
$ endif
$!
$! Check for MMK/MMS
$ If F$Search ("Sys$System:MMS.EXE") .nes. "" Then Make = "MMS"
$ If F$Type (MMK) .eqs. "STRING" Then Make = "MMK"
$!
$! Determine which C compiler
$ if f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. ""
$   then ! DEC C
$     COMPILER = "DEC C"
$     if (TCP .nes. "MULTINET")
$       then
$       PREFIX := "/PREFIX=ALL"
$     else
$       PREFIX := "/PREF=(ALL,EXC=(SOCKET,CONNECT,BIND,LISTEN,SOCKET_READ,SOCKET_WRITE,SOCKET_CLOSE,SELECT,ACCEPT,BCMP,BCOPY,BZERO,GETHOSTBYNAME,GETHOSTBYADDR,GETPEERNAME,GETDTABLESIZE,HTONS,HTONL,NTOHS,NTOHL,SEND,SENDTO,RECV,RECVFROM))"
$     endif
$     CCOPT = ""
$   else ! VAX C
$     COMPILER = "VAX C"
$     PREFIX := ""
$     write optf "sys$share:vaxcrtl.exe/share"
$     write aux "sys$share:vaxcrtl.exe/share"
$     if f$trnlnm("SYS").eqs."" then DEFINE SYS SYS$LIBRARY
$ endif
$ write sys$output "%VMSMAKE-I-CC, Building with ", COMPILER
$!
$ if (TCP .eqs. "UCX")
$   then
$    if (COMPILER .eqs. "VAX C") then write optf "sys$share:ucx$ipc/libr"
$ else if (TCP .eqs. "MULTINET")
$        then
$          write optf "multinet:multinet_socket_library/share"
$   else if (TCP .eqs. "NETLIB")
$          then
$            write optf "netlib_shrxfr/share"
$        else        ! Either default of NONE or unknown P1 parameter
$          write sys$error "%VMSMAKE-E-NONETWORK, Cannot determine which TCP stack you have"
$          exit
$        endif
$      endif
$ endif
$ write sys$output "%VMSMAKE-I-TCPFOUND, Building with the ", TCP, " network interface"
$!
$!
$!
$ close optf
$ close aux
$!
$!
$! Define key macros and other flags
$ DEFS := "''TCP',SLRN_LIB_DIR=""""""""""""""""''SLRNLIB'"""""""""""""""""
$ if (Make .nes. "")
$   then
$    open/write tfdl temp.fdl
$    write tfdl "FILE"
$    write tfdl "	NAME                    ""TEMP.MMS"""
$    write tfdl "	ORGANIZATION            sequential"
$    write tfdl ""
$    write tfdl "RECORD"
$    write tfdl "	BLOCK_SPAN              yes"
$    write tfdl "	CARRIAGE_CONTROL        carriage_return"
$    write tfdl "	FORMAT                  stream_lf"
$    close tfdl
$    create/fdl=temp.fdl 
$    open/append tmms temp.mms
$    write tmms "CPREF = ''PREFIX'"
$    write tmms "CFLAGS = /INC=(''SLANGLIB')/DEF=(''DEFS')$(CPREF) ''CDEB'"
$    write tmms "SLANGDIR = ''SLANGLIB'"
$    close tmms
$   else
$    CFLAGS := "/INC=(''SLANGLIB')/DEF=(''DEFS')''PREFIX' ''CDEB'" 
$ endif
$! Build
$ copy slrnconf.h config.h
$ purge config.h
$!
$! Compile the program
$!
$ if (Make .nes. "")
$   then
$    copy temp.mms,descrip.mms temp.mms
$    'Make'/des=temp.mms
$ else
$   if f$locate("L",p3).ne.f$length(p3) then goto link
$   CC/INCLUDE=('SLANGLIB') CHKSLANG.C
$   LINK/EXEC=CHKSLANG.EXE CHKSLANG.OBJ,aux.opt/opt
$   CHKSLANG := $SYS$DISK:[]CHKSLANG
$   CHKSLANG SLRN 'SLANG_VERSION'
$   v = f$verify(1)
$   CC 'CFLAGS' ART.C
$   CC 'CFLAGS' CHMAP.C
$   CC 'CFLAGS' EDITSCORE.C
$   CC 'CFLAGS' GROUP.C
$   CC 'CFLAGS' HASH.C
$   CC 'CFLAGS' HELP.C
$   CC 'CFLAGS' INTERP.C
$   CC 'CFLAGS' MENU.C
$   CC 'CFLAGS' MIME.C
$   CC 'CFLAGS' MISC.C
$   CC 'CFLAGS' POST.C
$   CC 'CFLAGS' RANGES.C
$   CC 'CFLAGS' SCORE.C
$   CC 'CFLAGS' SERVER.C
$   CC 'CFLAGS' SLRN.C
$   CC 'CFLAGS' SORTDATE.C
$   CC 'CFLAGS' STARTUP.C
$   CC 'CFLAGS' TTYMSG.C
$   CC 'CFLAGS' VERSION.C
$   CC 'CFLAGS' PRINT.C
$   CC 'CFLAGS' ART_MISC.C
$   CC 'CFLAGS' VFILE.C
$   CC 'CFLAGS' SNPRINTF.C
$   CC 'CFLAGS' UTIL.C
$   CC 'CFLAGS' DECODE.C		  			! AH 24-MAY-1996
$   CC 'CFLAGS' VMS.C
$   CC 'CFLAGS' VMSMAIL.C
$   CC 'CFLAGS' XOVER.C
$   CC 'CFLAGS' SLRNDIR.C
$   CC 'CFLAGS' GRPLENS.C
$ LINK:
$   LINK/EXEC=SLRN.EXE 'LFLAGS' slrn.opt/opt                  	! AH 16-MAY-1996
$   v=f$verify(v)
$ endif
$ purge *.opt
$ purge *.mms
$ purge *.fdl
$ exit
