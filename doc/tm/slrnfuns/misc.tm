This chapter is for all functions and variables that did not fit nicely in
any of the others. They are available in all modes.

\variable{_slrn_version}
\usage{Integer _slrn_version}
\description
   The \var{_slrn_version} variable is read only. It is an integer value
   representing the slrn's version number -- version aa.bb.cc.dd becomes
   aabbccdd.
\example
   In version 0.9.7.1, \var{_slrn_version} is 90701 (note that leading
   zeroes are omitted).
\seealso{_slrn_version_string}
\done

\variable{_slrn_version_string}
\usage{String _slrn_version_string}
\description
   The \var{_slrn_version_string} variable is read only. It contains the
   version string as displayed by the program itself (e.g. "0.9.7.1").
\seealso{_slrn_version}
\done

\function{datestring_to_unixtime}
\usage{Integer datestring_to_unixtime (String date)}
\description
   This function converts the date string \var{date} (in any format commonly
   used in "Date:" header lines) to an integer value, giving the number of
   seconds since 00:00:00 GMT, January 1, 1970.
\example
   The following function returns the date of the currently selected header
   as seconds since the Epoch:
   
        define get_article_time ()
        { 
	    return datestring_to_unixtime(extract_article_header("Date"));
        }
\done

\function{get_bg_color}
\usage{String get_bg_color (String obj)}
\description
   This function returns the current background color of the object
   specified by \var{obj}.
\notes
   Due to a limitation in S-Lang, this function only works on Unix.
\done

\function{get_fg_color}
\usage{String get_fg_color (String obj)}
\description
   This function returns the current foreground color of the object
   specified by \var{obj}.
\notes
   Due to a limitation in S-Lang, this function only works on Unix.
\done

