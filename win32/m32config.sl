require ("cmdopt");

private variable Script_Version_String = "0.1.0";
private variable Default_Install_Prefix = "C:/mingw/local";

private define exit_version ()
{
   () = fprintf (stdout, "Version: %S\n", Script_Version_String);
   exit (0);
}

private define exit_usage ()
{
   variable fp = stderr;
   () = fprintf (fp, "Usage: %s [options]\n", __argv[0]);
   variable opts =
     [
      "Options:\n",
      " -v|--version                             Print version\n",
      " -h|--help                                This message\n",
      " --prefix=/install/prefix                 Default is C:/mingw32/local\n",
      " --with-slang=/slang/install/prefix       slang install location\n",
      " --with-slrnpull=/path/to/slrnpull/root   Where slrnpull files are kept\n",
      " --destdir=/path                          Default is \"\"\n",
     ];
   foreach (opts)
     {
	variable opt = ();
	() = fputs (opt, fp);
     }
   exit (1);
}

% I originally coded this to use popen.  But pipes are broken under wine:
% <http://bugs.winehq.org/show_bug.cgi?id=25063>
% So, a temp file will be used.
private define subst_defs ()
{
   variable infile, outfile;
   (infile, outfile) = ();

   () = fprintf (stdout, "Creating %s from %s\n", outfile, infile);

   variable deffile_list = __pop_list (_NARGS-2);

   variable deffile_names = {};

   variable fp = NULL, tmp = NULL;

   foreach (deffile_list)
     {
	variable defs = ();
	if (typeof (defs) == Array_Type)
	  {
	     if (fp == NULL)
	       {
		  tmp = "win32/_tmpdefs.tmp";
		  fp = fopen (tmp, "w");
		  if (fp == NULL)
		    {
		       () = fprintf (stderr, "Unable to open %s\n", tmp);
		       exit (1);
		    }
	       }

	     foreach (defs)
	       {
		  variable def = ();
		  if (-1 == fprintf (fp, "%s\n", def))
		    {
		       () = fprintf (stderr, "Error writing to %s\n", tmp);
		       exit (1);
		    }
	       }

	     defs = tmp;
	     % drop
	  }
	list_append (deffile_names, defs);
     }

   deffile_names = strjoin (list_to_array (deffile_names), " ");

   if ((fp != NULL)
       && (-1 == fclose (fp)))
     {
	() = fprintf (stderr, "Error closing %s\n", tmp);
	exit (1);
     }

   variable cmd = "slsh win32/subst.sl ${deffile_names} $infile $outfile"$;

   if (0 != system (cmd))
     {
	() = fprintf (stderr, "%s failed\n", cmd);
	exit (1);
     }

   if (tmp != NULL)
     () = remove (tmp);
}

private define guess_slang_install_prefix ()
{
   variable prefix = _slang_install_prefix;
   if ((prefix == NULL) || (NULL == stat_file (prefix)))
     {
	prefix =Default_Install_Prefix;
	() = fprintf (stderr, "Unable to get _slang_install_prefix.  Assuming %s\n",
		      prefix);
     }
   Default_Install_Prefix = prefix;
   return prefix;
}

define slsh_main ()
{
   variable c = cmdopt_new ();
   variable destdir = "";
   variable slang_prefix = guess_slang_install_prefix();
   variable prefix = slang_prefix;
   variable slrnpull_root = "";

   c.add("h|help", &exit_usage);
   c.add("v|version", &exit_version);
   c.add("destdir", &destdir; type="str");
   c.add("prefix", &prefix; type="str");
   c.add("with-slang", &slang_prefix; type="str");
   c.add("with-slrnpull", &slrnpull_root; type="str", optional="");

   variable i = c.process (__argv, 1);

   if (i != __argc)
     exit_usage ();

   variable slanglib = slang_prefix + "/lib", slanginc = slang_prefix + "/include";

   if (slrnpull_root == "")
     slrnpull_root = "$prefix/var/spool/news/slrnpull"$;
   variable build_slrnpull = (slrnpull_root != NULL);

   variable defs = ["PREFIX $prefix "$, "DESTDIR $destdir"$,
		    "SLANGINC $slanginc"$, "SLANGLIB $slanglib"$,
		    "SLRNPULL_ROOT_DIR ${slrnpull_root}"$,
		   ];
   subst_defs (defs, "win32/makefile.m32in", "Makefile");
   subst_defs (defs, "src/win32/makefile.m32in", "src/Makefile");
   subst_defs (defs, "src/win32/slrnfeat.def", "src/slrnfeat.hin", "src/slrnfeat.h");

   % Remove config from a previous build
   () = remove ("src/config.h");

   () = fputs ("\
\n\
  Now run mingw32-make to build slrn.  But first, you should\n\
  look at Makefile and change the installation locations if necessary.\n\
  Also look at src/slrnfeat.h.\n\
\n", stdout);
}
