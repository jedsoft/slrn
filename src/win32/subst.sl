#!/usr/bin/env slsh

private variable Script_Version_String = "0.1.0";

require ("cmdopt");
require ("readascii");

private define process_line (defs, line)
{
   variable new_line = "";
   variable len = strbytelen (line);
   variable istart = 0;
   variable i = istart;
   while (i < len)
     {
	variable ch = line[i];
	i++;

	if (ch != '@')
	  continue;

	new_line = strcat (new_line, substrbytes (line, istart+1, i-istart-1));
	istart = i;

	while (i < len)
	  {
	     ch = line[i]; i++;
	     if (ch == '@')
	       break;
	  }
	then 
	  {
	     istart--;
	     continue;
	  }

	% Get here only if break in above loop was hit
	variable word = substrbytes (line, istart+1, i-istart-1);
	if (word != strtrans (word, "^a-zA-Z0-9_", ""))
	  {
	     % Not a real word -- skip it
	     istart--;
	     i--;		       %  start with the @
	     continue;
	  }

	if (0 == assoc_key_exists (defs, word))
	  {
	     istart--;		       %  keep the @
	     () = fprintf (stderr, "*** Warning: %s is undefined\n", word);
	     continue;
	  }

	variable val = defs[word];
	new_line = strcat (new_line, val);
	istart = i;
     }
   
   new_line = strcat (new_line, substrbytes (line, istart+1, -1));
   
   return new_line;
}
	
private define exit_version ()
{
   () = fprintf (stdout, "Version: %S\n", Script_Version_String);
   exit (0);
}

private define exit_usage ()
{
   variable fp = stderr;
   () = fprintf (fp, "Usage: %s [options] file.def file.in file.out\n", __argv[0]);
   variable opts = 
     [
      "Options:\n",
      " -v|--version               Print version\n",
      " -h|--help                  This message\n",
     ];
   foreach (opts)
     {
	variable opt = ();
	() = fputs (opt, fp);
     }
   exit (1);
}

private define read_defs_file (file)
{
   variable vars, vals;
   () = readascii (file, &vars, &vals; format="%s %s", comment="%");
   variable defs = Assoc_Type[String_Type];
   _for (0, length (vars)-1, 1)
     {
	variable i = ();
	defs[vars[i]] = vals[i];
     }
   return defs;
}

define slsh_main ()
{
   variable c = cmdopt_new ();

   c.add("h|help", &exit_usage);
   c.add("v|version", &exit_version);

   variable i = c.process (__argv, 1);

   if (i + 3!= __argc)
     exit_usage ();
   
   variable def_file = __argv[i];
   variable in_file = __argv[i+1];
   variable out_file = __argv[i+2];

   variable defs = read_defs_file (def_file);
   variable fpin = stdin, fpout = stdout;
   
   if (in_file != "-")
     {
	fpin = fopen (in_file, "r");
	if (fpin == NULL)
	  {
	     () = fprintf (stderr, "Unable to read %s\n", in_file);
	     exit (1);
	  }
     }
   if (out_file != "-")
     {
	fpout = fopen (out_file, "w");
	if (fpout == NULL)
	  {
	     () = fprintf (stderr, "Unable to open %s\n", out_file);
	     exit (1);
	  }
     }
   
   variable line;
   while (-1 != fgets (&line, fpin))
     {
	if (is_substr (line, "@"))
	  line = process_line (defs, line);
	if (-1 == fputs (line, fpout))
	  {
	     () = fprintf (stderr, "Write to %s failed\n", out_file);
	     exit (1);
	  }
     }
   if (-1 == fclose (fpout))
     {
	() = fprintf (stderr, "Write to %s failed\n", out_file);
	exit (1);
     }
   exit (0);
}
