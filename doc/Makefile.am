## Process this file with automake to produce Makefile.in -*- sh -*-

SUBDIRS = tm

SLRNPULL = @SLRNPULL@
PULLDOCS = slrnpull/README slrnpull/SETUP slrnpull/score \
 slrnpull/setgid.txt slrnpull/slrn.rc slrnpull/slrnpull.conf \
 slrnpull/slrnpull.sh slrnpull/README.offline

docdir = @docdir@
doc_DATA = FAQ FIRST_STEPS README.GroupLens README.SSL README.macros \
 README.multiuser THANKS help.txt manual.txt score.sl score.txt \
 slrn-doc.html slrn.rc slrnfuns.txt

man_MANS = slrn.1 @MAN_SLRNPULL@

EXTRA_DIST = $(doc_DATA) $(PULLDOCS) INSTALL.os2 INSTALL.unix INSTALL.vms \
 INSTALL.w32 VMS-SLRN.HLP pc-keys.txt slrn.1 slrnpull.1

MOSTLYCLEANFILES = *~ slrnpull/*~

install-data-local:
	@if test "x$(SLRNPULL)" = "xslrnpull"; then \
	$(mkinstalldirs) $(DESTDIR)$(docdir)/slrnpull; \
	list='$(PULLDOCS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(docdir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(docdir)/$$p; \
	  fi; \
	done; \
	fi;

uninstall-local:
	list='$(PULLDOCS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(docdir)/$$p; \
	done
	rmdir $(DESTDIR)$(docdir)/slrnpull
