% This function prints the currently selected article on a printer attached
% to the terminal.  It demonstates the 
define tty_print_article () 
{
   variable buf;

   art_select_article ();
   
   if (get_yes_no_cancel ("Are you sure you want to print article") <= 0)
     return;

   buf = article_as_string ();

   message ("printing...");
   update ();
   
   tt_send ("\e[5i");
   tt_send (buf);
   tt_send ("\e[4i");

   message ("printing...done");
}


definekey ("tty_print_article", "^P", "article");
