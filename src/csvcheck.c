/*
csvcheck - determine if files are properly formed CSV files and display
           position of first offending byte if not

Copyright (C) 2007  Robert Gamble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <csv.h>
#include "version.h"
#include "helper.h"

#define PROGRAM_NAME "csvcheck"
#define AUTHORS "Robert Gamble"

static struct option const longopts[] =
{
  {"delimiter", required_argument, NULL, 'd'},
  {"quote", required_argument, NULL, 'q'},
  {"version", no_argument, NULL, CHAR_MAX + 1},
  {"help", no_argument, NULL, CHAR_MAX + 2},
  {NULL, 0, NULL, 0}
};


/* The name this program was called with */
char *program_name;

/* The delimiter character */
char delimiter = CSV_COMMA;

/* The delimiter argument */
char *delimiter_name;

/* The quote character */
char quote = CSV_QUOTE;

/* The quote argument */
char *quote_name;


void usage (int status);
void check_file(char *filename);


void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s [OPTION]... [FILE]...\n\
Determine if file(s) are properly formed CSV files and display the position\n\
of the first offending byte if not.\n\
\n\
", program_name);
    printf("\
  -d, --delimiter=DELIM   use DELIM as the delimiter instead of comma\n\
  -q, --quote=QUOTE_CHAR  use QUOTE_CHAR as the quote character instead of\n\
                          double quote\n\
      --help              display this help and exit\n\
      --version           display version information and exit\n\
");
  exit(status);
  }
}

void
check_file(char *filename)
{
  size_t pos = 0;
  char buf[1024];
  struct csv_parser p;
  FILE *fp;
  size_t bytes_read;
  size_t retval;

  if (csv_init(&p, CSV_STRICT|CSV_STRICT_FINI) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(EXIT_FAILURE);
  }
  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  if (filename == NULL || !strcmp(filename, "-")) {
    fp = stdin;
  } else {
    fp = fopen(filename, "rb");
    if (fp == NULL) {
      fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
      csv_free(&p);
      return;
    }
  }

  while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
    if ((retval = csv_parse(&p, buf, bytes_read, NULL, NULL, NULL)) != bytes_read) {
      if (csv_error(&p) == CSV_EPARSE) {
        printf("%s: malformed at byte %lu\n", filename ? filename : "stdin", (unsigned long)pos + retval + 1);
        goto end;
      } else {
        printf("Error while processing %s: %s\n", filename ? filename : "stdin", csv_strerror(csv_error(&p)));
        goto end;
      }
    }
    pos += 1024;
  }

  if (csv_fini(&p, NULL, NULL, NULL) != 0)
    printf("%s: missing closing quote at end of input\n", filename ? filename : "stdin");
  else
    printf("%s well-formed\n", filename ? filename : "data is");

  end:
  fclose(fp);
}

int
main (int argc, char *argv[])
{
  int optc;

  program_name = argv[0];

  while ((optc = getopt_long(argc, argv, "d:q:", longopts, NULL)) != -1)
    switch (optc) {
      case 'd':
        delimiter_name = optarg;
        if (strlen(delimiter_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          delimiter = delimiter_name[0];
        break;

      case 'q':
        quote_name = optarg;
        if (strlen(quote_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          quote = quote_name[0];
        break;

      case CHAR_MAX + 1:
        /* --version */
        print_version(program_name);
        break;

      case CHAR_MAX + 2:
        /* --help */
        usage(EXIT_SUCCESS);
        break;

      default:
        usage(EXIT_FAILURE);
    }  

  if (optind < argc) {
    while (optind < argc)
      check_file(argv[optind++]);
  } else {
    /* Process from stdin */
    check_file(NULL);
  }

  return EXIT_SUCCESS;
}
