% Mode for editing slrn score files

% To use this, add the following line to your .jedrc file:
%    autoload ("score_mode", "score");

% Align the ':' characters in the same column
define score_indent_line ()
{
   variable col, colon;
   push_spot ();
   bol_skip_white ();

   EXIT_BLOCK {
      pop_spot ();
      if (bolp ()) skip_white ();
   }

   col = what_column ();
   if (looking_at_char ('[') or looking_at_char ('%')) col = 1;
   else if (eolp ())
     {
	push_spot ();
	bskip_chars ("\n\t ");
	bol_skip_white ();
	col = what_column ();
	pop_spot ();
     }
   else if (ffind_char (':'))
     {
	colon = what_column ();
	if (blooking_at ("Score")) colon -= 10; else colon -= 18;
	!if (colon) return;
	col -= colon;
     }
   else
     return;

   if (what_column () != col)
     {
	bol ();
	trim ();
	col--;
	whitespace (col);
     }
}

$1 = "score";
create_syntax_table ($1);
set_syntax_flags ($1, 0x20);
define_syntax ("%", "", '%', $1);
define_syntax ("([{", ")]}", '(', $1);
define_syntax ('\\', '\\', $1);
define_syntax ("-a-zA-Z:", 'w', $1);	% words
define_syntax ("-0-9", '0', $1);	% Numbers
define_syntax ('[', '#', $1);

() = define_keywords ($1, "Age:", 4);
() = define_keywords ($1, "Date:From:Xref:", 5);
() = define_keywords ($1, "Lines:Score:", 6);
() = define_keywords ($1, "Score::", 7);
() = define_keywords ($1, "Expires:Subject:", 8);
() = define_keywords ($1, "Has-Body:", 9);
() = define_keywords ($1, "Newsgroup:", 10);
() = define_keywords ($1, "Message-Id:References:", 11);

define score_mode ()
{
   variable score = "score";
   set_mode (score, 0);
   use_syntax_table (score);
   set_buffer_hook ("indent_hook", "score_indent_line");
   runhooks ("score_mode_hook");
   % called after the hook to give a chance to load the abbrev table
   if (abbrev_table_p (score)) use_abbrev_table (score);
}

% This function may be called by jed when starting newsreader

define score_arrange_score ()
{
   % See if this is a score file
   variable mode;
   variable group;
   variable score;
   variable group_re = "^[ \t]*\\(\\[.*\\]\\)";
     
   (mode, ) = what_mode ();
   if (strcmp(mode, "score")) return;
   
   push_spot ();
   EXIT_BLOCK 
     {
	pop_spot ();
     }
   
   % Find name of group for the score
   !if (re_fsearch (group_re)) return;
   group = regexp_nth_match (1);

   % indent the region
   push_spot ();
   do 
     {
	score_indent_line ();
	eol ();
	trim ();
     }
   while (down(1));
   pop_spot ();
   
   !if (bol_bsearch (group)) return;
   
   push_mark ();
   pop_spot ();
   bol ();
   push_mark ();
   push_mark ();
   eob ();
   score = bufsubstr ();
   del_region ();
   
   pop_mark_1 ();
   eol ();
   !if (re_fsearch (group_re)) eob ();
   insert (score);
   if (re_bsearch (group_re)) delete_line ();
   push_spot ();
   return;
}
   
