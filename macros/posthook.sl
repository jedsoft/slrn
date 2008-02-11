% This file illustrates the use of 'post_file_hook', which gets called
% immediately before a message is posted.  In this example, the header
% and body parts of the message are separated, and a shell command is run
% on the body part, finally the head and body are re-assembled.

define post_file_hook_command (cmd, file)
{
   variable header_file, body_file;
   variable fp, header_fp, body_fp;
   variable line;
   
   if (1 != get_yes_no_cancel (sprintf ("Execute %s on message", cmd)))
     return;

   fp = fopen (file, "r");
   if (fp == NULL)
     return;

   header_file = file + "-header";
   body_file = file + "-body";
   
   header_fp = fopen (header_file, "w");
   body_fp = fopen (body_file, "w");
   if ((header_fp == NULL) or (body_fp == NULL))
     return;

   while (-1 != fgets (&line, fp))
     {
	if (line == "\n")
	  break;
	() = fputs (line, header_fp);
     }
   () = fclose (header_fp);
   
   % Now do body
   while (-1 != fgets (&line, fp))
     {
	() = fputs (line, body_fp);
     }
   () = fclose (body_fp);

   () = system (sprintf ("%s %s", cmd, body_file));
   
   fp = fopen (file, "w");
   body_fp = fopen (body_file, "r");
   header_fp = fopen (header_file, "r");
   
   while (-1 != fgets (&line, header_fp))
     () = fputs (line, fp);
   () = fputs ("\n", fp);
   
   while (-1 != fgets (&line, body_fp))
     () = fputs (line, fp);

   % No need to close files unless we want to check for errors.  When
   % file pointer variables go out of scope, slang will close the file.
}

   
define post_file_hook (file)
{
   % Note: the post_file_hook_command function may be called multiple times, 
   % e.g., once to spell-check the article, once to grammar check it, 
   % and so on.
   post_file_hook_command ("ispell -x", file);
}
