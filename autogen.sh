#!/bin/sh
# borrowed from xmms CVS

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have gettext installed to compile slrn";
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

echo "Running gettextize, please ignore non-fatal messages...."
echo n | gettextize --copy --force;
aclocal -I autoconf;
autoheader;
automake --add-missing;
autoconf;

./configure $@

