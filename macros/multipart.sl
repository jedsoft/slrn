% This macro makes use of subject_compare_hook to put multipart binary
% postings into the same thread.
% Contributed by Jurriaan Kalkman <thunder7@xs4all.nl>

% try to determine if two headers belong to the same multipart
% posting. Return 0 if they do, 1 if they don't
%
% we assume headers use a number in [] or in () to tell us what
% part of a multipart-posting this is.
%
% there are three possible header-types we like:
%
% 1) blablabla <part-number>
% 2) blablabla <file-number> blabla <part-number>
% 3) blablabla <part-number> blabla <file-number>
%
define subject_compare_hook ( subject1, subject2)
{
   variable pattern;
   variable pos1, pos2, orgpos1, orgpos2, len1, len2, tmp;

% Do both headers match the template for a multi-part header?
   pattern = "[(\\[][0-9]+/[0-9]+[)\\]]";

   pos1 = string_match(subject1, pattern, 1);
   if (pos1 == 0) return 1;
% junk the position, but keep the length!
   (tmp, len1) = string_match_nth(0);
   orgpos1 = pos1;

   pos2 = string_match(subject2, pattern, 1);
   if (pos2 == 0) return 1;
% junk the position, but keep the length!
   (tmp, len2) = string_match_nth(0);
   orgpos2 = pos2;

% make sure we have the last possible match for hdr1
% 5 is the minimum length from the regexp pattern
   tmp = string_match(subject1, pattern, pos1 + 5);

   while (tmp > 0)
   {
      pos1 = pos1 + 4 + tmp;
      tmp = string_match(subject1, pattern, pos1 + 5);
   }

% make sure we have the last possible match for hdr2
   tmp = string_match(subject2, pattern, pos2 + 5);
   while (tmp > 0)
   {
      pos2 = pos2 + 4 + tmp;
      tmp = string_match(subject2, pattern, pos2 + 5);
   }
        
% if the part before the matching pattern is the same, they
% belong to the same thread
   if ( 0 == strcmp(substr(subject1, 1, pos1), substr(subject2, 1, pos2)))
   {
      return 0;
   }

% we have handled header-type 1) and 2) until now.
% now if the first parts match
   if ( 0 == strcmp(substr(subject1, 1, orgpos1), substr(subject2, 1, orgpos2)))
   {
% and the parts after them match
      if ( 0 == strcmp(substr(subject1, orgpos1 + len1, -1),
                       substr(subject2, orgpos2 + len2, -1)))
      {
         return 0;
      }
   }
   
   return 1;
}
