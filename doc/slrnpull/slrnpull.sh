#!/bin/sh

# I run this file from cron every night to pull a few newsgroups from my
# news server.  It does the following:
#
#     1.  Runs slrnpull in expire mode to expire old articles
#     2.  Starts up ppp via a ppp-on script (not provided)
#     3.  Runs slrnpull to grab articles from the server
#     4.  Turns off ppp via a ppp-off script (not provided)

# Configuration variables.  Change these!!!
dir=/var/spool/news
server=news.mit.edu
slrnpull=/home/john/src/slrn/src/objs/slrnpull

#----------------------------------------------------------------------------
# Make sure that all files will be readable by others
umask 022

# Before getting new articles, perform expiration.
$slrnpull -d $dir --expire

if /usr/sbin/ppp-on; then
 $slrnpull -d $dir -h $server
 /usr/sbin/ppp-off
else
 exit 1
fi


