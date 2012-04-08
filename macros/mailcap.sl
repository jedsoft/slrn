% Interface to a mailcap file described by: <http://tools.ietf.org/html/rfc1524>
% Copyright (C) 2012 John E. Davis <jed@jedsoft.org>
%
% This may be distributed under the terms of the GNU General Public
% License.  See the file COPYING for more information.
%
% Public functions:
%
%   mc = mailcap_lookup_entry (content_type);
%   mc.view (message);
%   mailcap_remove_tmp_files ();
%

require ("rand");
private variable Mailcap_Files =
  "$HOME/.mailcap:/usr/local/etc/mailcap:/etc/mailcap:/usr/etc/mailcap"$;

private variable Mailcap_Table = NULL;

private define view_method ();
private variable Mailcap_Type = struct
{
   % methods
   view = &view_method,
   content_type,		       %  full header

   % private
   _type,
   _command,
   _needsterminal = 0,
   _copiousoutput = 0,
   _compose,
   _composetyped,
   _print,
   _edit,
   _test,
   _x11bitmap,
   _textualnewlines,
   _description,
   _nametemplate,
   _xconvert,
};

private define unquote_field (value)
{
   ifnot (is_substrbytes (value, "\\"))
     return value;

   variable newvalue = "";
   variable i = 0, n = strbytelen (value);
   while (i < n)
     {
	variable ch = value[i]; i++;
	if ((ch == '\\') && (i < n))
	  {
	     ch = value[i]; i++;
	  }
	newvalue = strcat (newvalue, char (-int(ch)));   %  byte-semantics
     }
   return newvalue;
}


private define split_mailcap_line (line)
{
   variable fields = strchop (line, ';', '\\');
   fields = array_map (String_Type, &strtrim, fields);
   return array_map (String_Type, &unquote_field, fields);
}

private define get_name_value (field)
{
   variable fields = strchop (field, '=', 0);
   variable name = strtrim (fields[0]);
   if (length (fields) == 1)
     return name, NULL;
   variable value = strtrim (strjoin (fields[[1:]], "="));
   if (value == "") value = NULL;
   return name, value;
}

private define create_mailcap_entry (fields)
{
   if (length (fields) < 2)
     return NULL;
   variable mc = @Mailcap_Type;
   mc._type = fields[0];
   mc._command = fields[1];
   variable field_names = get_struct_field_names (mc)[[2:]];
   foreach (fields[[2:]])
     {
	variable f = ();
	variable name, value;
	(name, value) = get_name_value (f);
	name = strlow (name);
	if (name == "needsterminal")
	  {
	     mc._needsterminal = 1;
	     continue;
	  }
	if (name == "copiousoutput")
	  {
	     mc._copiousoutput = 1;
	     continue;
	  }
	if (name == "x11-bitmap") name = "x11bitmap";
	if (name == "x-convert") name = "xconvert";
	try
	  {
	     set_struct_field (mc, "_"+name, value);
	  }
	catch AnyError;
     }
   return mc;
}

private define read_mailcap (file)
{
   variable fp = fopen (file, "r");
   if (fp == NULL)
     return;

   variable line;

   while (-1 != fgets (&line, fp))
     {
	line = strtrim_end (line);
	if ((line[0] == '#') || (line == ""))
	  continue;
	while (line[-1] == '\\')
	  {
	     line = line[[:-2]];
	     variable next_line;
	     if ((-1 == fgets (&next_line, fp))
		 || (next_line[0] == '\n'))
	       break;
	     line = strcat (line, strtrim_end (line, "\n"));
	  }

	variable fields = split_mailcap_line (line);
	variable mc = create_mailcap_entry (fields);
	if (mc != NULL)
	  list_append (Mailcap_Table, mc);
     }
   () = fclose (fp);
}

private define read_mailcap_files ()
{
   Mailcap_Table = {};
   variable files = getenv ("MAILCAPS");
   if (files == NULL)
     files = Mailcap_Files;
   foreach (strchop (files, ':', 0))
     {
	variable file = ();
	read_mailcap (file);
     }
}

% This implementation assumes that the executable portion of the command
% has no whitespace.  It returns the expanded name, or NULL.
private define command_exists (command)
{
   command = strtok (command)[0];
   variable paths;
   if (path_is_absolute (command))
     paths = path_dirname (command);
   else
     {
	paths = getenv ("PATH");
	if (paths == NULL)
	  return NULL;
     }

   foreach (strchop (paths, path_get_delimiter(), 0))
     {
	variable dir = ();
	variable dirfile = path_concat (dir, command), st;
	st = stat_file (dirfile);
	if ((st != NULL) && stat_is ("reg", st.st_mode))
	  return dirfile;
     }
   return NULL;
}

% All the test entries in the mailcaps that I have seen test for X.
private variable Is_X_Tests =
  [
   "test -n \"$DISPLAY\"",
   "test \"$DISPLAY\" != \"\"",
   "test \"$DISPLAY\"",
   "sh -c 'test $DISPLAY'",
   "RunningX",
  ];
