% This example shows how followup_hook may be used to generate the
% X-Comment-To field associated with fido.* newsgroups.

% This variable will hold the followup_custom_headers as defined in the
% .slrnrc file.
variable Default_Followup_Headers;

Default_Followup_Headers = get_variable_value ("followup_custom_headers");
define followup_hook ()
{
   variable h;
   variable from;

   h = Default_Followup_Headers;
   
   if (0 == strncmp (current_newsgroup, "fido.", 5))
     {
	from = extract_displayed_article_header ("Reply-To");
	!if (strlen (from))
	  from = extract_displayed_article_header ("From");

	% It is a fido newsgroup.  Generate X-Comment-To: header
	h = sprintf ("%s\nX-Comment-To: %s", h, from);
     }

   set_string_variable ("followup_custom_headers", h);
}
