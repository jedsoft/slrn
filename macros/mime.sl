% The routines in this file parse a multipart MIME message.
% 
% The current functionality is more or less the same as that achieved
% by Thomas Wiegner's minimal_multipart patch.
%
% There is only one public function in this file:
%
%     mime_decode_multipart()
%
% If the current article has Content-Type equal to "multipart", the
% displayed article will be replaced by the text/plain portion of the
% article.
%
% To use this function, add the following lines to your .slrnrc file:
%
%    interpret "multimime.sl"
%    setkey article "mime_decode_multipart" KEYBINDING
%
% Then when a multipart article shows up, press the keysequence
% specified by the setkey statement. You can also add this function to
% a "read_article_hook" to have it automatically invoked.
%
% TODO: Add support for saving attachments, images, etc.  This should
% be straightforward.
%
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
   variable val = string_matches (key, word + `\C *= *"\([^"]+\)"`);
   if (val == NULL)
     val = string_matches (key, word + `\C *= *\([^; ]+\)`);
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
   type, disposition,
   header,
   list,			       %  non-null if multipart
   message, charset	       %  non-multipart decoded message
};

private define decode_base64 (str)
{
   return str;
}

private define decode_qp (str)
{
   return str;
}

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

private define parse_mime (art)
{
   variable header, body;
   (header, body) = split_article (art);

   variable node = @Mime_Node_Type;
   node.type = get_header_key (header, "Content-Type", 1);
   node.disposition = get_header_key (header, "Content-Disposition", 0);
   node.header = header;

   if (is_substr (node.type, "multipart/"))
     {
	parse_multipart (node, body);
	return node;
     }

   node.message = body;

   variable encoding = get_header_key (header, "Content-Transfer-Encoding", 1);
   if (is_substr (encoding, "base64"))
     node.message = decode_base64 (node.message);
   else if (is_substr (encoding, "quoted-printable"))
     node.message = decode_qp (node.message);

   node.charset = parse_subkeyword (node.type, "charset");

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
	     if (is_substrbytes (leaf.type, type))
	       return leaf;
	  }
     }
   return NULL;
}

define mime_decode_multipart ()
{
   variable h = extract_article_header ("Content-Type");
   if ((h == NULL) || (0 == is_substrbytes (h, "multipart/")))
     return;
   variable art = raw_article_as_string ();
   variable nodes = parse_mime (art);

   if (nodes.list == NULL)
     return;

   % Look for the first text/plane part that occurs in the original mime
   % encoded body.
   variable header = nodes.header;

   variable leaf, leaves = {};
   flatten_node_tree (nodes, leaves);
   leaf = find_first_matching_leaf (leaves, {"text/plain", "text/"});
   if (leaf == NULL)
     return;		       %  nothing to display

   % Replace some of the headers in the raw article by subpart headers
   variable value;
   foreach value (leaf.header) using ("values")
     set_header_key (header, value.name, value.value);

   art = "";
   foreach value (header) using ("values")
     art = sprintf ("%s%s: %s\n", art, value.name, value.value);

   art = art + "\n" + leaf.message;

   replace_article (art, 1);
}

