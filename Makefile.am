## Process this file with automake to produce Makefile.in -*- sh -*-
## Use aclocal -I autoconf/ && automake --foreign

SUBDIRS = autoconf doc po src
AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I autoconf

docdir = @docdir@
doc_DATA = COPYRIGHT COPYING README changes.txt

MACROS = macros/color.sl macros/gsort.sl macros/ispell.sl \
 macros/latin2.sl macros/multipart.sl macros/nn.sl macros/posthook.sl \
 macros/search.sl macros/tin-art.sl macros/tin-group.sl macros/ttyprint.sl \
 macros/util.sl macros/varset.sl macros/xcomment.sl

CONTRIB = contrib/cleanscore contrib/slrnrc-conv
CONTRIBDOCS = contrib/README.cleanscore contrib/README.slrnrc-conv \
 contrib/NEWS.cleanscore

#EXTRA_DIST = autoconf/config.rpath autoconf/mkinstalldirs autoconf/acinclude.m4 \
# autoconf/config.guess autoconf/config.sub autoconf/install-sh \
# autoconf/missing autoconf/mkinstalldirs 
EXTRA_DIST = $(doc_DATA) $(MACROS) $(CONTRIBDOCS) $(CONTRIB)

MOSTLYCLEANFILES = *~ config.h autoconf/*~ contrib/*~ macros/*~ po/*~

slrnpull:
	@echo ""
	@echo "You do not need to call \"make slrnpull\" explicitly."
	@echo "To enable support for slrnpull and build it automatically,"
	@echo "just pass --with-slrnpull when calling ./configure."
	@echo ""
uudecode:
	cd src; $(MAKE) uudecode

install-contrib:
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/contrib
	@list='$(CONTRIB)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(pkgdatadir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(pkgdatadir)/$$p; \
	  fi; \
	done;
	$(mkinstalldirs) $(DESTDIR)$(docdir)/contrib;
	@list='$(CONTRIBDOCS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(docdir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(docdir)/$$p; \
	  fi; \
	done;

dist-hook:
	-chmod -R a+r $(distdir)
	$(AMTAR) chojf $(distdir).tar.bz2 $(distdir)

all-local:
	@echo ""
	@echo "Compilation of slrn complete."
	@echo "To copy it into your system, type \"make install\"."
	@echo ""

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/macros
	@list='$(MACROS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(pkgdatadir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(pkgdatadir)/$$p; \
	  fi; \
	done;
	@echo ""
	@echo "Installation of slrn complete."
	@echo "If you're setting up slrn for the first time, please read \"doc/FIRST_STEPS\"."
	@echo "Enjoy :-)"
	@echo ""

uninstall-local:
	list='$(MACROS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(pkgdatadir)/$$p; \
	done
	rmdir $(DESTDIR)$(pkgdatadir)/macros || /bin/true
	list='$(CONTRIB)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(pkgdatadir)/$$p; \
	done
	rmdir $(DESTDIR)$(pkgdatadir)/contrib || /bin/true
	list='$(CONTRIBDOCS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(docdir)/$$p; \
	done
	rmdir $(DESTDIR)$(docdir)/contrib || /bin/true
	rmdir $(DESTDIR)$(docdir) || /bin/true
	rmdir $(DESTDIR)$(pkgdatadir) || /bin/true