private variable Is_Not_X_Tests =
  [
   "test -z \"$DISPLAY\"",
   "test \"$DISPLAY\" = \"\"",
  ];

define mailcap_lookup_entry (content_type)
{
   variable compose = qualifier_exists ("compose");
   variable edit = qualifier_exists ("edit");
   variable print = qualifier_exists ("print");

   variable type = strtrim (strchop (content_type, ';', 0)[0]);

   if (Mailcap_Table == NULL)
     read_mailcap_files ();

   type = strlow (type);
   variable basetype = strchop (type, '/', 0)[0];

   variable isX = (NULL != getenv ("DISPLAY"));
   variable mc;
   foreach mc (Mailcap_Table)
     {
	if ((mc._type != type) && (mc._type != basetype))
	  continue;
	if (compose && (mc._compose == NULL))
	  continue;
	if (edit && (mc._edit == NULL))
	  continue;
	if (print && (mc._print == NULL))
	  continue;

	if (NULL == command_exists (mc._command))
	  continue;

	variable test = mc._test;
	if (test == NULL)
	  break;

	variable needsX;
	if (any (test == Is_X_Tests))
	  needsX = 1;
	else if (any (test == Is_Not_X_Tests))
	  needsX = 0;
	else
	  {
	     vmessage ("**** WARNING: mailcap fast test entry %s not implemented", test);
	     if (0 == system (test))
	       break;
	     continue;
	  }
	if (isX == needsX)
	  break;
     }
   then return NULL;

   mc = @mc;
   mc.content_type = content_type;
   return mc;
}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

private variable Tmp_File_List = {};
define mailcap_remove_tmp_files ()
{
   loop (length (Tmp_File_List))
     {
	() = remove (list_pop (Tmp_File_List));
     }
}
#ifexists atexit
atexit (&mailcap_remove_tmp_files);
#endif

private define open_tmp_file (template, filep)
{
   variable file;
   variable flags = O_WRONLY | O_CREAT | O_EXCL | O_BINARY;
   variable mode = (S_IWUSR | S_IRUSR);
   loop (1000)
     {
	(file,) = strreplace (template, "%s", sprintf ("%X", rand()), strbytelen(template));
	variable fd = open (file, flags, mode);
	if (fd != NULL)
	  {
	     @filep = file;
	     return fd;
	  }
     }
   return NULL;
}

private define get_tmp_dir ()
{
   variable st, tmpdir, dir = NULL;
   foreach tmpdir (["TMP", "TMPDIR"])
     {
	tmpdir = getenv (tmpdir);
	if ((tmpdir == NULL)
	    || (st = stat_file (dir), st==NULL)
	    || (0 == stat_is ("dir", st.st_mode)))
	  continue;
	dir = tmpdir;
	break;
     }
   if (dir == NULL)
     {
#ifdef UNIX
	dir = "/tmp";
#else
	dir = ".";
#endif
     }
   return dir;
}


private define view_file_with_pager (file)
{
   variable pager = getenv ("PAGER");
   if (pager == NULL)
     pager = "more";

   () = system ("$pager '$file'"$);
}

private define view_method_internal (mc, str, cmd, tmpdir)
{
   ifnot (is_substrbytes (cmd, "%s"))
     {
	variable fp = popen (cmd, "w");
	if (fp == NULL)
	  throw OSError, "Unable to open a pipe to $cmd"$;
	() = fwrite (str, fp);
	() = fflush (fp);
	try
	  {
	     () = pclose (fp);
	  }
	catch UserBreakError;
	return;
     }

   variable ext = "tmp";

   variable template = mc._nametemplate;
   if (template == NULL)
     template = "%s.$ext"$;

   template = path_concat (tmpdir, template);

   variable file;
   variable fd = open_tmp_file (template, &file);
   if ((fd == NULL) || (fp = fdopen (fd, "w"), fp == NULL))
     throw OpenError, "Unable to make a temporary file";

   if ((bstrlen (str) != fwrite (str, fp))
       || (-1 == fflush (fp)))
     throw WriteError, "Error writing to $file"$;

   () = close (fd);
   (cmd,) = strreplace (cmd, "%s", file, strbytelen(cmd));

   variable status = system (cmd);

   if (mc._needsterminal)
     () = remove (file);
   else list_append (Tmp_File_List, file);

   if (status)
     throw OSError, "$cmd returned a non-0 value"$;
}

private define view_method (mc, message)
{
   variable pagerfile = NULL, fdpager, tmpdir = get_tmp_dir ();

   variable cmd = mc._command;

   if (mc._copiousoutput)
     {
	fdpager = open_tmp_file (path_concat (tmpdir, "out%s.tmp"), &pagerfile);
	if (fdpager == NULL)
	  throw OpenError, "Unable to create a temporary output file";
	cmd = cmd + " > $pagerfile"$;
     }

   try
     {
	view_method_internal (mc, message, cmd, tmpdir);
	if (pagerfile != NULL)
	  view_file_with_pager (pagerfile);
     }
   finally
     {
	if (pagerfile != NULL)
	  () = remove (pagerfile);
     }
}
