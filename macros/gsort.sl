% This macro sorts your group list ASCIIbetically.
% 
% It is kept simple to serve as a demonstration of the get_group_order /
% set_group_order intrinsic functions.  For more features, use a macro like
% group_sort.sl by J.B. Nicholson-Owens <http://forestfield.org/slrn/>
% 
% Note: You need at least S-Lang version 1.4.0 for this to work.

public define sort_groups ()
{
   variable group_order;
   
   group_order = get_group_order ();
   group_order = group_order[array_sort (group_order)];
   set_group_order (group_order);
}
