
define group_bob ()
{
   call ("bob");
}

define group_eob ()
{
   call ("eob");
}

define header_bob ()
{
   call ("header_bob");
}

define header_eob ()
{
   call ("header_eob");
}

define art_quit ()
{
   call ("quit");
}

define art_select_article ()
{
   !if (3 == is_article_visible ())
     call ("article_page_down");
}

define art_hide_article_window ()
{
   if (is_article_visible ())
     call ("hide_article");
}

define mark_spot ()
{
   call ("mark_spot");
}

define goto_spot ()
{
   call ("exchange_mark");   
}

define star_tag_header () 
{
   if (is_thread_collapsed ())
     {
	uncollapse_thread ();
	loop (thread_size () - 1)
	  {
	     () = header_down (1);
	     set_header_flags (get_header_flags () | HEADER_TAGGED);
	  }
	collapse_thread ();
     }
	
   set_header_flags (get_header_flags () | HEADER_TAGGED);
}

define star_untag_header () 
{
   set_header_flags (get_header_flags () & ~(HEADER_TAGGED));
}


