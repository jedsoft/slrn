The intrinsic functions described in this chapter are available in article
mode and allow you to manipulate the header window.

\function{collapse_thread}
\usage{Void collapse_thread ()}
\description
   This function may be used to collapse the current thread.
\seealso{uncollapse_thread, collapse_threads, is_thread_collapsed}
\done

\function{collapse_threads}
\usage{Void collapse_threads ()}
\description
   This function will collapse all threads in the current newsgroup.
\seealso{uncollapse_threads}
\done

\function{extract_article_header}
\usage{String_Type extract_article_header (String h)}
\description
   This function returns the article header line specified by the
   header keyword \var{h} of the currently selected header.  The
   currently selected header may correspond to the currently
   displayed article.  To get a header of the currently displayed
   article, use the \var{extract_displayed_article_header} function.

   If the header does not exist, it returns the empty string.
\notes
   This function will not query the server.  If you are looking for a
   non-NOV header which was not stored for expensive scoring, then download
   the message associated with the current header line.
\seealso{extract_displayed_article_header, is_article_visible}
\done

\function{extract_displayed_article_header}
\usage{String_Type extract_displayed_article_header (String h)}
\description
   This function returns the article header line specified by the
   header keyword \var{h} of the currently displayed message.
   If the header does not exist, it returns the empty string.
\seealso{extract_displayed_article_header, is_article_visible}
\done

\function{get_grouplens_score}
\usage{Integer get_grouplens_score ()}
\description
   This function returns the grouplens score of the current header.
   If the header has no grouplens score, or if grouplens support has
   not been enabled, 0 will be returned.
\done

\function{get_header_flags}
\usage{Integer get_header_flags ()}
\description
   This functions returns the flags for the current header.  This
   integer is a bitmapped value whose bits are defined by the following
   constants:
#v+
      HEADER_READ : set if header is marked as read
      HEADER_TAGGED : set if header has `*' tag
      HEADER_HIGH_SCORE : set if header has high score
      HEADER_LOW_SCORE : set if header has low score
#v-
\seealso{set_header_flags}
\done

\function{get_header_number}
\usage{Integer get_header_number ()}
\description
   This function returns the article number for the current header (i.e. the
   one assigned by the server and recorded in the newsrc file). If you want
   the current cursor position instead, use \var{header_cursor_pos}.
\done

\function{get_header_score}
\usage{Integer get_header_score ()}
\description
   This functions returns the score for the current header.
\seealso{set_header_score}
\done

\function{get_header_tag_number}
\usage{Integer get_header_tag_number ()}
\description
   This function returns the value of the numerical tag associated
   with the current header.  If the header has no numerical tag, zero
   is returned.
\done

\function{get_visible_headers}
\usage{String_Type get_visible_headers ()}
\description
  The \var{get_visible_headers} function returns the list of headers
  headers that are to be displayed when an article is viewed.  See the
  documentation for the \var{set_visible_headers} for the format of
  this string.
\seealso{set_visible_headers, is_article_visible, set_header_display_format}
\done

\function{goto_num_tagged_header}
\usage{Integer goto_num_tagged_header (Integer n)}
\description
   This function causes the header with numerical tag \var{n} to become the
   current header.  It returns 1 upon success or 0 upon failure.
\seealso{header_down, get_header_flags, call}
\done

\function{has_parent}
\usage{Integer has_parent ()}
\description
   Returns 1 if the current header has a parent (within a thread tree), 0
   otherwise.
\done

\function{header_cursor_pos}
\usage{Integer header_cursor_pos ()}
\description
   This function returns the current position of the cursor in the header
   summary window. This is the same as the ``header number'' of the article
   that gets displayed if the \var{use_header_numbers} config variable is
   turned on, so it is always a number in the range 1 through
   \var{SCREEN_HEIGHT}-3; do not confuse it with the article number assigned
   by the server (which can be obtained using the intrinsic function
   \var{get_header_number}). If the article pager is ``zoomed'', this
   function always returns 1.
\done

\function{header_down}
\usage{Integer header_down (Integer n)}
\description
   The function moves the current position down \var{n} headers.  It
   returns the number that was actually moved.
\seealso{header_up}
\done

\function{header_next_unread}
\usage{Integer header_next_unread ()}
\description
   Goto next unread header. When reading from an slrnpull spool, headers for
   which the article body is not present are skipped. The function returns
   one upon success or zero upon failure.
\seealso{header_down}
\done

\function{header_up}
\usage{Integer header_up (Integer n)}
\description
   The function moves the current position up \var{n} headers.  It
   returns the number that was actually moved.
\seealso{header_down}
\done

\function{headers_hidden_mode}
\usage{Int_Type headers_hidden_mode ()}
\description
   This function may be used to determine whether or not some headers
   will be hidden when an article is displayed.  It returns 0 is all
   headers will be displayed, or a non-zero value if some may be hidden.
\seealso{set_visible_headers, get_visible_headers, is_article_visible}
\done

\function{is_thread_collapsed}
\usage{Integer is_thread_collapsed ()}
\description
   If the current header is the start of a collapsed thread, this
   function will return a non-zero value.  If the thread is expanded,
   zero will be returned.
\seealso{collapse_thread}
\done

\function{locate_header_by_msgid}
\usage{Int_Type locate_header_by_msgid (String_Type msgid, Int_Type qs)}
\description
   The \var{locate_header_by_msgid} function may be used to set the
   current header to one whose message-id is given by \var{msgid}.  If
   the second parameter \var{qs} is non-zero, then the header will be
   retrieved from the server if it is not in the current list of
   headers.  The function returns \var{1} if an appropriate header was
   found, or \var{0} otherwise.
\example
   One possible use of this function is to mark the current position
   in the header list and return to that position later, e.g.,
#v+
       % Save the current position
       variable msgid = extract_article_header ("Message-ID");
          .
          .
       % Return to previous position.
       () = locate_header_by_msgid (msgid, 0);
#v-
\done

\function{next_tagged_header}
\usage{Integer next_tagged_header ()}
\description
   This function moves the current header position to the next \exmp{*}
   tagged header.  It returns non-zero upon success or zero upon
   failure.
\seealso{prev_tagged_header, goto_num_tagged_header, header_up, header_down}
\done

\function{prev_tagged_header}
\usage{Integer prev_tagged_header ()}
\description
   This function moves the current header position to the previous `*'
   tagged header.  It returns non-zero upon success or zero upon
   failure.
