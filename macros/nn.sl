%% Please note that these macros depend on the the ones defined in the file
%% util.sl which also comes with slrn. Make sure to interpret it first.

%!% I have no idea what this file has to do with the nn newsreader.  
%!% It simply defines a set of macros for selecting articles and then reading
%!% the articles associated with the selected headers.   Several people have
%!% remarked that this is how one reads with nn.
%!% 
%!% Based on that idea, here is how the macros in this file work:
%!% 
%!%    1.  Tag headers by one of the following methods:
%!%
%!%        a. Move to header and press the ' key.
%!%        b. Select a header by pressing the header number associated
%!%           with it.
%!%        c. Select headers via author/subject regular expressions
%!% 
%!%    2.  After selection, read selected articles via:
%!%           [          Previous selected
%!%           ]          Next Selected

variable NN_Tag_Selection_Mode = 0;


define nn_tag_header ()
{
   NN_Tag_Selection_Mode = 1;
   star_tag_header ();
}


define header_number_hook () 
{
   nn_tag_header ();
}


define nn_read_prev_tagged () 
{
   !if (prev_tagged_header ())
     error ("No more tagged headers.");
   
   star_untag_header ();
   art_select_article ();
}



define nn_read_next_tagged () 
{
   mark_spot ();
   ERROR_BLOCK
     {
	goto_spot ();
     }
   
   if (NN_Tag_Selection_Mode)
     {
	NN_Tag_Selection_Mode = 0;
	header_bob ();
     }
   
   !if (get_header_flags () & HEADER_TAGGED)
     {
	!if (next_tagged_header ())
	  error ("No more tagged headers.");
     }
   
   star_untag_header ();
   art_select_article ();
}



define nn_tag_header_cmd ()
{
   nn_tag_header ();
   () = header_down (1);
}

define nn_tag_via_subject_regexp ()
{
   variable str;
   variable count = 0;
   
   str = read_mini ("Tag subjects pattern: ", "", "");
   !if (strlen (str))
     return;

   mark_spot ();
   
   uncollapse_threads ();
   while (re_fsearch_subject (str))
     {
	nn_tag_header ();
	count++;
	!if (header_down (1))
	  break;
     }
   collapse_threads ();

   goto_spot ();
   vmessage ("%d headers marked.", count);
}


definekey ("nn_tag_via_subject_regexp", "%", "article");
definekey ("nn_tag_header_cmd", "'", "article");
definekey ("nn_read_next_tagged", "]", "article");
definekey ("nn_read_prev_tagged", "[", "article");

