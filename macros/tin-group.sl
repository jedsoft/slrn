% The macros here provide some tin-like keybindings for GROUP mode.

% The following macros depend on the util.sl macros being loaded.
% In your .slrnrc file, add the following lines:
%    interpret "util.sl"
%    interpret "tin-group.sl"
%    interpret "tin-art.sl"


!if (is_defined ("group_bob")) () = evalfile ("util.sl");


% The easy bindings
definekey ("toggle_list_all", "y", "group");		% yank in/out
definekey ("toggle_hidden", "r", "group");		% toggle all/unread
definekey ("post", "w", "group");
definekey ("quit","Q","group");
definekey ("group_search_forward","/","group");   
definekey ("help","h","group");   


% TAB key in group mode.  Apparantly this moves from one unread group to
% another and then cycles back to the top.
define tin_group_next_unread ()
{
   USER_BLOCK0
     {
	if (group_unread ())
	  {
	     if (-1 != select_group ())
	       return;
	  }
     }
   
   X_USER_BLOCK0 ();  % enter the current group if it has unread articles
   while (group_down_n (1)) X_USER_BLOCK0 ();
   
   group_bob ();
   do X_USER_BLOCK0 (); while (group_down_n (1));
   
   error ("No more unread groups.");
}
definekey ("tin_group_next_unread", "\t", "group");


% Using UP/DOWN arrow keys in group mode causes a wrap at 
% ends of the buffer.
define tin_group_up ()
{
   !if (group_up_n (1))
     group_eob ();
}
definekey ("tin_group_up", "k", "group");

define tin_group_down ()
{
   !if (group_down_n (1))
     group_bob ();
}
definekey ("tin_group_down", "j", "group");

#ifdef UNIX VMS
definekey ("tin_group_up", "\e[A", "group");
definekey ("tin_group_up", "\eOA", "group");
definekey ("tin_group_up", "^(ku)", "group");
definekey ("tin_group_down", "\e[B", "group");
definekey ("tin_group_down", "\eOB", "group");
definekey ("tin_group_down", "^(kd)", "group");

% Left/right arrow keys
definekey ("quit", "\e[D", "group");
definekey ("quit", "\eOD", "group");
definekey ("quit", "^(kl)", "group");
definekey ("select_group", "\e[C", "group");
definekey ("select_group", "\eOC", "group");
definekey ("select_group", "^(kr)", "group");

#else  
% OS/2
definekey ("tin_group_up", "\xE0H", "group");
definekey ("tin_group_down", "\xE0P", "group");
definekey ("quit", "\xE0K", "group");
definekey ("select_group", "\xE0M", "group");
#endif


% using the space to pagedown at the end of the list should wrap to the top
define tin_group_pagedown ()
{
   call ("page_down");
}
definekey ("tin_group_pagedown"," ","group");


% using the b to pageup at the top of the list should wrap to the bottom
define tin_group_pageup ()
{
   call ("page_up");
}
definekey ("tin_group_pageup","b","group");

% vim:set noexpandtab tw=0 ts=8 softtabstop=3:
