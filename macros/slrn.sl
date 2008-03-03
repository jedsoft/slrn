% This is the main slrn initialization file called from interp.c.
% It is not meant for end user configuration; edit slrn.rc instead.

define prepend_to_slang_load_path (dir)
{
   if (dir == NULL)
     return;

   variable path = get_slang_load_path ();
   variable delim = char (path_get_delimiter ());
   set_slang_load_path (strcat (dir, delim, path));
}

define append_to_slang_load_path (dir)
{
   if (dir == NULL)
     return;

   variable path = get_slang_load_path ();
   variable delim = char (path_get_delimiter ());
   set_slang_load_path (strcat (path, delim, dir));
}

% This function gets called from the "set macro_directory" line in the
% slrn.rc file.  The paths are either relative to the home directory, or
% are absolute
define slrn_set_macro_dir_hook (dirs)
{
   foreach (strchopr (dirs, ',', 0))
     {
	variable dir = ();
	if (0 == path_is_absolute (dir))
	  dir = make_home_filename (dir);

	prepend_to_slang_load_path (dir);
     }
}

define slrn_get_macro_dir_hook ()
{
   variable path = get_slang_load_path ();
   variable delim = char (path_get_delimiter ());
   return strtrans (path, delim, ",");
}

define search_path_for_file (path, file, delim)
{
   if (path_is_absolute (file) and (stat_file (file) != NULL))
     return file;

   if (path == NULL)
     return NULL;

   foreach (strtok (path, char(delim)))
     {
        variable dir = ();
        variable dirfile = path_concat (dir, file);

	if (stat_file (dirfile) != NULL)
	  return dirfile;
     }

   return NULL;
}

define find_executable (exe)
{
   return search_path_for_file (getenv ("PATH"), exe, path_get_delimiter ());
}

% A conforming slang application is one that has access to slsh's files
private define add_slsh_paths ()
{
   % Assume a standard install with:
   %   /prefix/bin/slsh
   %   /prefix/share/slsh/local-packages/
   variable prefixes = {};

   variable exe = find_executable ("slsh");
   if (exe != NULL)
     list_append (prefixes, path_dirname (path_dirname (exe)));

   list_append (prefixes, _slang_install_prefix);
   
   foreach (prefixes)
     {
	variable prefix = ();

	if (prefix == NULL)
	  continue;

	variable dir = path_concat (prefix, "share/slsh/local-packages");
	if (stat_file (dir) == NULL)
	  continue;
	
	append_to_slang_load_path (dir);
	append_to_slang_load_path (path_dirname (dir));
	return;
     }
}

add_slsh_paths ();
private define add_slsh_paths ();      %  delete it -- no longer needed

autoload ("require", "require");
autoload ("provide", "require");
autoload ("reverse", "arrayfuns");
autoload ("shift", "arrayfuns");

% Prepend the $HOME directory as a path for backward-compatibility
prepend_to_slang_load_path (path_dirname (make_home_filename (".")));