\function{get_variable_value}
\usage{Value get_variable_value (String v)}
\description
   This function returns the value of an internal variable specified
   by \var{v}.  Here \var{v} must be one of the variable names that can be
   used in \var{.slrnrc} `set' commands.  The type of the object returned will
   depend upon the type of the object \var{v} represents.
\seealso{set_integer_variable, set_string_variable}
\done

\function{quit}
\usage{Void quit (Integer exit_status)}
\description
   This function will cause the newsreader to exit with exit status
   specified by \var{exit_status}.
\seealso{call}
\done

\function{register_hook}
\usage{Integer register_hook (String hook, String function)}
\description
   register_hook can be used to call a given \var{function} whenever one of
   slrn's \var{hook}s is executed. It returns one of the following values:
#v+
   0: Hook does not exist or may not be defined multiple times.
   1: Function successfully registered.
   2: Given function was already registered for this hook.
   3: Undefined function successfully registered.
#v-
   If you register multiple functions for the same hook, they will be called
   in the order in which they were registered. If a function with the name
   of a hook is defined, it gets called after those that were registered
   using this function.

   It is possible to register a function first and define it
   afterwards. In this case, register_hook returns 3.
   
   cc_hook, make_from_string_hook and subject_compare_hook may only be
   defined once, as they return a value and slrn only expects a single
   return value when calling them.
\seealso{unregister_hook}
\done

\function{reload_scorefile}
\usage{Void reload_scorefile (Integer apply_now)}
\description
   This function can be used to reload the scorefile after a macro changed
   it.  If the integer \var{apply_now} is 1, the new scores are immediately
   applied.  If it is 0, the new scores are used the next time you enter a
   group; if -1, the user is queried.
\notes
   Outside article mode, \var{apply_now} has no effect.
\done

\function{server_name}
\usage{String server_name ()}
\description
   The \var{server_name} function returns the name of the current server.
\seealso{current_newsgroup}
\done

\function{set_color}
\usage{Void set_color (String obj, String fg, String bg)}
\description
   This function may be used to set the foreground and background
   colors of an object.  The \var{obj} parameter specifies the object and
   the \var{fg} and \var{bg} parameters specify the foreground and background
   colors, respectively.
\done

\function{set_color_attr}
\usage{Void set_color_attr (String obj, String fg, String bg, Integer attr)}
\description
   This functions works like set_color, but has the additional argument
   \var{attr} that allows you to assign attributes to the color object (if
   your terminal supports this). \var{attr} can be 0 (if you do not want any
   attributes to take effect) or any combination of the following constants:
#v+
   ATTR_BLINK     blinking text
   ATTR_BOLD      bold text
   ATTR_REV       inverse text
   ATTR_ULINE     underlined text
#v-
\done

\function{set_ignore_quotes}
\usage{Void set_ignore_quotes (Array_Type regexps)}
\description
   This function allows you to change the setting of the ignore_quotes
   configuration command. \var{regexps} has to be a (one-dimensional) array
   of 1-5 strings that are interpreted as regular expressions to detect
   quoted lines.
\example
   set_ignore_quotes (["^>", "^|"]);
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
   The effect of this command becomes visible with the next article you
   download. If one is currently displayed, it remains unaffected.
\done

\function{set_integer_variable}
\usage{Void set_integer_variable (String name, Integer v)}
\description
   This function may be used to set the value of the internal integer
   variable specified by \var{name} to value \var{v}.  \var{name} must be an integer
   variable name allowed in .slrnrc \var{set} commands.
\seealso{set_string_variable, get_variable_value}
\done

\function{set_string_variable}
\usage{Void set_string_variable (String name, String v)}
\description
   This function may be used to set the value of the internal string
   variable specified by \var{name} to value \var{v}.  \var{name} must be a string
   variable name allowed in .slrnrc \var{set} commands.
\seealso{set_integer_variable, get_variable_value}
\done

\function{set_strip_re_regexp}
\usage{Void set_strip_re_regexp (Array_Type regexps)}
\description
   This function allows you to change the setting of the strip_re_regexp
   configuration command. It works like set_ignore_quotes.
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
\seealso{set_ignore_quotes, set_strip_sig_regexp, set_strip_was_regexp}
\done

\function{set_strip_sig_regexp}
\usage{Void set_strip_sig_regexp (Array_Type regexps)}
\description
   This function allows you to change the setting of the strip_sig_regexp
   configuration command. It works like set_ignore_quotes.
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
   The effect of this command becomes visible with the next article you
   download. If one is currently displayed, it remains unaffected.
\seealso{set_ignore_quotes, set_strip_re_regexp, set_strip_was_regexp}
\done

\function{set_strip_was_regexp}
\usage{Void set_strip_was_regexp (Array_Type regexps)}
\description
   This function allows you to change the setting of the strip_was_regexp
   configuration command. It works like set_ignore_quotes.
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
\seealso{set_ignore_quotes, set_strip_re_regexp, set_strip_sig_regexp}
\done

\function{set_utf8_conversion_table}
\usage{Void set_utf8_conversion_table (Array_Type table)}
\description
   This function can be used to define a conversion table for decoding
   UTF-8.  \var{table} has to be a two-dimensional array of integer values
   that has two columns: The left column contains the Unicode characters you
   want to convert, the right column the corresponding local characters.

   When decoding, any non-ASCII characters that cannot be found in your
   table are displayed as question marks.  If \var{table} has no rows, UTF-8
   will be converted to Latin 1, which is also the default if this function
   is not called. Thus, you can reset the default using
#v+
   set_utf8_conversion_table (Integer_Type[2,0]);
#v-
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
\done

\function{setlocale}
\usage{String setlocale (Integer category, String locale)}
\description
   You can use this function to change the current locale at runtime.  You
   may want to do this if you read groups in different languages.  The
   syntax is identical to the one of setlocale(3).  For \var{category}, the
   following constants are defined:
#v+
      LC_CTYPE : affects character handling (e.g. which 8bit characters
                 are regarded as upper/lower case)
      LC_TIME  : affects the formatting of dates (12-hour vs. 24-hour
                 clock, language of month names etc.)
#v-
   The \var{locale} can be any locale supported by your system.  If it is an
   empty string, the locale to use will be taken from the environment.  The
   function will return the name of the locale that was actually selected.

   Please note that locales are not supported by all systems.  In this case,
   this function will trigger an slang error.
\done

\function{tt_send}
\usage{Void tt_send (String_Type s)}
\description
   This function may be used to send a string directly to the display
   without any involvement of the screen management layer.
\seealso{message, update}
\done

\function{unregister_hook}
\usage{Integer unregister_hook (String hook, String function)}
\description
   This function is used to unregister functions that were assigned to a
   hook using register_hook. Its return values are:
#v+
   0: Hook does not exist or function is not assigned to it.
   1: Function successfully unregistered.
#v-
\seealso{register_hook}
\done

\function{update}
\usage{update ()}
\description
   This function may be used to force the display to be updated.
\seealso{message}
\done

