## Process this file with automake to produce Makefile.in -*- sh -*-
## Based on the Makefile by John E. Davis, adapted to automake.
##
##  jed version 0.98-5 or newer is required.  tm2txt is part of the slang
##  distribution.
##
SOURCE_FILES = slrnfuns.tm slrnfuns/article.tm slrnfuns/dialog.tm \
 slrnfuns/file.tm slrnfuns/group.tm slrnfuns/header.tm slrnfuns/hooks.tm \
 slrnfuns/keys.tm slrnfuns/misc.tm

EXTRA_DIST = $(SOURCE_FILES) slrnfuns/tm-sort.sl slrnfuns/whatelse.sl

TXT_FILES = slrnfuns.txt
SGML_FILES = slrnfuns.sgml
HTML_FILES = slrnfuns.html

MOSTLYCLEANFILES = *~ *.dvi *.log *.aux *.toc slrnfuns/*.BAK slrnfuns/*~
DISTCLEANFILES = *.html $(TXT_FILES) $(SGML_FILES)

SGML2LATEX = sgml2latex -p letter -o tex
SGML2HTML = sgml2html
SGML2TXT = sgml2txt -f --pass="-P -c"
TM2TXT = tm2txt --quiet
LATEX = latex

TEXTDIR = ..
HTMLDIR = html
SGMLDIR = sgml

convert-all: $(SGML_FILES) $(HTML_FILES) $(TEX_FILES) $(TXT_FILES)
convert-txt: $(TXT_FILES)

update: convert-txt
##	-mkdir $(SUBDIRS)
	-mv $(TXT_FILES) $(TEXTDIR)
##	-mv *.html $(HTMLDIR)

##----- SGML Files ----------------------------------------------------------
slrnfuns.sgml: $(SOURCE_FILES)
	jed -script tm2sgml.sl slrnfuns
##----- HTML Files ----------------------------------------------------------
slrnfuns.html : slrnfuns.sgml
	$(SGML2HTML) slrnfuns.sgml
##----- Text Files ----------------------------------------------------------
slrnfuns.txt : slrnfuns.sgml
	$(SGML2TXT) slrnfuns.sgml
##	cat slrnfuns/*.tm | $(TM2TXT) > slrnfuns.txt
