/*
csvcut -   cut the specified fields from a CSV file

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

/*lint -esym(534, csv_fwrite2, atexit) -emacro(534, putc) */
/*lint -esym(715, not_a_space, field_spec_cb1, field_spec_cb2, cb1, cb2) */
/*lint -ecall(732, csv_set_delim, csv_set_quote, csv_fwrite2) */
/*lint -esym(750, AUTHORS) */
/*lint -e801 goto used */
/*lint -esym(818, cb1) */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <csv.h>
#include "version.h"
#include "helper.h"

#define PROGRAM_NAME "csvcut"
#define AUTHORS "Robert Gamble"

typedef struct entry {
  size_t size;       /* The size of the current entry */
  size_t alloc_size; /* The size allocated for data */
  char *data;        /* The field data */
} entry;

/* Each field specification is stored in a field_spec structure */
typedef struct field_spec {
  char *start_name;
  char *stop_name;
  size_t start_value;
  size_t stop_value;
} field_spec;

static struct option const longopts[] =
{
  {"fields", required_argument, NULL, 'f'},
  {"delimiter", required_argument, NULL, 'd'},
  {"quote", required_argument, NULL, 'q'},
  {"strict", no_argument, NULL, 's'},
  {"complement", no_argument, NULL, 'c'},
  {"make-empty-fields", no_argument, NULL, 'm'},
  {"reresolve-fields", no_argument, NULL, 'r'},
  {"version", no_argument, NULL, CHAR_MAX + 1},
  {"help", no_argument, NULL, CHAR_MAX + 2},
  {NULL, 0, NULL, 0}
};

/* The current csv field being processed, starting at 0 */
size_t current_field;         

/* if set output all fields except those selected */
int complement;

/* The quote character */
char quote = CSV_QUOTE;

/* The delimiter character */
char delimiter = CSV_COMMA;

/* The quote argument */
const char *quote_string;

/* The delimiter argument */
const char *delimiter_string;

/* strict mode in effect if set */
int strict;

/* The number of fields waiting for name resolution */
int unresolved_fields = 0;

/* The name this program was called with */
char *program_name;

/* The current output file */
FILE *outfile;

/* Pointer to the array of entries */
entry *entry_array;

/* Pointer to the array of field specifications */
field_spec *field_spec_array;

/* Size of the field_spec_array */
size_t field_spec_size = 0;

/* The current size of the entry array */
size_t entry_array_size = 0;

/* The field specifications passed to the program */
const char *field_spec_arg;

/* Haven't seen a non-empty record if set */
int first_record = 1;

/* Create emptry fields as needed if set */
int make_empty_fields;

/* If set, re-resolve field names for every file */
int reresolve;

/* Function Prototypes */
void cb1 (void *s, size_t len, void *data);
void cb2 (int c, void *data);
void field_spec_cb1 (void *s, size_t len, void *data);
void field_spec_cb2 (int c, void *data);
void cut_file(char *filename);
void usage (int status);
void add_field_spec(char *start, char *stop, size_t start_value, size_t stop_value);
void process_field_specs(const char *f);
void print_unresolved_fields(void);
void unresolve_fields(void);
void cleanup(void);


/* Functions */
void
cleanup(void)
{
  /* Free memory for entry_array */
  /* Only to be called once by atexit! */
  size_t i;
  for (i = 0; i < entry_array_size; i++)
    free(entry_array[i].data);
  free(entry_array);
  /* field_spec_array: */
  if (field_spec_array) {
    for (i=0; i<field_spec_size; ++i) {
      if (field_spec_array[i].start_name)
        free(field_spec_array[i].start_name);
      if (field_spec_array[i].stop_name)
        free(field_spec_array[i].stop_name);
    }
    free(field_spec_array);
  }
}

void
print_unresolved_fields(void)
{
  /* Print the first unresolved field name found and exit */
  size_t i;
  fprintf(stderr, "Unable to resolve the field ");
  for (i = 0; i < field_spec_size; i++) {
    if (field_spec_array[i].start_name != NULL && field_spec_array[i].start_value == 0) {
      fprintf(stderr, "'%s' ", field_spec_array[i].start_name);
      break;
    }
    if (field_spec_array[i].stop_name != NULL && field_spec_array[i].stop_value == 0) {
      fprintf(stderr, "'%s' ", field_spec_array[i].stop_name);
      break;
    }
  }
  puts("");
  exit(EXIT_FAILURE);
}

