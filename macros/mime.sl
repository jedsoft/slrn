% The routines in this file parse a multipart MIME message.
% Copyright (C) 2012 John E. Davis <jed@jedsoft.org>
%
% This may be distributed under the terms of the GNU General Public
% License.  See the file COPYING for more information.
%
% Public functions in this file:
%
%     mime_process_multipart()
%     mime_browse()
%     mime_set_save_charset (name)
%
% If the current article has Content-Type equal to "multipart", the
% mime_process_multipart function will replace displayed article by the
% text/plain portion of the article.
%
% The mime_browse function may be used to save or view other parts of
% the mime article.
%
% To use these functions, add the following lines to your .slrnrc file:
%
%    interpret "mime.sl"
%    setkey article "mime_process_multipart" KEYBINDING
%    setkey article "mime_browse" KEYBINDING
%
% Then when a multipart article shows up, press the mime_browse
% keysequence specified by the setkey statement. You can also a call
% to the mime_process_multipart function in a "read_article_hook" to
% have it automatically invoked.
%
% When saving a mime part to a file, it will be converted the slrn
% display charset.  The function `mime_set_save_charset` may be used
% to specify a different one.
%
% The view specified by a mailcap file is used for viewing.

autoload ("mailcap_lookup_entry", "mailcap");

private variable Mime_Save_Dir = make_home_filename ("");
private variable Mime_Save_Charset = get_charset ("display");

define mime_set_save_charset (charset)
{
   Mime_Save_Charset = charset;
}

private define set_header_key (hash, name, value)
{
   hash[strlow(name)] = struct
     {
	name = name,
	value = strtrim (value),
     };
}

private define get_header_key (hash, name, lowercase)
{
   try
     {
	variable h = hash[strlow (name)];
	return h.value;
     }
   catch AnyError;
   return "";
}

private define merge_headers (a, b)
{
   variable c = Assoc_Type[Struct_Type];
   variable value;

   foreach value (a) using ("values")
     set_header_key (c, value.name, value.value);
   foreach value (b) using ("values")
     set_header_key (c, value.name, value.value);
   return c;
}

private define split_article (art)
{
   variable ofs = is_substrbytes (art, "\n\n");
   if (ofs == 0)
     throw DataError, "Unable to find the header separator";

   variable header = substrbytes (art, 1, ofs-1);
   (header,) = strreplace (header, "\n ", " ", strbytelen(header));
   (header,) = strreplace (header, "\n\t", " ", strbytelen(header));
   header = strchop (header, '\n', 0);

   variable hash = Assoc_Type[Struct_Type];
   _for (0, length (header)-1, 1)
     {
	variable i = ();
	variable fields = strchop (header[i], ':', 0);
	set_header_key (hash, fields[0], strjoin (fields[[1:]], ":"));
     }
   variable body = substrbytes (art, ofs+2, -1);
   return hash, body;
}


private define parse_subkeyword (key, word)
{
   variable val = string_matches (key, `\C` + word + ` *= *"\([^"]+\)"`);
   if (val == NULL)
     val = string_matches (key, `\C` + word + ` *= *\([^; ]+\)`);
   if (val == NULL)
     return val;

   return val[1];
}

private define get_multipart_boundary (header)
{
   variable ct = get_header_key (header, "Content-Type", 0);
   if (ct == "")
     return NULL;

   ifnot (is_substr (strlow (ct), "multipart/"))
     return NULL;

   variable boundary = parse_subkeyword (ct, "boundary");
   if (boundary == NULL)
     return NULL;

   return boundary;
}

% The idea here is to represent an article as a list of mime objects
% in the form of a tree.  For a non-multipart article, there is only
% one node.  For a multipart message, there will be a linked list of
% nodes, one for each subpart.  If the subpart is a multipart, a new
% subtree will begin.  For example, here is an article with a
% two-multiparts, with the second contained in the first.
%
%                  article
%                  /    \
%                       /\
%
private variable Mime_Node_Type = struct
{
   mimetype,			       %  lowercase type/subtype, from content-type
   disposition,			       %  content-disposition header
   content_type,		       %  full content-type header
   header,			       %  assoc array of header keywords
   list,			       %  non-null list of nodes if multipart
   message, charset, encoding,	       %  non-multipart decoded message
   converted = 0,
};

private define parse_mime ();
private define parse_multipart (node, body)
{
   variable boundary = get_multipart_boundary (node.header);
   if (boundary == NULL)
     return;

   boundary = "--" + boundary;
   variable
     blen = strbytelen (boundary),
     boundary_end = boundary + "--", blen_end = blen + 2;

   node.list = {};

   body = strchop (body, '\n', 0);
   variable i = 0, imax = length(body);
   while (i < imax)
     {
	if (strnbytecmp (body[i], boundary, blen))
	  {
	     i++;
	     continue;
	  }

	if (0 == strnbytecmp (body[i], boundary_end, blen_end))
	  break;

	i++;
	variable i0 = i;
	if (i0 == imax)
	  break;

	while (i < imax)
	  {
	     if (strnbytecmp (body[i], boundary, blen))
	       {
		  i++;
		  continue;
	       }
	     break;
	  }
	variable new_node = parse_mime (strjoin (body[[i0:i-1]], "\n"));
	if (new_node != NULL)
	  list_append (node.list, new_node);
     }
}

