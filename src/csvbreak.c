/*
csvbreak - Break a CSV file into multiple files based on the value
           of a specific field

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
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include <ctype.h>
#include <csv.h>
#include "version.h"
#include "helper.h"

#define PROGRAM_NAME "csvbreak"
#define AUTHORS "Robert Gamble"

typedef struct entry {
  size_t size;       /* The size of the current entry */
  size_t alloc_size; /* The size allocated for data */
  char *data;        /* The field data */
} entry;

typedef struct file {
  char *name;
  char *filename;
  FILE *fp;
  long unsigned count;
} file;

static struct option const longopts[] = 
{
  {"print-counts", no_argument, NULL, 'c'},
  {"delimiter", required_argument, NULL, 'd'},
  {"field", required_argument, NULL, 'f'},
  {"header", required_argument, NULL, 'h'},
  {"quote", required_argument, NULL, 'q'},
  {"remove-break-field", required_argument, NULL, 'r'},
  {"strict", no_argument, NULL, 's'},
  {"suffix", required_argument, NULL, 'S'},
  {"prefix", required_argument, NULL, 'P'},
  {"version", no_argument, NULL, CHAR_MAX + 1},
  {"help", no_argument, NULL, CHAR_MAX + 2},
  {NULL, 0, NULL, 0}
};


/* If set, just print counts */
int just_print_counts;

/* The array of files that have been opened */
file *file_array;

/* The current entry array */
entry *entry_array;

/* The current size of the file array */
size_t file_array_size;

/* The current size of the entry array */
size_t entry_array_size;

/* The prefix to use for created files */
char *filename_prefix = "";

/* The suffic to use for created files */
char *filename_suffix = ".csv";

/* The name this program was called with */
char *program_name;

/* Do we need to resolve a field name? */
int need_name_resolution;

/* The current input file*/
FILE *infile;

/* The numeric index of the field to break on */
unsigned long break_field;

/* The break field argument */
char *break_field_name;

/* The delimiter character */
char delimiter = CSV_COMMA;

/* The delimiter argument */
char *delimiter_name;

/* The quote character */
char quote = CSV_QUOTE;

/* The quote argument */
char *quote_name;

/* Enforce strict CSV? */
int strict;

/* The current field number */
long unsigned current_field;

/* The current record number */
long unsigned current_record = 1; 

/* True while the current record is the first non-empty record */
int first_record = 1;

/* The name of the file to print the current record to */
char *cur_filename;

/* The current output file handle */
FILE *cur_file;

/* The header array */
entry *header;

/* The current size of the header array */
size_t header_size;

/* Option to print header */
int write_header;

/* Don't print the break field if set */
int remove_break_field;

/* cleanup() will remove files at exit if set */
int call_remove_files = 1;

int close_one_file(void);
void select_file(char *field_value, size_t len);
void print_record(void);
void free_files(void);
void close_files(void);
void remove_files(void);
void usage (int status);
char * make_file_name(char *data);
void cb1 (void *data, size_t len, void *vp);
void cb2 (int c, void *vp);
void make_header(void);
void print_header(void);
void cleanup(void);


void
cleanup(void)
{
  /* Free memory for entry_array, header,
     and file_array and close open files */

  /* Only to be called once by atexit! */
  size_t i;

  /* Remove files if unsuccessful termination */
  if (call_remove_files)
    remove_files();

  for (i = 0; entry_array && i < entry_array_size; i++)
    free(entry_array[i].data);
  free(entry_array);

  for (i = 0; header && i < header_size; i++)
    free(header[i].data);
  free(header);

  for (i = 0; file_array && i < file_array_size; i++) {
    free(file_array[i].name);
    free(file_array[i].filename);
    if (file_array[i].fp)
      fclose(file_array[i].fp);
  }
  free(file_array);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s -f FIELD [OPTIONS]... [FILE]\n\
Break CSV records into multiple files based on the value of the specified field\n\
\n\
  -c, --print-counts           don't break file, just print counts by value\n\
  -d, --delimiter=DELIM_CHAR   use DELIM_CHAR instead of comma as delimiter\n\
  -f, --field=FIELD            field name or number to break on\n\
  -h, --header                 print the header record to each file created\n\
", program_name);
    printf("\
  -q, --quote=QUOTE_CHAR       use QUOTE_CHAR instead of double quote as quote\n\
                               character\n\
  -r, --remove-break-field     do not print the break field to created files\n\
  -s, --strict                 enforce strict mode, mal-formed CSV files will\n\
                               cause an error\n\
  -S, --suffix                 the suffix to use for the created files\n\
                               the default is .csv\n\
");
    printf("\
  -P, --prefix                 the prefix to use for the created files\n\
      --version                display version information and exit\n\
      --help                   display this help and exit\n\
");
  }
  exit(status);
}

void
make_header(void)
{
  size_t i;
  header = xmalloc(entry_array_size * sizeof(struct entry));
  for (i = 0; i < entry_array_size; i++) {
    header[i].size = entry_array[i].size;
    header[i].data = xmalloc(entry_array[i].size);
    memcpy(header[i].data, entry_array[i].data, entry_array[i].size);
    header_size++;
  }
}

int
close_one_file(void)
{
  size_t i;

  for (i = 0; i < file_array_size; i++) {
    if (file_array[i].fp) {
      fclose(file_array[i].fp);
      file_array[i].fp = NULL;
      return 0;
    }
  }

  /* No files to close */
  return -1;
}

void
select_file(char *field_value, size_t len)
{
  /* Find the file handle if open, otherwise open the file
     Set cur_file to the file handle */
  file *ptr;
  char *str_value;
  size_t i = 0;

  str_value = Strndup(field_value, len);

  while (i < file_array_size) {
    if (!strcmp(file_array[i].name, str_value)) {
      /* Found a match */
      free(str_value);
      file_array[i].count++;

      if (just_print_counts)
        return;

      if (file_array[i].fp) {
        cur_file = file_array[i].fp;
        return;
      }

      file_array[i].fp = fopen(file_array[i].filename, "ab");
      if (!file_array[i].fp) {
        /* Can't open file, may be too many open, try closing one first */
        if (close_one_file() != 0)
          err("Failed to open file");
        file_array[i].fp = fopen(file_array[i].filename, "ab");
        if (!file_array[i].fp)
          err("Failed to open file");
      }
   
      return;
    }
    i++;
  }

  /* Not found, add */
  file_array = xrealloc(file_array, (file_array_size+1) * sizeof(struct file));
  file_array_size++;

  ptr = &file_array[file_array_size - 1];
  ptr->filename = NULL; 
  ptr->fp = NULL;
  ptr->name = str_value;
  ptr->filename = make_file_name(str_value);
  ptr->count = 1;
 
  if (just_print_counts) return;
 
  ptr->fp = fopen(ptr->filename, "wb");
  if (!ptr->fp) {
    if (close_one_file() != 0)
      err("Failed to open file");
    ptr->fp = fopen(ptr->filename, "ab");
    if (!ptr->fp)
      err("Failed to open file");
  }

  if (ptr->fp == NULL)
    err("Failed to open file");

  cur_file = ptr->fp;

  /* Print header if we opened a new file */
  if (write_header)
    print_header();
}

void
print_record(void)
{
  int first_field = 1;
  size_t idx;

  for (idx = 0; idx < current_field; idx++) {
    if (remove_break_field && idx + 1 == break_field)
      continue;

    if (first_field)
      first_field = 0;
    else
      fputc(delimiter, cur_file);

    csv_fwrite2(cur_file,
                entry_array[idx].size ? entry_array[idx].data : "",
                entry_array[idx].size, quote);
  }
  fputc('\n', cur_file);
}

void
print_header(void)
{
  int first_field = 1;
  size_t idx;

  if (header_size == 0)
    return;

  for (idx = 0; idx < header_size; idx++) {
    if (remove_break_field && idx + 1 == break_field)
      continue;

    if (first_field)
      first_field = 0;
    else
      fputc(delimiter, cur_file);

    csv_fwrite2(cur_file,
                header[idx].size ? header[idx].data : "",
                header[idx].size, quote);
  }
  fputc('\n', cur_file);
}

void
print_counts(void)
{
  size_t i = file_array_size;
  while (i) {
    i--;
    printf("%s: %lu\n", file_array[i].name, file_array[i].count);
  }
}