void
unresolve_fields(void)
{
  /* Unresolve all field thats originally needed resolving for -? option */
  size_t i;
  for (i = 0; i < field_spec_size; i++) {
    if (field_spec_array[i].start_name != NULL) {
      field_spec_array[i].start_value = 0;
      unresolved_fields++;
    }
    if (field_spec_array[i].stop_name != NULL) {
      field_spec_array[i].stop_value = 0;
      unresolved_fields++;
    }
  }
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s [OPTIONS]... [FILE]...\n\
Print selected fields of CSV files or CSV data received from standard input\n\
\n\
  -f, --fields=FIELD_LIST      comma seperated list of fields to select\n\
  -d, --delimiter=DELIM_CHAR   use DELIM_CHAR instead of comma as delimiter\n\
  -q, --quote=QUOTE_CHAR       use QUOTE_CHAR instead of double quote as quote\n\
                               character\n\
", program_name);
    printf("\
  -r, --reresolve-fields       re-resolve the field names specified for each\n\
                               file processed instead of using the positions\n\
                               resolved from the first file\n\
  -s, --strict                 enforce strict mode, mal-formed CSV files will\n\
                               cause an error\n\
");
    printf("\
  -c, --complement             output all fields except those specified\n\
  -m, --make-empty-fields      cause the creation of empty fields for those\n\
                               specified in the field specs but not in the data\n\
      --version                display version information and exit\n\
      --help                   display this help and exit\n\
");
  }
  exit(status);
}

int
not_a_space(unsigned char c)
{
  (void)c;
  /* Preserve spaces when parsing field specs */
  return 0;
}

void
process_field_specs(const char *f)
{
  struct csv_parser p;
  const char *errmsg[] = {
    "Failed to initialize csv parser",
    "Invalid field spec",
    "Field list cannot be empty"
  };
  const char *errstr = NULL;

  size_t len = strlen(f);

  if (csv_init(&p, CSV_STRICT|CSV_STRICT_FINI))
    errstr = errmsg[0];

  if (!errstr) csv_set_space_func(&p, not_a_space);

  if ((!errstr) && (csv_parse(&p, f, len, field_spec_cb1, field_spec_cb2, NULL) != len))
    errstr = errmsg[1];

  if ((!errstr) && (csv_fini(&p, field_spec_cb1, field_spec_cb2, NULL)))
    errstr = errmsg[1];

  if ((!errstr) && (field_spec_size == 0))
    errstr = errmsg[2];

  csv_free(&p);

  if (errstr) err(errstr);
}


void
add_field_spec(char *start, char *stop, size_t start_value, size_t stop_value)
{
  field_spec *ptr;
  field_spec_array = xrealloc(field_spec_array, (field_spec_size+1) * sizeof(field_spec));
  field_spec_size++;
  ptr = &field_spec_array[field_spec_size-1];
  ptr->start_name = start;
  ptr->stop_name = stop;
  ptr->start_value = start_value;
  ptr->stop_value = stop_value;
}

void
cut_file(char *filename)
{
  FILE *fp;
  struct csv_parser p;
  char buf[1024];
  size_t bytes_read;

  if (csv_init(&p, strict ? CSV_STRICT|CSV_STRICT_FINI : 0) != 0)
    err("Failed to initialize csv parser");

  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  if (filename == NULL || !strcmp(filename, "-")) {
    fp = stdin;
  } else {
    fp = fopen(filename, "rb");
  }

  if (!fp) {
    fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
    csv_free(&p);
    return;
  }

  while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
    if (csv_parse(&p, buf, bytes_read, cb1, cb2, NULL) != bytes_read) {
      fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
      csv_free(&p);
      fclose(fp);
      return;
    }
  }

  if (csv_fini(&p, cb1, cb2, NULL) != 0) {
    fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
    csv_free(&p);
    fclose(fp);
    return;
  }

  csv_free(&p);

  if (ferror(fp)) {
    fprintf(stderr, "Error reading file %s\n", filename);
    fclose(fp);
    return;
  }

  fclose(fp);
}

void
field_spec_cb1(void *s, size_t len, void *data)
{
  size_t left_size = 0, right_size = 0;
  long unsigned left_value = 0, right_value = 0;
  char *left = NULL, *right = NULL;
  char *ptr = NULL, *s_copy = NULL;
  int left_ended = 0;
  (void)data;

  ptr = s_copy = Strndup(s, len);

  while (*ptr) {
    if (*ptr == '-') {
      if (left_ended || left_size == 0)
        err("Invalid field spec");
      if (left) free(left);
      left = Strndup(s, left_size);
      if (Is_numeric(left)) {
        left_value = strtoul(left, NULL, 10);
        if (left_value == 0)
          err("0 is not a valid field index");
      } else
        unresolved_fields++;
      left_ended = 1;
      right = ++ptr;
    } else {
      if (left_ended) {
        right_size++;
      } else {
        left_size++;
      }
      ptr++;
    }
  }

  if (left_ended) {
    right = Strndup(right, right_size);
    if (Is_numeric(right)) {
      right_value = strtoul(right, NULL, 10);
      if (right_value == 0)
        err("0 is not a valid field index");
    } else
      unresolved_fields++;
  } else {
    if (left) free(left);
    left = Strndup(s, left_size);
    if (Is_numeric(left)) {
      left_value = strtoul(left, NULL, 10);
      if (left_value == 0)
        err("0 is not a valid field index");
    } else {
      right = Strndup(left, left_size);
      unresolved_fields+=2;
    }
    right_value = left_value;
  }
  /* printf("left = %d (%s), right = %d (%s)\n", left_value, left, right_value, right); */
  if (left_value != 0) {
    free(left);
    left = NULL;
  }
  if (right_value != 0) {
    free(right);
    right = NULL;
  }

  add_field_spec(left, right, left_value, right_value);

  free(s_copy);
}

