#% -*- mode: textmac; mode: fold -*-

#%{{{ Macro Definitions 

#i linuxdoc.tm

#d slang \bf{S-lang}
#d kw#1 \tt{$1}
#d exmp#1 \tt{$1}
#d var#1 \tt{$1}
#d ldots ...
#d chapter#1 <chapt>$1<p>
#d preface <preface>
#d tag#1 <tag>$1</tag>
#d newline <newline>

#d function#1 \subsection{$1\label{$1}}<descrip>
#d variable#1 \subsection{$1\label{$1}}<descrip>
#cd function#1 <p><bf>$1</bf>\label{$1}<p><descrip>
#d synopsis#1 <tag> Synopsis </tag> $1
#d keywords#1 <tag> Keywords </tag> $1
#d usage#1 <tag> Usage </tag> <tt>$1</tt>
#d description <tag> Description </tag>
#d example <tag> Example </tag>
#d notes <tag> Notes </tag>
#d seealso#1 <tag> See Also </tag> <tt>$1</tt>
#d r#1 \ref{$1}{$1}
#d done </descrip><p>

#d documentstyle article
#d sect1 \section
#d sect2 \subsection
#d sect3 \subsubsection

#d slang-manual \bf{A Guide to the S-Lang Language}

#%}}}

\linuxdoc
\begin{\documentstyle}

\title Slrn Intrinsic Function Reference Manual
\author John E. Davis, \tt{davis@space.mit.edu}\newline
Thomas Schultz, \tt{tststs@gmx.de}
\date \__today__

\toc

\sect1{Header and Thread Functions}
#i slrnfuns/header.tm

\sect1{Article Functions}
#i slrnfuns/article.tm

\sect1{Group Functions}
#i slrnfuns/group.tm

\sect1{Dialog and Message Functions}
#i slrnfuns/dialog.tm

\sect1{Key Input Functions}
#i slrnfuns/keys.tm

\sect1{File I/O Functions}
#i slrnfuns/file.tm

\sect1{Miscellaneous Functions}
#i slrnfuns/misc.tm

\sect1{Hooks}
#i slrnfuns/hooks.tm

\end{\documentstyle}
