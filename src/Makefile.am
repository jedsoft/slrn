## Process this file with automake to produce Makefile.in -*- sh -*-

EXTRA_PROGRAMS = chkslang slrnpull uudecode
SLRNPULL = @SLRNPULL@
bin_PROGRAMS = slangversion slrn $(SLRNPULL)

slrn_SOURCES = art.c chmap.c editscore.c group.c grplens.c hash.c help.c \
 hooks.c interp.c menu.c mime.c misc.c post.c score.c server.c sortdate.c \
 slrn.c startup.c ttymsg.c util.c decode.c xover.c slrndir.c version.c \
 print.c art_misc.c vfile.c snprintf.c art_sort.c ranges.c

slrnpull_SOURCES = nntplib.c slrndir.c slrnpull.c snprintf.c ttymsg.c util.c \
 ranges.c vfile.c
slrnpull_DEPENDENCIES = stamp2

DEFS=-Dunix -DSLRN_LIB_DIR=\"$(slrn_libdir)\" -DSHAREDIR=\"$(pkgdatadir)\" \
 -DSYSCONFDIR=\"$(sysconfdir)\"
INCLUDES=-I. -I$(top_srcdir)/intl @SLANG_INC@ @SSLINC@ @UUINC@ @LOCKINC@
LDADD=@LIBINTL@ @SLANG_LIB@ -lslang @TERMCAP@ @SSLLIB@ @UULIB@ @LOCKLIB@

uudecode_SOURCES = slrndir.c snprintf.c ttymsg.c util.c
uudecode_LDADD=uudecode.o @LIBINTL@ @SLANG_LIB@ -lslang

EXTRA_DIST = art.h charmaps.h chmap.h clientlib.c clientlib.h group.h \
 grplens.h hash.h hooks.h interp.h jdmacros.h menu.h mime.h misc.h \
 nntpcodes.h nntplib.h post.h print.h score.h server.h slrn.h slrnconf.h \
 slrndir.h slrnfeat.h sltcp.h snprintf.h startup.h ttymsg.h \
 util.h decode.h version.h vfile.h vms.h vmsmail.h xover.h DESCRIP.MMS \
 Makefile.g32 Makefile.os2 Makefile.w32 VMSMAKE.COM nntp.c sltcp.c spool.c \
 vms.c vmsmail.c art_sort.h ranges.h

MOSTLYCLEANFILES = *~ config.h chkslang slrnpull stamp2 uudecode

uudecode.o: decode.c
	$(COMPILE) -DSTANDALONE -o uudecode.o -c decode.c

## This is a hack
slangversion_SOURCES =
slangversion$(EXEEXT): chkslang$(EXEEXT)
	./chkslang$(EXEEXT) slrn 10003 10400

## Another hack: Find out whether slrnpull support gets compiled in
stamp2: config.h
	@if test "x$(SLRNPULL)" != "xslrnpull$(EXEEXT)"; then \
	echo ""; \
	echo "*** WARNING ***"; \
	echo ""; \
	echo "You're building slrnpull without having support for it enabled in slrn."; \
	echo "You most probably do not want to do this."; \
	echo "Please call ./configure --with-slrnpull and compile again."; \
	echo ""; \
	else \
	touch stamp2; \
	fi;
