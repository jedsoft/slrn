This chapter describes some functions which are useful if you want to
interact with the user.

\function{get_response}
\usage{Integer get_response (String choices, String prompt)}
\description
   This function will prompt the user for a single character using the
   prompt as specifed by the second parameter.  The first parameter,
   choices, specified the characters that will be accepted.  Any
   character in the prompt string that is preceeded by \\001 will be
   given the `response_char' color.
\example
   The following:
#v+
       rsp = get_response ("yYnN", "Are you hungry? \001Yes, \001No");
#v-
   will return one of the four characters \var{y}, \var{Y}, \var{n},
   or \var{N} to the variable \var{rsp}.
\seealso{get_yes_no_cancel, set_color, get_select_box_response}
\done

\function{get_select_box_response}
\usage{Integer get_select_box_response (title, item_1, ..., n_items)}
\description
   This function pops a selection box and queries the user for a
   response.  An integer is returned which indicates the user's choice.
\example
#v+
       variable rsp = get_select_box_response (
                         "Pick a number:",
                         "one", "two", "three", "four",
                         4);
       message (sprintf ("You chose %d", rsp));
#v-
\seealso{read_mini, message, get_yes_no_cancel, get_response, select_list_box}
\done

\function{get_yes_no_cancel}
\usage{Integer get_yes_no_cancel (str)}
\description
   This function displays \var{str} in the minibuffer after concatenating
   \exmp{"? [Y]-es, N-o, C-ancel"} to it.  It then awaits user input and
   returns:
#v+
        1 if yes
        0 if no
       -1 if cancel
#v-
\notes
   If a \var{%} character is to appear, it must be doubled.
\seealso{get_select_box_response, getkey, read_mini, select_list_box}
\done

\function{message_now}
\usage{Void message_now (String_Type s)}
\description
  This function displays the string \var{s} to the message area
  immediately.
\seealso{message, vmessage, error}
\done

\function{popup_window}
\usage{Int popup_window (String title, String text)}
\description
  This function creates a popup window which contains the given \var{text}
  and uses \var{title} as its title. It returns the key that was used to
  exit the window.
\notes
  Since slrn 0.9.7.4, this function expands TABs in the \var{text}
  correctly. TABs in \var{title} are not expanded and should be avoided.
\seealso{select_list_box}
\done

\function{read_mini}
\usage{String read_mini (String p, String dflt, String init)}
\description
   This function will prompt the user for a string value using prompt
   \var{p}.  The second parameter \var{dfl} is used to specify the
   default value. If the final parameter is not the empty string
   (\exmp{""}), it will be made available to the user for editing.
\seealso{read_mini_filename, read_mini_no_echo, read_mini_integer, read_mini_variable, getkey, set_input_string, set_input_chars}
\done

\function{read_mini_filename}
\usage{String read_mini_filename (String p, String dflt, String init)}
\description
   This function works like \var{read_mini}, but allows the user to tab
   complete filenames.
\seealso{read_mini, read_mini_variable, getkey, set_input_string, set_input_chars}
\done

\function{read_mini_integer}
\usage{Integer read_mini_integer (String p, Integer dflt)}
\description
   This function will prompt the user for an integer value using prompt
   \var{p} and taking \var{dflt} as the default.
\seealso{read_mini}
\done

\function{read_mini_no_echo}
\usage{String read_mini_no_echo (String p, String dflt, String init)}
\description
   This function performs the same purpose as \var{read_mini} except it
   does not echo the entered text to the screen.
\seealso{read_mini, getkey, set_input_string, set_input_chars}
\done

\function{read_mini_variable}
\usage{String read_mini_variable (String p, String dflt, String init)}
\description
   This function works like \var{read_mini}, but allows the user to tab
   complete the names of slrn's configuration variables.
\seealso{read_mini, read_mini_filename, getkey, set_input_string, set_input_chars}
\done

\function{select_list_box}
\usage{String_Type select_list_box (title, s_1, ... s_n, n, active_n)}
#v+
    String_Type title, s_1, ... s_n
    Int_Type n, active_n
#v-
\description
   This purpose of this function is to present a list of \var{n} strings,
   specified by the \var{s_1}, ... \var{s_n} parameters to the user and have
   the user select one.  The user interface for this operation is that
   of a box of strings.  The title of the box is specified by the
   \var{title} parameter.  The \var{active_n} parameter specifies which string
   is to be the default selection.  It returns the string selected by
   the user.
\seealso{get_select_box_response, get_response}
\done

\function{set_input_chars}
\usage{Void set_input_chars (String val)}
\description
   This function may be used to set the character that will be returned
   by the next prompt for single character input in the minibuffer.
   This is the type of input that \var{get_response} solicits.
\example
#v+
       set_input_chars ("y");
       if ('y' == get_yes_no_cancel ("Really Quit"))
         quit (0);
#v-
\seealso{set_input_string, get_response, get_yes_no_cancel}
\done

\function{set_input_string}
\usage{Void set_input_string (String val)}
\description
   This function may be used to set the string that will be returned
   by the next prompt for input in the minibuffer.  One can set the
   value returned for the next n prompts by separating the values by
   \\n characters.
\example
   The code
#v+
       variable a, b;
       set_input_string ("Apple\nOrange");
       a = read_mini ("Enter Fruit", "", "");
       b = read_mini ("Enter another Fruit", "", "");
#v-
   will result in \var{a} having the value \var{Apple} and \var{b} having the
   value \var{Orange}.
\seealso{read_mini, set_input_chars}
\done