void
field_spec_cb2(int c, void *data)
{
  (void)data;
  /* Field spec should not contain newlines */
  if (c >= 0)
    err("Invalid field spec");
}

void
cb1(void *s, size_t len, void *data)
{
  size_t i = 0;
  (void)data;

  if (unresolved_fields) {
    if (first_record) {
      while (i < field_spec_size) {
        if (field_spec_array[i].start_value == 0 
            && strlen(field_spec_array[i].start_name) == len 
            && !strncmp(field_spec_array[i].start_name, s, len)) {
              field_spec_array[i].start_value = current_field+1;
              unresolved_fields--;
        }
        if (field_spec_array[i].stop_value == 0 
            && strlen(field_spec_array[i].stop_name) == len 
            && !strncmp(field_spec_array[i].stop_name, s, len)) {
              field_spec_array[i].stop_value = current_field+1;
              unresolved_fields--;
        }
        i++;
      }
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

  memcpy(entry_array[current_field].data, s, len);
  entry_array[current_field].size = len;

  current_field++;
}

void
cb2 (int m, void *data)
{
  size_t i, j; 
  int first_field = 1;

  (void)m;
  (void)data;

  if (first_record && current_field > 0)
    first_record = 0;

  if (unresolved_fields && !first_record)
    print_unresolved_fields();

  if (complement) {
    for (i = 1; i <= current_field; i++) {
      for (j = 0; j < field_spec_size; j++) {
        if (i >= field_spec_array[j].start_value && i <= field_spec_array[j].stop_value)
          goto dont_print;
      }
      /* If got this far, output field */
      if (first_field)
        first_field = 0;
      else
        fputc(delimiter, outfile);
      csv_fwrite2(outfile, 
                  entry_array[i-1].size ? entry_array[i-1].data : "",
                  entry_array[i-1].size, quote);
      dont_print:
        ;
    }
  } else {
    /* Print the fields according to the field specs */
    for (i = 0; i < field_spec_size; i++) {
      for (j = field_spec_array[i].start_value;
           j <= field_spec_array[i].stop_value;
           j++) {
        if (j > current_field)
          if (make_empty_fields)
            if (!first_field) {
              fputc(delimiter, outfile);
              fputc(quote, outfile);
              fputc(quote, outfile);
            } else 
              first_field = 0;
          else
            break;
        else {
          if (first_field)
            first_field = 0;
          else
            fputc(delimiter, outfile);
          csv_fwrite2(outfile,
                      entry_array[j-1].size ?  entry_array[j-1].data : "",
                      entry_array[j-1].size, quote);
        }
      }
    }
  }
  
  current_field = 0;
  putc('\n', outfile);
}

int
main (int argc, char *argv[])
{
  int optc;

  program_name = argv[0];

  while ((optc = getopt_long(argc, argv, "cd:f:mq:rs", longopts, NULL)) != -1)
    switch (optc) {
      case 'c':
        complement = 1;
        break;

      case 'd':
        delimiter_string = optarg;
        if (strlen(delimiter_string) > 1)
          err("delimiter must be exactly one byte long");
        else
          delimiter = delimiter_string[0];
        break;

      case 'f':
        field_spec_arg = optarg;
        break;

      case 'm':
        make_empty_fields = 1;
        break;

      case 'q':
        quote_string = optarg;
        if (strlen(quote_string) > 1)
          err("delimiter must be exactly one byte long");
        else
          quote = quote_string[0];
        break;

      case 'r':
        reresolve = 1;
        break;

      case 's':
        strict = 1;
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

  if (field_spec_arg)
    process_field_specs(field_spec_arg);
  else 
    err("You must specify a list of fields");

  outfile = stdout;

  if (optind < argc) {
    while (optind < argc) {
      cut_file(argv[optind++]);
      if (reresolve) {
        unresolve_fields();
        first_record = 1;
      }
    }
  } else {
    /* Process from stdin */
    cut_file(NULL);
  }

  exit(EXIT_SUCCESS);
}

