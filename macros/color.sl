% This file implements a function called 'edit_colors' that may be used for
% designing a color scheme interactively.  You can define a keybinding for it
% (e.g. "ESC e c") by putting a line like this in your slrnrc file:
%
% setkey article edit_colors "\eec"
%
% The macro illustrates several things:
%
%   * How to create and use a linked list in the S-Lang language
%   * Interaction with files
%   * The slrn select_list_box function

variable Color_List_Root = NULL;

define color_save_colors_to_file ()
{
   variable file;
   variable fp;
   variable x;

   if (Color_List_Root == NULL)
     return;

#ifdef UNIX
   file = ".slrnrc";
#else
   file = "slrn.rc";
#endif

   if (1 != get_yes_no_cancel ("Save colors"))
     return;
   
   file = make_home_filename (file);
   file = read_mini ("Save colors to: ", "", file);
   !if (strlen (file))
     return;
   
   fp = fopen (file, "a");
   if (fp == NULL)
     verror ("Unable to open %s", file);

   x = Color_List_Root;
   while (x != NULL)
     {
	if ((x.fg != NULL) and (x.bg != NULL))
	  () = fputs (sprintf ("color\t%s\t%s\t%s\n", x.obj, x.fg, x.bg), fp);
	x = x.next;
     }
   
   () = fclose (fp);
}

define color_store_color (obj, fg, bg)
{
   variable x;
   
   x = Color_List_Root;
   while (x != NULL)
     {
	if (x.obj == obj)
	  break;
	
	x = x.next;
     }
   if (x == NULL) 
     {
	x = struct { obj, fg, bg, next };
	x.next = Color_List_Root;
	Color_List_Root = x;
	x.obj = obj;
     }
   
   x.fg = fg;
   x.bg = bg;
}



define color_get_color_for_object (title)
{
   variable n;
   
   n = _stkdepth ();
   return select_list_box (title,
			   "black",
			   "red",
			   "green",
			   "brown",
			   "blue",
			   "magenta",
			   "cyan",
			   "lightgray",
			   "gray",
			   "brightred",
			   "brightgreen",
			   "yellow",
			   "brightblue",
			   "brightmagenta",
			   "brightcyan",
			   "white",
			   "default",
			   _stkdepth () - n - 1,
			   0);
}

define edit_colors ()
{
   variable n, fg, bg;
   variable obj;
   
   forever 
     {
	n = _stkdepth ();
	obj = select_list_box ("Object",    %  title
			       "EXIT",
			       "article",
			       "author",
			       "boldtext",
			       "box",
			       "cursor",
			       "date",
			       "description",
			       "error",
			       "frame",
			       "from_myself",
			       "group",
			       "grouplens_display",
			       "header_name",
			       "header_number",
			       "headers",
			       "high_score",
			       "italicstext",
			       "menu",
			       "menu_press",
			       "message",
			       "neg_score",
			       "normal",
			       "pgpsignature",
			       "pos_score",
			       "quotes",
			       "quotes1",
			       "quotes2",
			       "quotes3",
			       "quotes4",
			       "quotes5",
			       "quotes6",
			       "quotes7",
			       "response_char",
			       "selection",
			       "signature",
			       "status",
			       "subject",
			       "thread_number",
			       "tilde",
			       "tree",
			       "underlinetext",
			       "unread_subject",
			       "url",
			       "verbatim",
			       _stkdepth () - n - 1,
			       0);

	if ((obj == "EXIT") or (obj == ""))
	  break;
	
	fg = color_get_color_for_object ("Foreground color for " + obj);
	if (fg == "") break;

	bg = color_get_color_for_object ("Background color for " + obj);
	if (bg == "") break;
	
	set_color (obj, fg, bg);
	color_store_color (obj, fg, bg);
	call ("redraw");
     }
   
   color_save_colors_to_file ();
}

