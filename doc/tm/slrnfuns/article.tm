The intrinsic functions described in this chapter are available in article
mode and allow you to manipulate the article window.

\function{_is_article_visible}
\usage{Integer _is_article_visible ()}
\description
   This function returns information about whether or not the article
   associated with the current header is visible in a window and
   whether or not it is attached to the current header.
   Specifically, it returns a bitmapped value:
#v+
      0  : if the article window is hidden and not associated with the
             current header.
      1  : if the article window is showing but the current header
             does not refer to the article.
      2  : if the article window is hidden but attached to the current
             header.
      3  : if the article window is showing and contains the current
             header article.
#v-
\example
   If one only want to know whether or not there is an article visible
   in the window, then use
#v+
      _is_article_visible () & 1
#v-
   To determine whether or not it is associated with the current
   header regardless of whether or not it is showing, use
#v+
      _is_article_visible () & 2
#v-
\seealso{_is_article_visible, is_article_window_zoomed, call}
\done

\function{article_as_string}
\usage{String_Type article_as_string ()}
\description
   This function will return the entire contents of the current
   article as a string.  If no article has been dowloaded, the empty
   string will be returned.  The current article may not be the one
   associated with the currently selected header.
\seealso{raw_article_as_string, replace_article, is_article_visible}
\done

\function{article_cline_as_string}
\usage{String_Type article_cline_as_string ()}
\description
   Returns the current line of the article (i.e. the first one that is
   displayed in the article pager) as a string.  If no article has been
   downloaded, the empty string will be returned.
\seealso{article_as_string, article_cline_number}
\done

\function{article_cline_number}
\usage{Integer article_cline_number ()}
\description
   Returns the number of the line that is currently displayed at the top of
   the article pager.  Counting starts at 1 and ignores hidden lines.
\seealso{article_cline_as_string, article_count_lines, article_goto_line}
\done

\function{article_count_lines}
\usage{Integer article_count_lines ()}
\description
   This function returns the total number of visible lines in the article
   pager.  For example, you could use it with \var{article_goto_line} to
   jump to the last line in the pager:
#v+
     define article_bob ()
     { 
         () = article_goto_line (article_count_lines ());
     }
#v-
\done

\function{article_goto_line}
\usage{Integer article_goto_line (Integer linenum)}
\description
   Makes the article pager jump to line number \var{linenum} (i.e., puts
   this line at the top of the pager).  As in \var{article_cline_number},
   counting starts at 1 and ignores hidden lines.  If you specify 0 or a
   negative number as the argument, no action is taken and the function
   returns zero.  Otherwise, the new line number is returned. If the article
   has less than \var{linenum} lines, the function goes to the last line.
\seealso{article_line_down, article_line_up}
\done

\function{article_line_down}
\usage{Integer article_line_down (Integer num)}
\description
   Scrolls the article down \var{num} lines.  Returns the number of lines
   that were actually scrolled.
\notes
   If no article is in the pager, scrolling down first downloads an article.
\seealso{article_goto_line, article_line_up}
\done

\function{article_line_up}
\usage{Integer article_line_up (Integer num)}
\description
   Scrolls the article up \var{num} lines and returns the number of lines
   that were actually scrolled.
\seealso{article_goto_line, article_line_down}
\done

\function{bsearch_article}
\usage{Integer bsearch_article (String_Type pat)}
\description
   This function works like search_article, but does a backward search.
\seealso{search_article}
\done

\function{get_article_window_size}
\usage{Integer get_article_window_size ()}
\description
   \var{get_article_window_size} may be used to determine the height of the
   article window.
\seealso{set_article_window_size}
\done

\function{get_body_status}
\usage{Integer get_body_status ()}
\description
   When reading from an slrnpull spool in "true offline" mode, the body that
   belongs to an article header might not yet be on disk. This function
   returns one of the following values:
#v+
   -1  : currently, no header is selected
    0  : the body of the current article is present
    1  : the body of the article is missing
    2  : the body of the article is missing, but requested for download
#v-
\seealso{request_body}
\done

\function{get_next_art_pgdn_action}
\usage{Integer get_next_art_pgdn_action ()}
\description
   This function may be used to get information about what action slrn
   will take when an attempt is made to go to the next page of the
   current article, e.g., by pressing the space key.  It returns one
   of the following integers:
#v+
       -1  Not in article mode
        0  Next page of the article will be displayed
        1  The next unread article will be displayed
        2  The newsreader will go to the next newsgroup
#v-
\done

\function{is_article_visible}
\usage{Integer is_article_visible ()}
\description
   This function returns information about whether or not the article
   associated with the current header is visible in a window.
   Specifically, it returns:
#v+
      0  : if the article window is hidden
      1  : if the article window is showing but the current header
             does not refer to the article
      3  : if the article window contains the current header article
#v-
\notes
   For some purposes, it may be more useful to the the
   \var{_is_article_visible} function which may be slightly more
   useful.  In fact, \var{is_article_visible} may be written in terms
   of \var{_is_article_visible} as
#v+
     define is_article_visible ()
     { 
         variable status = _is_article_visible ();
         !if (status & 1) return 0;
         return status;
     }
#v-
\seealso{_is_article_visible, is_article_window_zoomed, call}
\done

\function{is_article_window_zoomed}
\usage{Integer is_article_window_zoomed ()}
\description
   This function returns 1 if the article window is zoomed, or 0
   otherwise.
\seealso{is_article_visible, call}
\done

\function{pipe_article}
\usage{Void pipe_article (String cmd)}
\description
   This function may be used to pipe the current article to the command
   given by the \var{cmd} argument.  If the article window is hidden, it
   downloads the article associated with the currently selected header.
\seealso{read_mini}
\done

\function{raw_article_as_string}
\usage{String_Type raw_article_as_string ()}
\description
   This function works exactly like \var{article_as_string}, except that it
   returns the article as it was sent by the server, not as it has been
   prepared for display (e.g. before MIME decoding and character recoding).
\notes
   The raw article is no longer available after you called
   \var{replace_article}. In this case, the output is the same as the one of
   \var{article_as_string}.
\done

\function{re_bsearch_article}
\usage{Integer re_bsearch_article (String_Type pat)}
\description
   This function works like re_search_article, but does a backward search.
\seealso{re_search_article}
\done

\function{re_search_article}
\usage{Integer re_search_article (String_Type pat)}
\description
   This function searches forward in the current article for a string
   matching the regular expression given by the parameter \var{pat}.  It
   returns 0 if no matching line is found.  Otherwise, it returns 1 and the
   matching line will be left on the stack as a string.
\seealso{re_search_article_first}
\seealso{search_article}
\done

\function{re_search_article_first}
\usage{Integer re_search_article_first (String_Type pat)}
\description
   Works like re_search_article, but finds the first match in the article
   (searching from the beginning instead of forward from the current point).
\seealso{re_search_article}
\done

\function{replace_article}
\usage{replace_article (String_Type string [,Int_Type process_mime])}
\description
  The \var{replace_article} may be used to replace the text of
  the currently displayed article with an arbitrary string.  If the
  value of the optional \exmp{process_mime} argument is non-0, then
  the new headers will be examined and mime parsed accordingly.
\example
  The following code fragment causes the text of an article to be
  replaced by its lowercase equivalent:
#v+
     replace_article (strlow (article_as_string ()));
#v-
\seealso{article_as_string, is_article_visible}
\done

\function{request_body}
\usage{request_body (Integer mode)}
\description
  If an article does not yet have a body, you can request it for download
  using this function. Setting \var{mode} to 1 requests the body, setting it
  to 0 un-requests it.
\seealso{get_body_status}
\done

\function{save_current_article}
\usage{Integer save_current_article (String filename)}
\description
   This function saves the currently selected article to a file specified by
   \var{filename}.  If the article window is hidden, it downloads the the
   article associated with the currently selected header.  It returns 0 upon
   success; upon failure, it returns -1 and sets an slang error condition.
\notes
   This function always creates a new file, overwriting existing ones.
\seealso{}
\done

\function{search_article}
\usage{Integer search_article (String str)}
\description
   This function searches forward in the current article (if none is
   visible, in the one associated with the currently selected header) for
   the string given by the parameter \var{str}.  It returns 0 if no matching
   line is found.  Otherwise, it returns 1 and the matching line will be
   left on the stack as a string.
\seealso{re_search_article}
\seealso{search_article_first}
\done

\function{search_article_first}
\usage{Integer search_article_first (String_Type pat)}
\description
   Works like search_article, but finds the first match in the article
   (searching from the beginning instead of forward from the current point).
   This means you can find all matches in the article by calling
   search_article_first once and subsequently using search_article.
\seealso{search_article}
\done

\function{set_article_window_size}
\usage{Void set_article_window_size (Integer nrows)}
\description
   The \var{set_article_window_size} may be used to set the height of the
   article window.  The variable \var{SCREEN_HEIGHT} may be used to
   facilitate this.
\seealso{get_article_window_size}
\done
