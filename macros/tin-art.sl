
!if (is_defined ("tin_group_next_unread")) () = evalfile ("tin-group.sl");

% Simple mappings
definekey ("article_bob", "^R", "article");
definekey ("article_eob", "$", "article");

definekey ("article_bob", "g", "article");
definekey ("article_eob", "G", "article");
definekey ("article_page_down","\r","article");
definekey ("fast_quit","Q","article");
definekey ("undelete","z","article");
definekey ("undelete","Z","article");
definekey ("toggle_headers","^h","article");
undefinekey ("^k", "article");
definekey ("create_score","^k","article");
definekey ("post","w","article");
definekey ("delete","K","article");
definekey ("forward","m","article");
definekey ("author_search_forward","a","article");
definekey ("author_search_backward","A","article");
definekey ("article_search","B","article");
definekey ("save","s","article");
definekey ("help","h","article");
definekey ("cancel","D","article");
definekey ("zoom_article_window","\ez","article"); % replacement for slrn zoom mapping
definekey ("forward_digest","\eg","article"); % replacement for slrn digest mapping


define tin_next_listed_group ()
{
   art_quit ();
   if (group_down_n (1))
     {
	if (-1 != select_group ())
	   return;
     }
   error ("No more groups.");
}


define tin_prev_listed_group ()
{
   art_quit ();
   if (group_up_n (1))
     {
	if (-1 != select_group ())
	   return;
     }
   error ("No more groups.");
}


define tin_next_listed_article ()
{
   if (header_down (1))
     {
	art_select_article ();
	return;
     }
   error ("No more articles.");
}


define tin_prev_listed_article ()
{
   if (header_up (1))
     {
	art_select_article ();
	return;
     }
   error ("No more articles.");
}


define tin_next_listed ()
{
   if (is_article_visible ())
     {
	return tin_next_listed_article ();
     }
   return tin_next_listed_group ();
}
definekey ("tin_next_listed","n","article");


define tin_prev_listed ()
{
   if (is_article_visible ())
     {
	return tin_prev_listed_article ();
     }
   return tin_prev_listed_group ();
}
definekey ("tin_prev_listed","p","article");


define tin_art_next_unread_or_group ()
{
   if (get_header_flags () & HEADER_READ)
     {
	!if (header_next_unread ())
	  {
	     header_bob ();
	     if (get_header_flags () & HEADER_READ)
	       {
		  !if (header_next_unread ())
		    {
		       !if (is_article_visible ())
			 {
			    art_quit ();
			    tin_group_next_unread ();
			    return;
			 }

		       art_hide_article_window ();
		       return;
		    }
	       }
	  }
     }
   !if (is_article_window_zoomed ())
      call ("zoom_article_window");
   art_select_article ();
}
definekey ("tin_art_next_unread_or_group", "\t", "article");


define tin_art_quit ()
{
   if (is_article_visible ())
     {
	art_hide_article_window ();
	return;
     }

   art_quit ();
}
definekey ("tin_art_quit", "q", "article");
#ifdef UNIX VMS
definekey ("tin_art_quit", "\e[D", "article");	% left arrow
definekey ("tin_art_quit", "\eOD", "article");	% left arrow
definekey ("tin_art_quit", "^(kl)", "article");
definekey ("art_select_article", "\e[C", "article");  % right arrow
definekey ("art_select_article", "\eOC", "article");  % right arrow
definekey ("art_select_article", "^(kr)", "article");
#else
definekey ("tin_art_quit", "\xE0K", "article");   % left arrow
definekey ("art_select_article", "\xE0M", "article");   % right arrow
#endif

define tin_space_key_cmd ()
{
   !if (is_article_visible ())
     {
	ERROR_BLOCK
	  {
	     _clear_error ();
	     header_bob ();
	  }
	call ("header_page_down");
	return;
     }

   call ("article_page_down");
}
definekey ("tin_space_key_cmd", " ", "article");


define tin_article_pageup ()
{
   !if (is_article_visible ())
     {
	ERROR_BLOCK
	  {
	     _clear_error ();
	     header_eob ();
	  }
	call ("header_page_up");
	return;
     }

   call ("article_page_up");
}
definekey ("tin_article_pageup","b","article");


define tin_article_down ()
{
   if (is_article_visible ())
     {
	call ("article_line_down");    % spirit of tin anyway  ;)
     }
   else
     {
	ERROR_BLOCK
	  {
	     _clear_error ();
	     header_bob ();
	  }
	call ("header_line_down");
	return;
     }
}
definekey ("tin_article_down","j","article");
#ifdef UNIX VMS
definekey ("tin_article_down","\e[B","article"); % down arrow
definekey ("tin_article_down","\eOB","article"); % down arrow
#endif

define tin_article_up ()
{
   if (is_article_visible ())
     {
	call ("article_line_up");    % spirit of tin anyway  ;)
     }
   else
     {
	ERROR_BLOCK
	  {
	     _clear_error ();
	     header_eob ();
	  }
	call ("header_line_up");
	return;
     }
}
definekey ("tin_article_up","k","article");
#ifdef UNIX VMS
definekey ("tin_article_up","\e[A","article");   % up arrow
definekey ("tin_article_up","\eOA","article");   % up arrow
#endif

define tin_art_catchup_quit ()
{
   call ("catchup_all");
   % Provide visual feedback that catchup worked
   update ();
   art_quit ();
   group_down_n (1);
}
definekey ("tin_art_catchup_quit", "c", "article");

define tin_art_catchup_next ()
{
   tin_art_catchup_quit ();
   tin_group_next_unread ();
}
definekey ("tin_art_catchup_next", "C", "article");


variable Last_Search_Str = "";
define re_subject_search_forward ()
{
   variable str;
   variable found;

   ERROR_BLOCK
     {
        () = header_up (1);
     }

   !if (header_down (1))
      header_bob ();

   str = read_mini ("Subject re-search fwd: ", Last_Search_Str,"");
   !if (strlen (str))
      return;

   Last_Search_Str = str;

   found=re_fsearch_subject (str);
   !if (found)
     {
	header_bob ();
        found=re_fsearch_subject (str);
     }
   !if (found)
      error ("Not found.");
}

define re_subject_search_backward ()
{
   variable str;
   variable found;

   ERROR_BLOCK
     {
        () = header_down (1);
     }

   !if (header_up (1))
      header_eob ();

   str = read_mini ("Subject re-search bwd: ", Last_Search_Str,"");
   !if (strlen (str))
      return;

   Last_Search_Str = str;

   found=re_bsearch_subject (str);
   !if (found)
     {
	header_eob ();
        found=re_bsearch_subject (str);
     }
   !if (found)
     {
	error ("Not found.");
     }
}


define tin_search_forward ()
{
   if (is_article_visible ())
     {
	call ("article_search");
	return;
     }
   re_subject_search_forward ();
   return;
}
definekey ("tin_search_forward", "/", "article");


define tin_search_backward ()
{
   if (is_article_visible ())
     {
	call ("article_search");
	return;
     }
   re_subject_search_backward ();
   return;
}
definekey ("tin_search_backward", "?", "article");


define tin_article_r ()
{
   if (is_article_visible ())
     {
	call ("reply");
	return;
     }
   call ("toggle_hidden");
   return;
}
definekey ("tin_article_r", "r", "article");


define tin_article_d ()
{
   if (is_article_visible ())
      call ("toggle_rot13");
      else call ("toggle_header_formats");
}
definekey ("tin_article_d","d","article");



% vim:set noexpandtab tw=0 ts=8 softtabstop=3:
