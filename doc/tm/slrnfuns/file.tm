This chapter lists some useful functions for file input / output. Please
note that most functions you might expect to find here (like opening and
reading from a regular file) are already part of \slang itself. If you need
one of those, please consult the file slangfun.txt which comes with \slang.

\function{close_log_file}
\usage{Void close_log_file ()}
\description
   The \var{close_log_file} function closes the file previously opened
   by \var{open_log_file}.
\seealso{open_log_file, log_message}
\done

\function{log_message}
\usage{Void log_message (String_Type msg)}
\description
   The \var{log_message} function may be used to write a string to the
   log file.  If no log file has been opened via \var{open_log_file},
   the message will be written to \var{stderr}.
\seealso{open_log_file, close_log_file, message}
\done

\function{make_home_filename}
\usage{String_Type make_home_filename (String_Type name)}
\description
   This function returns the complete filename associated with a file called
   \var{name} located in the user's home directory. If \var{name} is already
   an absolute filename or explicitly relative to the current directory
   (i.e. starts with one or two dots, followed by a directory separator), it
   remains unchanged.
\seealso{read_mini}
\done

\function{open_log_file}
\usage{Void open_log_file (String_Type file)}
\description
   The \var{open_log_file} function causes \slang traceback messages
   to be written to the specified file.  This is useful for debugging
   macros.  Traceback messages are enabled by setting the \slang
   variable \var{_traceback} to a non-zero value.
\seealso{close_log_file, log_message, _traceback, _trace_function}
\done

\function{print_file}
\usage{Void print_file (String_Type file)}
\description
   The \var{print_file} function may be used to send a specified file
   to the printer.
\notes
   The printer is specified via the slrnrc \var{printer_name} variable.
\done

