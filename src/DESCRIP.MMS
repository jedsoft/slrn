# -*- sh -*-
# Makefile for VMS version of SLRN
#
# Don't call this directly, but only via vmsmake.com

.IFDEF __MMK__
.ELSE
EXE = .EXE
OBJ = .OBJ
OLB = .OLB
OPT = .OPT
.ENDIF

SLANG_VERSION = 10003

OFILES = art$(OBJ), -
	 art_misc$(OBJ), -
	 chmap$(OBJ), -
	 decode$(OBJ), -
	 editscore$(OBJ), -
	 group$(OBJ), -
	 grplens$(OBJ)
	 hash$(OBJ), -
	 help$(OBJ), -
	 interp$(OBJ), -
	 menu$(OBJ), -
	 mime$(OBJ), -
	 misc$(OBJ), -
	 post$(OBJ), -
	 print$(OBJ), -
	 ranges$(OBJ), -
	 score$(OBJ), -
	 server$(OBJ), -
	 slrndir$(OBJ), -
	 slrn$(OBJ), -
	 snprintf$(OBJ), -
	 sortdate$(OBJ), -
	 startup$(OBJ), -
	 ttymsg$(OBJ), -
	 util$(OBJ), -
	 version$(OBJ), -
	 vfile$(OBJ), -
	 vmsmail$(OBJ), -
	 vms$(OBJ), -
	 xover$(OBJ)

ALL : slrn$(EXE)
	continue

slrn$(EXE) : slangversion $(OFILES)
    LINK/EXEC=$@ $(LFLAGS) slrn.opt/opt

slangversion : chkslang$(EXE)
	@ CHKSLANG := $SYS$DISK:[]CHKSLANG
	CHKSLANG slrn $(SLANG_VERSION)
	
chkslang$(EXE) : chkslang$(OBJ)
	$(LINK) /EXEC=$@ chkslang$(OBJ),aux.opt/opt

chkslang$(OBJ) : chkslang.c $(SLANGDIR)slang.h


slrn$(OBJ) : slrn.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, server.h, slrn.h, group.h, ranges.h, misc.h, - 
             startup.h, art.h, score.h

group$(OBJ) : group.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, - 
              jdmacros.h, slrn.h, group.h, ranges.h, art.h, misc.h, post.h, - 
              server.h, hash.h, score.h, menu.h

misc$(OBJ) : misc.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, - 
             jdmacros.h, misc.h, group.h, ranges.h, slrn.h, post.h, server.h, -
             mime.h
             
server$(OBJ) : server.c, slrnconf.h, slrnfeat.h, server.h, misc.h, -
               decode.h, clientlib.c, nntp.c

art$(OBJ) : art.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
            jdmacros.h, slrn.h, -
            group.h, ranges.h, server.h, art.h, misc.h, post.h, startup.h, -
            hash.h, score.h, decode.h, menu.h

post$(OBJ) : post.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, slrn.h, server.h, misc.h, post.h, group.h, ranges.h -
             art.h, decode.h

startup$(OBJ) : startup.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
                jdmacros.h, slrn.h, group.h, ranges.h, misc.h, art.h, post.h, - 
                startup.h, score.h, decode.h, server.h

help$(OBJ) : help.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, - 
             jdmacros.h, slrn.h, misc.h

hash$(OBJ) : hash.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, hash.h
             
vms$(OBJ) : vms.c, slrnconf.h, slrnfeat.h

score$(OBJ) : score.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, slrn.h, group.h, ranges.h, art.h, score.h, misc.h, -
             server.h, hash.h
             
vmsmail$(OBJ) : vmsmail.c, slrnconf.h, slrnfeat.h, vmsmail.h

mime$(OBJ) : mime.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, server.h, slrn.h, misc.h, group.h, ranges.h, art.h, -
             decode.h, mime.h
             
decode$(OBJ) : decode.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, slrn.h, misc.h, decode.h
             
menu$(OBJ) : menu.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, slrn.h, misc.h, menu.h    

interp$(OBJ) : interp.c, slrnconf.h, slrnfeat.h, $(SLANGDIR)slang.h, -
             jdmacros.h, slrn.h, group.h, ranges.h, art.h, misc.h, startup.h, - 
             server.h, menu.h, interp.h

grplens$(OBJ) : grplens.c, slrnfeat.h, $(SLANGDIR)slang.h, -
                jdmacros.h, slrn.h, group.h, ranges.h, -
                art.h, misc.h, decode.h, grplens.h sltcp.h util.h, -
		server.h

clean :
    delete *$(OBJ);*, $(TARGET);*, chkslang$(EXE);*
