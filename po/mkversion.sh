#version 1.0
# The initial echo is necessary because the solaris version of sed cannot
# grok input without a trailing newline.
cat ../src/version.h | sed -n 's/^.*SLRN_VERSION_STR.*[^"]*"\([^"]*\)"/\1/p'
