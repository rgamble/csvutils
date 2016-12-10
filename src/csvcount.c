/*
csvcount - reads CSV data from input file(s) and reports the number
           of fields and rows encountered in each file

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
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include "libcsv/csv.h"
#include "version.h"
#include "helper.h"

#define PROGRAM_NAME "csvcount"
#define AUTHORS "Robert Gamble"

/* Print totals if set */
int show_totals;

/* Print only number of fields? */
int print_fields;

/* Print only the number of rows? */
int print_rows;

/* The number of fields encountered for the current file */
long unsigned fields;

/* The number of rows encountered for the current file */
long unsigned rows;

/* The number of fields encountered for all files */
long unsigned total_fields;

/* The number of rows encountered for all files */
long unsigned total_rows;

/* The quote character */
char quote = CSV_QUOTE;

/* The delimiter character */
char delimiter = CSV_COMMA;

/* The quote argument */
char *quote_string;

/* The delimiter argument */
char *delimiter_string;

/* The name this program was called with */
char *program_name;


static struct option const longopts[] =
{
  {"fields", no_argument, NULL, 'f'},
  {"rows", no_argument, NULL, 'r'},
  {"delimiter", required_argument, NULL, 'd'},
  {"quote", required_argument, NULL, 'q'},
  {"help", no_argument, NULL, CHAR_MAX + 1},
  {"version", no_argument, NULL, CHAR_MAX + 2},
  {NULL, 0, NULL, 0}
};


void cb1 (void *s, size_t len, void *data);
void cb2 (int c, void *data);
void usage (int status);
void count_file(char *filename);

void
cb1 (void *s, size_t len, void *data)
{
  fields++;
  total_fields++;
}

void
cb2 (int c, void *data)
{
  rows++;
  total_rows++;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s [OPTION]... FILE...\n\
Print the number of fields and rows in a CSV file.\n\
  -f, --fields           print only the number of fields\n\
  -r, --rows             print only the number of rows\n\
  -d, --delimiter=DELIM  use DELIMITER as the field delimiter instead of comma\n\
  -q, --quote=QUOTE      use QUOTE as the quote character instead of double quote\n\
      --version          display version information and exit\n\
      --help             display this help and exit\n\
", program_name);
  }
  exit(status);
}


void
count_file(char *filename)
{
  FILE *fp;
  struct csv_parser p;
  char buf[1024];
  size_t bytes_read;

  fields = rows = 0;

  if (csv_init(&p, 0) != 0)
    err("Failed to initialize csv parser");
  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  if (filename == NULL || !strcmp(filename, "-")) {
    fp = stdin;
  } else {
    fp = fopen(filename, "rb");
    if (fp == NULL) {
      fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
      csv_free(&p);
      exit(EXIT_FAILURE);
    }
  }

  while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
    if (csv_parse(&p, buf, bytes_read, cb1, cb2, NULL) != bytes_read) {
      fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
      csv_free(&p);
      return;
    }
  }

  csv_fini(&p, cb1, cb2, NULL);
  csv_free(&p);

  if (ferror(fp)) {
    fprintf(stderr, "Error reading file %s\n", filename);
    fclose(fp);
    return;
  }

  fclose(fp);

  if (print_rows)
    printf("%8lu ", rows);

  if (print_fields)
    printf("%8lu ", fields);

  printf("%s\n", filename ? filename : "");
}

int
main (int argc, char *argv[])
{
  int optc;

  while ((optc = getopt_long(argc, argv, "d:fq:r", longopts, NULL)) != -1)
    switch (optc) {
      case 'd':
        delimiter_string = optarg;
        if (strlen(delimiter_string) > 1)
          err("delimiter must be exactly one byte long");
        else
          delimiter = delimiter_string[0];
        break;

      case 'f':
        print_fields = 1;
        break;

      case 'q':
        quote_string = optarg;
        if (strlen(quote_string) > 1)
          err("delimiter must be exactly one byte long");
        else
          quote = quote_string[0];
        break;

      case 'r':
        print_rows = 1;
        break;

      case CHAR_MAX + 1:
        usage(EXIT_SUCCESS);
        break;
 
      case CHAR_MAX + 2:
        print_version(PROGRAM_NAME);
        break;

      default:
        usage(EXIT_FAILURE);
    }

  if (!print_rows && !print_fields)
    print_rows = print_fields = 1;

  if (optind < argc) {
    if (argc - optind > 1)
      show_totals = 1;
    while (optind < argc)
      count_file(argv[optind++]);
  } else {
    /* Process from stdin */
    count_file(NULL);
  }

  if (show_totals) {
    if (print_rows) printf("%8lu ", total_rows);
    if (print_fields) printf("%8lu ", total_fields);
    puts("total");
  }

  exit(EXIT_SUCCESS);
}
 
