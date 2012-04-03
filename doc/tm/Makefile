# -*- sh -*-

# To create the SGML files, you will need to install the tmexpand
# package.  See http://www.jedsoft.org/ for more information.
#
TMEXPAND = tmexpand
TM2SGML = $(TMEXPAND)

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
SGML2TXT = linuxdoc -B txt -f
#SGML2TXT = sgml2txt -f --pass="-P -c"
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
	$(TM2SGML) -I$(MACRODIR) slrnfuns.tm slrnfuns.sgml
##----- HTML Files ----------------------------------------------------------
slrnfuns.html : slrnfuns.sgml
	$(SGML2HTML) slrnfuns.sgml
##----- Text Files ----------------------------------------------------------
slrnfuns.txt : slrnfuns.sgml
	$(SGML2TXT) slrnfuns.sgml
##	cat slrnfuns/*.tm | $(TM2TXT) > slrnfuns.txt
