/*
csvfix - reads (possibly malformed) CSV data from input file
         and writes properly formed CSV to output file

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

#define PROGRAM_NAME "csvfix"
#define AUTHORS "Robert Gamble"

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

/* The output delimiter character */
char output_delimiter = CSV_COMMA;

/* The output delimiter argument */
char *output_delimiter_name;

/* The output quote character */
char output_quote = CSV_QUOTE;

/* The output quote argument */
char *output_quote_name;

/* The current_field */
size_t current_field;

static struct option const longopts[] =
{
  {"delimiter", required_argument, NULL, 'd'},
  {"quote", required_argument, NULL, 'q'},
  {"version", no_argument, NULL, CHAR_MAX + 1},
  {"help", no_argument, NULL, CHAR_MAX + 2},
  {"output-delimiter", required_argument, NULL,  CHAR_MAX + 3},
  {"output-quote", required_argument, NULL,  CHAR_MAX + 4},
  {NULL, 0, NULL, 0}
};

void usage (int status);
void cb1 (void *s, size_t i, void *outfile);
void cb2 (int c, void *outfile);

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s [OPTIONS] [FILE1] [FILE2]\n\
Process possibly malformed CSV data from FILE1 or standard input and write\n\
properly formed CSV data to FILE2 or standard output\n\
\n\
  -d, --delimiter=DELIM         use DELIM instead of comma as delimiter\n\
  -q, --quote=QUOTE_CHAR        use QUOTE_CHAR instead of double quote as quote\n\
                                character\n\
", program_name);
    printf("\
      --output-delimiter=DELIM  use DELIM as the output delimiter\n\
      --output-quote=QUOTE_CHAR use QUOTE_CHAR as the output quote character\n\
      --version                 display version information and exit\n\
      --help                    display this help and exit\n\
");
  }
  exit(status);
}

void
cb1 (void *s, size_t i, void *outfile)
{
  if (current_field != 0) fputc(output_delimiter,(FILE *)outfile);
  csv_fwrite2((FILE *)outfile, i ? s : "", i, output_quote);
  current_field++;
}

void
cb2 (int c, void *outfile)
{
  fputc('\n', (FILE *)outfile);
  current_field = 0;
}

int
main (int argc, char *argv[])
{
  char buf[1024];
  size_t i;
  struct csv_parser p;
  FILE *infile, *outfile;
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

      case CHAR_MAX + 3:
        /* --output-delimiter */
        output_delimiter_name = optarg;
        if (strlen(output_delimiter_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          output_delimiter = output_delimiter_name[0];
        break;

      case CHAR_MAX + 4:
        /* --output-quote */
        output_quote_name = optarg;
        if (strlen(output_quote_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          output_quote = output_quote_name[0];
        break;

      case CHAR_MAX + 1:
        /* --version */
        print_version(PROGRAM_NAME);
        break;

      case CHAR_MAX + 2:
        /* --help */
        usage(EXIT_SUCCESS);
        break;

      default:
        usage(EXIT_FAILURE);
    }

  csv_init(&p, 0);
  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  infile = stdin;
  outfile = stdout;

  if (argc > optind) {
    if (argc - optind == 2) {
      if (!strcmp(argv[optind], argv[optind+1]))
        err("Input file and output file must not be the same!");
    } else if (argc - optind > 2) {
      usage(EXIT_FAILURE);
    }
    infile = fopen(argv[optind], "rb");
    if (infile == NULL) {
      fprintf(stderr, "Failed to open file %s: %s\n", argv[optind], strerror(errno));
      exit(EXIT_FAILURE);
    }
    if (argc - optind == 2) {
      outfile = fopen(argv[optind+1], "wb");
      if (outfile == NULL) {
        fprintf(stderr, "Failed to open file %s: %s\n", argv[optind+1], strerror(errno));
        fclose(infile);
        exit(EXIT_FAILURE);
      }
    }
  }

  while ((i=fread(buf, 1, 1024, infile)) > 0) {
    if (csv_parse(&p, buf, i, cb1, cb2, outfile) != i) {
      fprintf(stderr, "Error parsing file: %s\n", csv_strerror(csv_error(&p)));
      fclose(infile);
      fclose(outfile);
      if (argc - optind == 2) remove(argv[optind]);
      exit(EXIT_FAILURE);
    }
  }

  csv_fini(&p, cb1, cb2, outfile);
  csv_free(&p);

  if (ferror(infile)) {
    fprintf(stderr, "Error reading from input file");
    fclose(infile);
    fclose(outfile);
    if (argc - optind == 2) remove(argv[argc - optind]);
    exit(EXIT_FAILURE);
  }

  fclose(infile);
  fclose(outfile);
  return EXIT_SUCCESS;
}

