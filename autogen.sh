#!/bin/sh
# borrowed from xmms CVS

(autopoint --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autopoint (part of gettext) installed to compile slrn";
	echo;
	exit;
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile slrn";
	echo;
	exit;
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autoconf installed to compile slrn";
	echo;
	exit;
}

echo "Generating configuration files for slrn, please wait...."
echo;

autopoint -f;
aclocal -I autoconf;
autoheader;
automake --foreign --add-missing;
autoconf;

./configure $@

