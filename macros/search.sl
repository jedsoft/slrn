% This macro allows one to search throgh the bodies of the articles in the
% current newsgroup.  It binds the function 'search_newsgroup' to the '$' key
% in article mode.
% 
% If you need more sophisticated features, you can use the ``improved search''
% available from <http://slrn.sourceforge.net/macros/>

variable Search_Last_Art_Search_Str = "";
define search_newsgroup ()
{
   variable str;
   variable flags;
   
   str = read_mini ("Search for regexp: ", Search_Last_Art_Search_Str, "");
   if (str == "")
     return;

   Search_Last_Art_Search_Str = str;
   
   uncollapse_threads ();
   call ("hide_article");
   
   do
     {
	flags = get_header_flags ();
	
	call ("article_line_up");
	if (re_search_article (str))
	  {
	     pop ();
	     return;
	  }
	
	set_header_flags (flags);
	
	call ("hide_article");
     }
   while (header_down (1));
   
   error ("Not found.");
}
definekey ("search_newsgroup", "$", "article");
