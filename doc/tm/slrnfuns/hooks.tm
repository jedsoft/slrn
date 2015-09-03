This chapter is special. Rather than describing intrinsic functions, it
gives you a list of hooks that can be used to execute arbitrary \slang code
on certain events (e.g. whenever entering article mode).

You can define code for a hook by putting it into a function that has
exactly the same name as the hook. However, the preferred way to add code to
a hook is now using the register_hook () intrinsic function on an arbitrary
macro. This mechanism allows to connect more than one macro to a hook, which
comes in handy if you want to use pre-written macro sets.

\function{article_mode_hook}
\usage{Void article_mode_hook ()}
\description
   This hook is called during article mode after headers have been retrieved
   but before sorting them.  You can use this hook to set variables based on
   the group name.
\example
   The following macro can be used to change the \var{sorting_method} to
   something more appropriate for newsgroups which contain encoded articles
   and to chose a different signature when posting to comp.*:
#v+
   define make_group_specific_settings ()
   {
     variable sorting_method = 7;
     variable signature_file = ".signature";

     if (is_substr (current_newsgroup (), "binaries")
         or is_substr (current_newsgroup (), "pictures"))
      sorting_method = 3;

     if (0 == strncmp (current_newsgroup (), "comp.", 5))
       signature_file = ".nerd-signature";

     set_integer_variable ("sorting_method", sorting_method);
     set_string_variable ("signature", signature_file);
   }
   () = register_hook ("article_mode_hook",
                       "make_group_specific_settings");
#v-
\done

\function{article_mode_quit_hook}
\usage{Void article_mode_quit_hook ()}
\description
   This function is called whenever you leave article mode, including the
   times you switch directly to a different group (without quitting to group
   mode in between).
\done

\function{article_mode_startup_hook}
\usage{Void article_mode_startup_hook ()}
\description
   Unlike \var{article_mode_hook}, which gets called prior to sorting the
   headers, this hook gets called after sorting has taken place.
\done

\function{cc_hook}
\usage{String cc_hook (String address)}
\description
   This hook is called when sending a "courtesy copy" of a followup -- it
   gets the author's email address as an argument and is expected to leave a
   string on the stack which will be used as the address the CC is actually
   sent to.  If the returned string is empty, no CC is sent.
\notes
   As this hook returns a value, you cannot bind multiple macros to it.
\done

\function{followup_hook}
\usage{Void followup_hook ()}
\description
   Function called when following up to an article.
\done

\function{forward_hook}
\usage{Void forward_hook ()}
\description
   Function called when forwarding an article to someone.
\done

\function{group_mode_hook}
\usage{Void group_mode_hook ()}
\description
   This hook will be called whenever group mode is entered.  This includes
   the times when one exits article mode back to group mode.
\done

\function{group_mode_startup_hook}
\usage{Void group_mode_startup_hook ()}
\description
   This hook is called after checking for news and immediately before
   entering the main keyboard loop.  When called, group mode will be active.
\done

\function{header_number_hook}
\usage{Void header_number_hook ()}
\description
   If defined, this function will be called after selecting a header via a
   header number.
\done

\function{make_from_string_hook}
\usage{String make_from_string_hook ()}
\description
   This function is expected to leave a string on the stack which will be
   used to generate ``From'' header lines whenever one is needed.
\notes
   As this hook returns a value, you cannot bind multiple macros to it.
\example
   Here is a simple example:
#v+
   define make_from_string_hook ()
   {
     return "My Name <me@my.machine>";
   }
#v-
\done

\function{make_save_filename_hook}
\usage{String make_save_filename_hook ()}
\description
   This function is expected to leave a string on the stack that will be
   used to decide what folder an article should be saved to. If the returned
   filename is not absolute, it is interpreted as relative to
   save_directory.
\notes
   As this hook returns a value, you cannot bind multiple macros to it.
\example
   Here is a simple example:
#v+
   define make_save_filename_hook ()
   {
     if (string_match (extract_article_header ("Subject"), "slrn", 1) != 0)
       return "slrn-related";
     else
       return current_newsgroup();
   }
#v-
\done

\function{post_file_hook}
\usage{Void post_file_hook (String file)}
\description
   Function called after composing and filtering, but before posting
   article.
   This hook takes a single parameter: the name of the file that slrn is
   about to post.
\example
   An example of this hook is included in macros/posthook.sl in slrn's
   source tree.
\done

\function{post_filter_hook}
\usage{Void post_filter_hook (String file)}
\description
   This hook may be called just before slrn attempts to post a file. The
   hook is only called if the user selects the filter option from the
   prompt:
#v+     
     Post the message? Yes, No, Edit, poStpone, Filter
#v-
   This hook takes a single parameter: the name of the file that slrn is
   about to post.
\example
   An example of this hook is included in macros/ispell.sl in slrn's source
   tree.
\done

\function{post_hook}
\usage{Void post_hook ()}
\description
   Function called when posting an article.
\done

\function{pre_article_mode_hook}
\usage{Void pre_article_mode_hook ()}
\description
   This hook is similar to \var{article_mode_hook} except that it is called
   before any headers for the group have been retrieved.
\done

\function{quit_hook}
\usage{Void quit_hook ()}
\description
   Function called when slrn exits. Note that slrn already disconnected from
   the server at the time this hook is run. In this hook, it is safe to
   assume that startup_hook was run before (i.e., if slrn exits before
   startup_hook had a chance to execute, quit_hook is omitted).
\done

\function{read_article_hook}
\usage{Void read_article_hook ()}
\description
   Function called after reading and processing an article. It may use the
   replace_article function to change it.
\done

\function{reply_hook}
\usage{Void reply_hook ()}
\description
   Function called when replying to poster.
\done

\function{resize_screen_hook}
\usage{Void resize_screen_hook ()}
\description
   This hook will be called whenever the screen size changes.
\done

\function{startup_hook}
\usage{Void startup_hook ()}
\description
   This hook is called right after the newsreader is initialized and
   immediately before checking for news.  It allows the user to set
   variables on a server by server basis.
\example
   The following example sets the `lines_per_update' variable to 20 and
   turns off reading of the active file if the servername is `uhog' (it is a
   slow server):
#v+
   define make_server_specific_settings ()
   {
     !if (strcmp (server_name (), "uhog"))
     {
        set_integer_variable ("lines_per_update", 20);
        set_integer_variable ("read_active", 0);
     }
   }
   () = register_hook ("startup_hook",
                       "make_server_specific_settings");
#v-
\done

\function{subject_compare_hook}
\usage{Integer subject_compare_hook (String subject1, String subject2)}
\description
   slrn puts postings with identical subjects into the same thread.  This
   hook can be used to override slrn's decision that two subjects are not
   identical: In this case, it is called with both subjects as arguments.
   If it returns 0, the articles are put in the same thread.
\done

\function{supersede_hook}
\usage{Void supersede_hook ()}
\description
   Function called when superseding an article.
\done
