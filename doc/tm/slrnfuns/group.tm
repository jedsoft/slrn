The intrinsic functions described in this chapter are available in group
mode.

\function{current_newsgroup}
\usage{String current_newsgroup ()}
\description
   This function returns the name of the current newsgroup.
\seealso{server_name}
\done

\function{get_group_flags}
\usage{Integer get_group_flags ()}
\description
   This function returns the flags associated with the current
   newsgroup.  This integer is a bitmapped value whose bits are
   defined by the following constants:
#v+
     GROUP_UNSUBSCRIBED   : set if the group is unsubscribed
     GROUP_NEW_GROUP_FLAG : set if the group is new
#v-
\seealso{get_header_flags, set_group_flags, current_newsgroup}
\done

\function{get_group_order}
\usage{Array_Type get_group_order ()}
\description
   This function returns an array of strings that contains the names of all
   known groups in the current order.
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
\seealso{set_group_order}
\done

\function{group_down_n}
\usage{Integer group_down_n (Integer n)}
\description
   This function moves the current group pointer down \var{n} groups and
   returns the actual number moved.
\seealso{group_up_n, group_search, current_newsgroup}
\done

\function{group_search}
\usage{Integer group_search (String name)}
\description
   This function searches for a newsgroup containing the string
   \var{name}.  It also searches newsgroup descriptions.  A non-zero value
   is returned upon success or zero upon failure.
\notes
   This search may wrap.
\seealso{select_group, current_newsgroup}
\done

\function{group_unread}
\usage{Integer group_unread ()}
\description
   This function returns the number of unread articles in the current
   newsgroup.
\seealso{select_group, current_newsgroup, is_group_mode}
\done

\function{group_up_n}
\usage{Integer group_up_n (Integer n)}
\description
   This function moves the current group pointer up \var{n} groups and
   returns the actual number moved.
\seealso{group_down_n, group_search, current_newsgroup}
\done

\function{hide_current_group}
\usage{Void hide_current_group ()}
\description
   Hides the current group in the group window. Hidden groups can be
   displayed again by calling ``toggle_hidden''.
\done

\function{is_group_mode}
\usage{Integer is_group_mode ()}
\description
   This function returns non-zero if the current mode is group-mode.
\seealso{}
\done

\function{select_group}
\usage{Integer select_group ()}
\description
   This function may be used to select the current group.  It returns
   0 upon success or -1 upon failure.  It can fail if the group has no
   articles.
   
   Note that in some situations, this function will set an slang error
   condition. This includes cases in which the user interrupted transfer of
   article headers or all articles got killed by the scorefile.   
\seealso{current_newsgroup}
\done

\function{set_group_display_format}
\usage{Void set_group_display_format (Int_Type nth, String_Type fmt)}
\description
   This function may be used to set the \var{nth} group display format to
   \var{fmt}.  One may interactively toggle between the formats via the
   \var{toggle_group_formats} keybinding.

   The generic format is identical to the one described in
   \var{set_header_display_format}. The following descriptors are defined:
#v+
         F : Group flag (`U' for unsubscribed, `N' for new)
         d : Group description (needs to be downloaded once with slrn -d)
         g : goto a specified column
         h : ``High water mark'' (highest article number in the group)
         l : ``Low water mark'' (lowest article number in the group)
         n : Group name
         t : Total number of articles in the group (estimate)
         u : Number of unread articles in the group
#v-
\done

\function{set_group_flags}
\usage{Void set_group_flags (Integer flags)}
\description
   This function may be used to set the flags associated with the
   current newsgroup.
\seealso{get_group_flags}
\done

\function{set_group_order}
\usage{Void set_group_order (Array_Type names)}
\description
   When \var{names} is a one-dimensional array of strings (group names),
   slrn will sort the group list into the implied order.  Strings that do
   not match known groups are ignored; existing groups that are not included
   in \var{names} remain in their current (relative) order, but will be
   moved to the end of the list.
\example
   According to the above rule, it is possible to move a group to the top of
   the list by using it as the only element of \var{names}:
#v+
      set_group_order ("news.software.readers");
#v-
   Moving a group to the end of the list can be done by removing it from the
   list returned by get_group_order and calling set_group_order on the
   result.

   An example for a simple group sort based on this function can be found in
   the file gsort.sl that comes with slrn.
\notes
   This function is only available if slrn was compiled with S-Lang 1.4.x.
\seealso{get_group_order}
\done
