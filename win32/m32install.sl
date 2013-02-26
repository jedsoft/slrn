private variable Script_Version_String = "0.1.1";

require ("cmdopt");
require ("glob");

private variable MANDIR = "$root/share/man/man1";
private variable BINDIR = "$root/bin";
private variable DOCDIR = "$root/share/doc/slrn";
private variable CONFDIR = "$root/etc";
%private variable LOCALEDIR = "$root/share/locale";
private variable SLRN_SLANG_DIR = "$root/share/slrn/slang";

private variable BIN_FILES = ["src/slrn.exe", "src/slrnpull.exe"];
private variable MAN_FILES = ["doc/slrn.1", "doc/slrnpull.1"];
private variable DOC_FILES
  = ["COPYRIGHT", "COPYING", "README", "changes.txt", "@doc/INSTFILES"];
private variable SLANG_FILES = ["@macros/INSTFILES"];
private variable CONF_FILES = NULL;

%---------------------------------------------------------------------------
private define convert_path (path)
{
   return strtrans (path, "/", "\\");
}

private define mkdir_p (dir);
private define mkdir_p (dir)
{
   dir = convert_path (dir);

   if ((-1 == mkdir (dir)) && (errno != EEXIST))
     {
	variable parent = path_dirname (dir);
	if ((-1 == mkdir_p (parent))
	    || (-1 == mkdir (dir)))
	  {
	     () = fprintf (stderr, "Failed to create %s: %s\n",
			   dir, errno_string ());
	     exit (1);
	  }
     }
   return 0;
}

private define run_cmd (cmd)
{
   () = system (cmd);
}

private define install_files_array (file, dir);   %  forward decl
private define install_file (file, dir)
{
   dir = convert_path (dir);
   file = convert_path (file);

   if (file[0] == '@')
     {
	file = file[[1:]];
	variable fp = fopen (file, "r");
	if (fp == NULL)
	  {
	     () = fprintf (stderr, "Unable to open %S\n", file);
	     exit (1);
	  }
	variable files = fgetslines (fp);
	() = fclose (fp);
	files = array_map (String_Type, &strtrim, files);
	files = files[where (files != "")];
	files = files[where (array_map (Int_Type, &strncmp, files, "#", 1))];
	files = files[where (array_map (Int_Type, &strncmp, files, "%", 1))];
	files = array_map (String_Type, &path_concat, path_dirname(file), files);
	install_files_array (files, dir);
	return;
     }

   () = fprintf (stdout, "Installing %s into %s\n", file, dir);
   run_cmd ("copy /y $file $dir"$);
}

private define install_files_array (files, dir)
{
   foreach (files)
     {
	variable file = ();
	variable f = file;
	if (f[0] == '@')
	  f = f[[1:]];
	if (NULL == stat_file (f))
	  {
	     () = fprintf (stderr, "*** WARNING: Unable to stat %s -- skipping it\n", f);
	     continue;
	  }
	install_file (file, dir);
     }
}

private define install_files (pat, dir)
{
   install_files_array (glob (pat), dir);
}

private define install_slrn (root, pullroot)
{
   variable dir;

   dir = _$(BINDIR); () = mkdir_p (dir);
   install_files_array (BIN_FILES, dir);

   dir = _$(SLRN_SLANG_DIR); () = mkdir_p (dir);
   install_files_array (SLANG_FILES, dir);

   dir = _$(MANDIR); () = mkdir_p (dir);
   install_files_array (MAN_FILES, dir);

   dir = _$(DOCDIR); () = mkdir_p (dir);
   install_files_array (DOC_FILES, dir);

   dir = _$(CONFDIR); () = mkdir_p (dir);
   install_files_array (CONF_FILES, dir);

   () = mkdir_p (pullroot);
}

private define strip_drive_prefix (path)
{
   if ((strlen(path) >= 3)
       && (path[1] == ':'))
     path = path[[2:]];

   return path;
}

private define exit_version ()
{
   () = fprintf (stdout, "Version: %S\n", Script_Version_String);
   exit (0);
}

private define exit_usage ()
{
   variable fp = stderr;
   () = fprintf (fp, "Usage: %s [options] install\n", __argv[0]);
   variable opts =
     [
      "Options:\n",
      " -v|--version               Print version\n",
      " -h|--help                  This message\n",
      " --prefix=/install/prefix   Default is /usr\n",
      " --distdir=/path            Default is blank\n",
      " --slrnpull=/path           Default is $prefix/var/spool/news/slrnpull\n",
     ];
   foreach (opts)
     {
	variable opt = ();
	() = fputs (opt, fp);
     }
   exit (1);
}

define slsh_main ()
{
   variable c = cmdopt_new ();
   variable destdir = "";
   variable prefix = "/usr";
   variable slrnpull_root = NULL;
   c.add("h|help", &exit_usage);
   c.add("v|version", &exit_version);
   c.add("destdir", &destdir; type="str");
   c.add("prefix", &prefix; type="str");
   c.add("slrnpull", &slrnpull_root; type="str");

   variable i = c.process (__argv, 1);

   if ((i + 1 != __argc) || (__argv[i] != "install"))
     exit_usage ();

   () = fprintf (stdout, "Using destdir=%s, prefix=%s\n", destdir, prefix);


   if (slrnpull_root == NULL)
     slrnpull_root = "$prefix/var/spool/news/slrnpull"$;

   if (destdir != "")
     {
	prefix = destdir + strip_drive_prefix (prefix);
	slrnpull_root = destdir + strip_drive_prefix (slrnpull_root);
     }

   install_slrn (prefix, slrnpull_root);
}
