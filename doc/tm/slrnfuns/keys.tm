This chapter describes functions that can be used to control interactive
functions from your macros.

\function{call}
\usage{Void call (String fun)}
\description
   This function is used to execute an interactive slrn internal
   function.  Such functions are used with \var{setkey} statements in the
   \var{.slrnrc} startup files.
\seealso{definekey, undefinekey, set_prefix_argument}
\done

\function{definekey}
\usage{definekey (String fun, String key, String km)}
\description
   This function is used to bind a key sequence specified by \var{key} to
   a function \var{fun} in the keymap \var{km}.  Here \var{fun} can be any
   predefined slang function that takes 0 arguments and returns void.
   The parameter \var{km} must be either "article", "group", or "readline".
\seealso{undefinekey, call, set_prefix_argument}
\done

\function{get_prefix_arg}
\usage{Int_Type get_prefix_arg ()}
\description
   The \var{get_prefix_arg} function returns the value of the prefix
   argument. If no prefix argument has been set, the function returns
   \var{-1}, which is an impossible value for the prefix argument.
\notes
   The prefix argument is specified interactively via the ESC key
   followed by one or more digits that determine value of the prefix
   argument.
   
   This concept has been borrowed from the emacs text editor.
\seealso{set_prefix_argument, reset_prefix_arg}
\done

\function{getkey}
\usage{Integer getkey ()}
\description
   Read a character from the terminal and returns its value.
   Note: Function and arrow keys usually return more than one character.
\seealso{ungetkey, input_pending, read_mini}
\done

\function{input_pending}
\usage{Integer input_pending (Integer tsecs)}
\description
   This function checks for keyboard input.  Its argument specifies
   the number of tenths of a second to wait.  It returns 0 if no input
   is available or a non-sero value if input is available.
\seealso{getkey, ungetkey}
\done

\function{reset_prefix_arg}
\usage{Void reset_prefix_arg ()}
\description
   The \var{reset_prefix_arg} function may be used to reset the prefix
   argument.  This is usually necessary after calling to keep the
   argument from propagating to other functions.
\seealso{get_prefix_arg, set_prefix_argument}
\done

\function{set_prefix_argument}
\usage{Void set_prefix_argument (Integer val)}
\description
   The \var{set_prefix_argument} function may be used to set the prefix
   argument to \var{val}.  It is mainly used immediately before
   \var{calling}
   internal functions which take prefix arguments.
\seealso{call}
\done

\function{undefinekey}
\usage{Void undefinekey (String key, String map)}
\description
   This function undefineds a key sequence specified by \var{key} from
   keymap \var{map}.
\seealso{definekey}
\done

\function{ungetkey}
\usage{Void ungetkey (Integer ch)}
\description
   This function pushes the character \var{ch} back upon the input stream
   such that the next call to \var{getkey} will return it.  It is possible
   to push several characters back.
\seealso{getkey}
\done