void
remove_files(void)
{
  size_t i = file_array_size;
  while (i--) {
    /* Close file if open, some OSes won't remove an open file */
    if (file_array[i].fp) {
      fclose(file_array[i].fp);
      file_array[i].fp = NULL;  /* So the cleanup function won't close again */
    }
    remove(file_array[i].filename);
  }
}

char *
make_file_name(char *name)
{
  char *filename = xmalloc(strlen(name) + strlen(filename_prefix) + strlen(filename_suffix) + 1);
  strcpy(filename, filename_prefix);
  strcat(filename, name);
  strcat(filename, filename_suffix);
  return filename;
}

void
cb1 (void *data, size_t len, void *vp)
{
  if (need_name_resolution) {
    if ((strlen(break_field_name) == len) && !strncmp(break_field_name, data, len)) {
      break_field = current_field + 1;
      need_name_resolution = 0;
    }
  }

  /* Check for element to hold entry, create one if needed */
  if (current_field >= entry_array_size) {
    entry_array = xrealloc(entry_array, (entry_array_size+1) * sizeof(struct entry));
    entry_array_size++;
    entry_array[entry_array_size-1].data = NULL;
    entry_array[entry_array_size-1].size = 0;
    entry_array[entry_array_size-1].alloc_size = 0;
  }

  /* Element exists at this point, make sure it is large enough */
  if (entry_array[current_field].alloc_size < len) {
    entry_array[current_field].data = xrealloc(entry_array[current_field].data, len);
    entry_array[current_field].alloc_size = len;
  }

  memcpy(entry_array[current_field].data, data, len);
  entry_array[current_field].size = len;

  if (current_field + 1 == break_field && !(first_record && write_header))
    select_file(data, len);

  current_field++;
}

void
cb2 (int c, void *vp) 
{
  /* No longer first record when first non-empty record seen */
  if (first_record && current_field > 0) {
    if (write_header)
      make_header();
    else
      if (break_field <= current_field) print_record();
    first_record = 0;
  } else {
    if (need_name_resolution && !first_record) {
      /* Didn't find field name */
      fprintf(stderr, "Couldn't find field '%s'\n", break_field_name);
      exit(EXIT_FAILURE);
    } else {
      if (!just_print_counts)
        if (break_field <= current_field) print_record();
    }
  }

  current_field = 0;
  current_record++;
}


int
main (int argc, char *argv[])
{
  int optc;
  struct csv_parser p;
  size_t bytes_read;
  char buf[1024];

  program_name = argv[0];

  while ((optc = getopt_long(argc, argv, "cd:f:hq:rsS:P:", longopts, NULL)) != -1)
    switch (optc) {
      case 'c':
        just_print_counts = 1;
        break;

      case 'd':
        delimiter_name = optarg;
        if (strlen(delimiter_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          delimiter = delimiter_name[0];
        break;

      case 'f':
        break_field_name = optarg;
        break;

      case 'h':
        write_header = 1;
        break;

      case 'q':
        quote_name = optarg;
        if (strlen(quote_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          quote = quote_name[0];
        break;

      case 'r':
        remove_break_field = 1;
        break;
 
      case 's':
        strict = 1;
        break;
 
      case 'P':
        filename_prefix = optarg;
        break;

      case 'S':
        filename_suffix = optarg;
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

  atexit(cleanup);

  if (!break_field_name)
    err("Must specify a field to break on");

  if (Is_numeric(break_field_name))
    break_field = strtoul(break_field_name, NULL, 10);
  else
    write_header = need_name_resolution = 1;

  if (optind < argc) {
    if (optind + 1 < argc)
      usage(EXIT_FAILURE);
    infile = fopen(argv[optind], "rb");
    if (!infile)
      err("Could not open file");
  } else {
    infile = stdin;
  }

  csv_init(&p, strict ? CSV_STRICT|CSV_STRICT_FINI : 0);
  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  while ((bytes_read=fread(buf, 1, 1024, infile)) > 0) {
    if (csv_parse(&p, buf, bytes_read, cb1, cb2, NULL) != bytes_read) {
      fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
      exit(EXIT_FAILURE);
    }
  }

  if (csv_fini(&p, cb1, cb2, NULL)) {
    fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
    exit(EXIT_FAILURE);
  }

  csv_free(&p);

  if (just_print_counts)
    print_counts();

  call_remove_files = 0;
  exit(EXIT_SUCCESS);
}
  
