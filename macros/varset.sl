% This macro will prompt for a variable name and lets you change it, using
% the current value as the default.

define set_variable ()
{
   variable name, value;
   
   name = read_mini_variable ("Set variable: ", "", "");
   if (name == "")
     return;
   
   value = get_variable_value (name);
   if (typeof (value) == Integer_Type)
     {
	value = read_mini_integer ("New value: ", value);
	set_integer_variable (name, value);
     }
   else
     {
	value = read_mini ("New value: ", "", value);
	set_string_variable (name, value);
     }
}