private define extract_mimetype (content_type)
{
   return strlow (strtrim (strchop (content_type, ';', 0)[0]));
}

private define parse_mime (art)
{
   variable header, body;
   (header, body) = split_article (art);

   variable node = @Mime_Node_Type;
   node.content_type = get_header_key (header, "Content-Type", 1);
   node.disposition = get_header_key (header, "Content-Disposition", 0);
   node.header = header;
   node.mimetype = extract_mimetype (node.content_type);

   if (is_substr (node.mimetype, "multipart/"))
     {
	parse_multipart (node, body);
	return node;
     }

   node.message = body;

   variable encoding = get_header_key (header, "Content-Transfer-Encoding", 1);
   if (is_substr (encoding, "base64"))
     node.encoding = "base64";
   else if (is_substr (encoding, "quoted-printable"))
     node.encoding = "quoted-printable";

   node.charset = parse_subkeyword (node.content_type, "charset");

   return node;
}

private define flatten_node_tree (node, leaves);   %  recursive
private define flatten_node_tree (node, leaves)
{
   if (node.list == NULL)
     {
	list_append (leaves, node);
	return;
     }

   foreach node (node.list)
     flatten_node_tree (node, leaves);
}

% Search for the first node whose type matches one in the types list.
private define find_first_matching_leaf (leaves, types)
{
   variable type, leaf;
   foreach type (types)
     {
	foreach leaf (leaves)
	  {
	     if (is_substrbytes (leaf.mimetype, type))
	       return leaf;
	  }
     }
   return NULL;
}

% The (cached) msgid/mime objects/article header for current article
private variable Mime_Object_List = NULL;
private variable Mime_MessageID = NULL;
private variable Mime_Article_Headers = NULL;

% Returns NULL if the message is not Mime Encoded, otherwise it
% returns the value of the Content-Type header.
private define is_mime_message ()
{
   variable h = extract_article_header ("Mime-Version");
   if ((h == NULL) || (h == ""))
     return NULL;

   h = extract_article_header ("Content-Type");
   if (h == "") h = NULL;
   return h;
}

% This function returns the top-level headers in the message
private define process_mime_message ()
{
   variable msgid = extract_article_header ("Message-ID");
   if (msgid == Mime_MessageID)
     return;

   Mime_MessageID = NULL;
   Mime_Object_List = NULL;

   variable art = raw_article_as_string ();
   variable nodes = parse_mime (art);

   variable leaf, leaves = {};
   flatten_node_tree (nodes, leaves);

   Mime_MessageID = msgid;
   Mime_Object_List = leaves;
   Mime_Article_Headers = nodes.header;
}

private define convert_mime_object (); %  forward decl
private define replace_article_with_mime_obj (obj)
{
   % Replace some of the headers in the raw article by subpart headers
   variable header = merge_headers (Mime_Article_Headers, obj.header);

   obj = @obj;
   obj.header = header;

   variable value, art = "";
   foreach value (header) using ("values")
     art = sprintf ("%s%s: %s\n", art, value.name, value.value);

   art = art + "\n" + convert_mime_object (obj);

   replace_cooked_article (art, 0);
}

private define is_attachment (node)
{
   return is_substrbytes (strlow (node.disposition), "attachment");
}

private define is_text (node)
{
   return is_substrbytes (node.mimetype, "text/");
}

private define get_mime_filename (node)
{
   variable file = parse_subkeyword (node.disposition, "filename");
   if (file != NULL)
     return file;
   file = parse_subkeyword (node.content_type, "name");
   if (file != NULL)
     return file;

   return "";
}

private define format_type (type, width)
{
   if (0 == strnbytecmp (type, "application", 11))
     type = "app" + substrbytes (type, 12, -1);

   if (strbytelen (type) <= width)
     return type;
   type = type[[0:width-4]];
   type += "...";
   return type;
}

private define convert_mime_object (obj)
{
   variable str = obj.message;

   if (obj.converted)
     return str;

   if (obj.encoding == "base64")
     str = decode_base64_string (str);
   else if (obj.encoding == "quoted-printable")
     str = decode_qp_string (str);

   variable charset = obj.charset;
   if ((charset != NULL) && (charset != "")
       && (Mime_Save_Charset != NULL)
       && (strlow(charset) != strlow(Mime_Save_Charset)))
     {
	str = charset_convert_string (str, charset, Mime_Save_Charset, 0);
     }
   obj.converted = 1;
   return str;
}

private define save_mime_object (obj, fp)
{
   if (typeof (fp) == String_Type)
     {
	variable file = fp;
	fp = fopen (file, "w");
	if (fp == NULL)
	  throw OpenError, "Could not open $file for writing"$;
     }

   variable str = convert_mime_object (obj);

   () = fwrite (str, fp);
   () = fflush (fp);
}

