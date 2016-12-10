#include <stdio.h>
#include <stdlib.h>
#include <libcsv/csv.h>

#define VERSION "0.9.3"

#if !defined(CSV_MAJOR) || !defined(CSV_MINOR) || !defined(CSV_RELEASE) || CSV_MAJOR < 3
#  error "This version of csvutils requires libcsv 3.0.0 or higher"
#endif

void
print_version(char *program_name)
{
  fprintf(stderr, "\
%s (csvutils) %s\n\
Compiled with libcsv version %d.%d.%d\n\
Copyright (C) 2008 Robert Gamble\n\
This is free software.  You may redistribute copies of it under the terms of\n\
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n\
Written by Robert Gamble.\n\
", program_name, VERSION, CSV_MAJOR, CSV_MINOR, CSV_RELEASE);
  exit(EXIT_SUCCESS);
}