\seealso{next_tagged_header, goto_num_tagged_header, header_up, header_down}
\done

\function{re_bsearch_author}
\usage{Integer re_bsearch_author (String regexp)}
\description
   Search backward for header whose author matches regular expression
   \var{regexp}. If successful, it returns 1 and the current header is set
   to the matching header.  It returns 0 upon failure.
\seealso{re_fsearch_author, re_fsearch_subject}
\done

\function{re_bsearch_subject}
\usage{Integer re_bsearch_subject (String regexp)}
\description
   Search backward for header whose subject matches regular expression
   \var{regexp}. If successful, it returns 1 and the current header is set
   to the matching header.  It returns 0 upon failure.
\seealso{re_fsearch_author, re_bsearch_subject}
\done

\function{re_fsearch_author}
\usage{Integer re_bsearch_author (String regexp)}
\description
   Search forward for header whose author matches regular expression
   \var{regexp}. If successful, it returns 1 and the current header is set
   to the matching header.  It returns 0 upon failure.
\seealso{re_bsearch_author, re_fsearch_subject}
\done

\function{re_fsearch_subject}
\usage{Integer re_fsearch_subject (String regexp)}
\description
   Search forward for header whose subject matches regular expression
   \var{regexp}. If successful, it returns 1 and the current header is set
   to the matching header.  It returns 0 upon failure.
\seealso{re_fsearch_author, re_bsearch_subject}
\done

\function{set_header_display_format}
\usage{Void set_header_display_format (Int_Type nth, String_Type fmt)}
\description
   The \var{set_header_display_format} function may be used to set the
   \var{nth} header display format to \var{fmt}.  One may
   interactively toggle between the formats via the
   \var{toggle_header_formats} keybinding.
   
   The generic format specifier begins with the \exmp{%} character and must
   be of the form:
#v+
        %[-][w]x
#v-
   where the brackets indicate optional items.  Here, \em{w} is a width
   specifier consisting of one or more digits.  If the minus sign (-)
   is present, the item will be right justified, otherwise it will be
   left justified.  The item specifier \em{x} is required and, depending
   on it value, has the following meaning:
#v+
         s : subject
         S : score
         r : author real name
         f : from header
         G : Group lens score
         l : Number of lines
         n : server number
         d : date
         t : thread tree
         F : flags (read/unread, `*' and `#' tags, header number)
         % : percent character
         g : goto a specified column
#v-
   Thus, \exmp{"%F%-5l:%t%s"} indicates that the header window will contain
   the, in order: the flags, the number of lines the article contains
   right justified in a 5 character field, a `:', the tree, and the
   subject.
   
   The \var{g} format specifier must be preceeded by a number that
   indicates the next write should take place at the specified column.
   If the column number is negative, then the column is interpreted as
   an offset from the right side of the display.  For example,
   \exmp{%-24g%f} indicates that then \em{From} header is to
   be written out 24 columns from the right edge of the window.
\seealso{set_visible_headers}
\done

\function{set_header_flags}
\usage{Void set_header_flags (Integer flags)}
\description
   This function may be used to set the flags associated with the
   currently selected header.  See the description for the
   \var{get_header_flags} function for more information.
\seealso{get_header_flags}
\done

\function{set_header_score}
\usage{Void set_header_score (Integer score)}
\description
   This function may be used to set the score of the current header.
\seealso{get_header_score}
\done

\function{set_visible_headers}
\usage{Void set_visible_headers (String_Type header_list)}
\description
  The \var{set_visible_headers} function may be used to specify the
  headers that are displayed when an article is viewed.  The string 
  \var{header_list} specifies a comma separated list of headers to
  show.
\example
  To show only the From header and headers that start with `X-', use:
#v+
     set_visible_headers ("X-,From:");
#v-
\seealso{get_visible_headers, headers_hidden_mode, is_article_visible, set_header_display_format}
\done

\function{sort_by_sorting_method}
\usage{Void sort_by_sorting_method ()}
\description
   This function sorts the articles in header overview by the current
   sorting mode.
\example
   This is useful if you want to apply changes to \var{sorting_method}:
#v+
     set_integer_variable("sorting_method", 3);
     sort_by_sorting_method ();
#v-
\done

\function{thread_size}
\usage{Integer thread_size ()}
\description
   This function returns the number of articles in the current thread
   or subthread.
\done

\function{uncollapse_thread}
\usage{Void uncollapse_thread ()}
\description
   This function may be used to uncollapse the current thread.
\seealso{thread_size, collapse_thread, is_thread_collapsed}
\done

\function{uncollapse_threads}
\usage{Void uncollapse_threads ()}
\description
   This function uncollapses all threads.  This is usually necessary if you
   want to use the header movement functions to access hidden headers.
\seealso{collapse_threads}
\done
