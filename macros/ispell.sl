% This macro demonstrates the use of the post_filter_hook

%static
define ispell_file (file)
{
   variable cmd = "ispell -x";
   () = system (sprintf ("%s '%s'", cmd, file));
}


define post_filter_hook (file)
{
   variable rsp;
   variable cmd;
   
   forever 
     {
	rsp = get_response ("NnIi",
			    "Select Filter? \001None, \001Ispell");
	
	switch (rsp)
	  {
	   case 'i' or case 'I':
	     ispell_file (file);
	  }
	  {
	   case 'n' or case 'N':
	     return;
	  }
     }
}