private define make_safe_filename (file)
{
   return strtrans (file, "-+_/.%@{}:A-Za-z0-9", "_");
}

private define mime_quit_hook ()
{
   % Only call the mailcap_remove_tmp_files if it was loaded
   variable r = __get_reference ("mailcap_remove_tmp_files");
   if (r != NULL) (@r)();
}
() = register_hook ("quit_hook", &mime_quit_hook);

private define view_mime_object (obj)
{
   variable type = obj.mimetype;
   variable mc = mailcap_lookup_entry (obj.content_type);
   if (mc == NULL)
     throw NotImplementedError, "No viewer for $type available"$;

   variable str = convert_mime_object (obj);

   variable e;
   try (e)
     {
	set_display_state (0);
	mc.view (str);
     }
   catch OSError:
     {
	() = fprintf (stdout, "\n*** ERROR; %S\n\nPress enter to continue.",
		      e.message);
	variable line;
	() = fgets (&line, stdin);
	throw;
     }
   finally
     {
	set_display_state (1);
     }
}

define mime_browse ()
{
   if (NULL == is_mime_message ())
     return;

   process_mime_message ();

   variable node, descriptions = {};
   variable filenames = {};
   list_append (descriptions, "View full message with all parts");
   foreach node (Mime_Object_List)
     {
	variable filename = get_mime_filename (node);
	filename = path_basename (rfc1522_decode_string (filename));
	list_append (filenames, filename);
	variable attachment = "";
	variable charset = node.charset; if (charset == NULL) charset = "";
	if (is_attachment (node)) attachment = "[attachment]";
	list_append (descriptions,
		     sprintf ("%-16s |%12s| %s%S",
			      format_type (node.mimetype, 16),
			      charset,
			      attachment,
			      filename,
			     ));
     }

   forever
     {
	variable n = get_select_box_response ("Browse Mime",
					      __push_list (descriptions),
					      length (descriptions));

	if (n == 0)
	  {
	     % View full message option
	     replace_article (raw_article_as_string (), 0);
	     return;
	  }

	n--;

	node = Mime_Object_List[n];
	filename = filenames[n];

	n = get_response ("SsVvCc\007", "Action: \001Save to file, \001View, \001Cancel");

	if ((n == 7) || (n == 'c') || (n == 'C'))
	  return;

	if ((n == 'S') || (n == 's'))
	  {
	     filename = path_concat (Mime_Save_Dir, filename);
	     forever
	       {
		  filename = read_mini_filename ("Save to:", "", filename);
		  if (NULL != stat_file (filename))
		    {
		       n = get_yes_no_cancel ("File exists, Overwrite?", 0);
		       if (n == 0)
			 continue;
		       if (n == -1)
			 return;
		    }
		  save_mime_object (node, filename);
		  Mime_Save_Dir = path_dirname (filename);
		  break;
	       }

	     message_now ("Saved to $filename"$);
	     continue;
	  }

	if ((n == 'V') || (n == 'v'))
	  {
	     if (is_substrbytes (node.mimetype, "text/plain"))
	       {
		  replace_article_with_mime_obj (node);
		  return;
	       }

	     view_mime_object (node);
	  }
	break;
     }
}

define mime_process_multipart ()
{
   variable h = is_mime_message ();

   if (h == NULL)
     return;

   variable mimetype = extract_mimetype (h);

   if (0 == is_substrbytes (mimetype, "multipart/"))
     {
	if (mimetype != "text/plain")
	  vmessage ("This is a MIME message with Content-Type %s", h);

	return;
     }
   process_mime_message ();

   variable node, leaf;
   % Look for the first text/plain part that occurs in the original mime
   % encoded body.
   leaf = find_first_matching_leaf (Mime_Object_List, {"text/plain", "text/"});
   if (leaf == NULL)
     return;		       %  nothing to display

   variable num_attchments = 0, num_non_text = 0,
     num_text = 0, num_html = 0, num_plain = 0;
   foreach node (Mime_Object_List)
     {
	if (is_attachment (node))
	  num_attchments++;
	else if (is_text (node))
	  {
	     num_text++;
	     num_html += (0 != is_substrbytes (node.mimetype, "/html"));
	     num_plain += (0 != is_substrbytes (node.mimetype, "/plain"));
	  }
	else
	  num_non_text++;
     }
   replace_article_with_mime_obj (leaf);
   update ();

   variable msg_parts = String_Type[0];
   if (num_attchments)
     msg_parts = [msg_parts, sprintf ("%d attachments", num_attchments)];
   if (num_non_text)
     msg_parts = [msg_parts, sprintf ("%d non-text", num_non_text)];
   if (num_text)
     msg_parts = [msg_parts, sprintf ("%d text/plain (%d html, %d other)",
				      num_plain, num_html,
				      num_text - (num_plain+num_html))];

   vmessage ("MIME %s [%s]", mimetype, strjoin (msg_parts, ", "));
}
() = register_hook ("read_article_hook", &mime_process_multipart);
