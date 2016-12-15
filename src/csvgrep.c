/*
csvgrep - search for a pattern in the specified fields of a CSV file

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

/*lint -esym(534, csv_fwrite2, regerror, atexit) ignoring return value */
/*lint -esym(715, field_spec_cb1, field_spec_cb2, cb1, cb2) */
/*lint -ecall(732, csv_fwrite2, csv_set_delim, csv_set_quote) */
/*lint -esym(750, AUTHORS) */
/*lint -e801 goto used */
/*lint -esym(818, cb1) */
/*lint -esym(843, preg) */
/*lint -esym(844, re) */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <getopt.h>

#ifndef WITHOUT_POSIX
#  include <regex.h>
#  define POSIX_SUPPORT ""
#else
#  define POSIX_SUPPORT " (compiled without posix support)"
#endif

#ifndef WITHOUT_PCRE
#  include <pcre.h>
#  define PCRE_SUPPORT ""
#else
#  define PCRE_SUPPORT " (compiled without pcre support)"
#endif

#include <csv.h>
#include "version.h"
#include "helper.h"

#define PROGRAM_NAME "csvgrep"
#define AUTHORS "Robert Gamble"


typedef struct entry {
  size_t size;       /* The size of the current entry */
  size_t alloc_size; /* The size allocated for data */
  char *data;        /* The field data */
} entry;

typedef struct field_spec {
  char *start_name;
  char *stop_name;
  size_t start_value;
  size_t stop_value;
} field_spec;

enum { NONE, FIXED, EXTENDED, PCRE } match_type = NONE;

static struct option const longopts[] = 
{
  {"fields", required_argument, NULL, 'f'},
  {"count", no_argument, NULL, 'c'},
  {"perl-regexp", no_argument, NULL, 'P'},
  {"extended-regexp", no_argument, NULL, 'E'},
  {"ignore-case", no_argument, NULL, 'i'},
  {"delimiter", required_argument, NULL, 'd'},
  {"quote", required_argument, NULL, 'q'},
  {"invert-match", no_argument, NULL, 'v'},
  {"strict", no_argument, NULL, 's'},
  {"record-number", no_argument, NULL, 'n'},
  {"fixed-strings", no_argument, NULL, 'F'},
  {"reresolve-fields", no_argument, NULL, 'r'},
  {"files-with-matches", no_argument, NULL, 'l'},
  {"files-without-match", no_argument, NULL, 'L'},
  {"with-filename", no_argument, NULL, 'H'},
  {"no-filename", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, CHAR_MAX + 1},
  {"help", no_argument, NULL, CHAR_MAX + 2},
  {"print-header", no_argument, NULL, CHAR_MAX + 3},
  {"no-print-header", no_argument, NULL, CHAR_MAX + 4},
  {NULL, 0, NULL, 0}
};

char errbuf[255];

/* set to 1 if multiple files provided on commandline */
int multiple_files;

/* prefix output with filenames is set */
int print_filenames;

/* suppress output of filenames is set */
int noprint_filenames;

/* print only filenames that match if set */
int print_matching_filenames;

/* print only filenames that don't match if set */
int print_nonmatching_filenames;

/* The number of fields waiting for name resolution */
int unresolved_fields = 0;

/* Pointer to the array of entries */
entry *entry_array;

/* Pointer to the array of field specifications */
field_spec *field_spec_array;

/* Size of the field_spec_array */
size_t field_spec_size = 0;

/* The current size of the entry array */
size_t entry_array_size = 0;

/* The name this program was called with */
char *program_name;

#ifndef WITHOUT_POSIX
/* regexp compiled regex */
regex_t preg;
#endif

#ifndef WITHOUT_PCRE
/* pcre compiled regex */
pcre *re;
#endif

/* Print line numbers? */
int print_line_no;

/* The number of matches in the current file */
unsigned long cur_matches;

/* The search field argument */
const char *field_spec_arg;

/* The delimiter character */
char delimiter = CSV_COMMA;

/* The delimiter argument */
const char *delimiter_name;

/* The quote character */
char quote = CSV_QUOTE;

/* The quote argument */
const char *quote_name;

/* Print counts instead of matches if true */
int print_count;

/* Ignore case when matching pattern if true */
int ignore_case;

/* Enforce strict CSV? */
int strict;

/* The current field number */
long unsigned current_field;

/* The current record number */
long unsigned current_record = 1; 

/* The search pattern */
char *pattern;

/* True while the current record is the first non-empty record */
int first_record = 1;

/* Print non-matching lines */
int invert_match;

/* Did we match? */
int match;

/* If set, re-resolve field names for every file */
int reresolve;

/* The name of the file being processed */
char *cur_filename;

/* print CSV header if set */
int print_header;

/* do not print CSV header if set */
int no_print_header;

void print_unresolved_fields(void);
void unresolve_fields(void);
void process_field_specs(const char *f);
void add_field_spec(char *start, char *stop, size_t start_value, size_t stop_value);
void field_spec_cb1(void *s, size_t len, void *data);
void field_spec_cb2(int c, void *data);
void print_record(void);
void usage(int status);
int matches_pattern (const char *pat, char *data, size_t len);
void cb1 (void *data, size_t len, void *vp);
void cb2 (int c, void *vp);
void grep_file(char *filename);
void cleanup(void);

void
cleanup(void)
{
  /* Free memory for entry_array and field_spec_array */
  /* Only to be called once by atexit! */
  size_t i;
  for (i = 0; i < entry_array_size; i++)
    free(entry_array[i].data);
  free(entry_array);

  for (i = 0; i < field_spec_size; i++) {
    free(field_spec_array[i].start_name);
    free(field_spec_array[i].stop_name);
  }
  free(field_spec_array);
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
process_field_specs(const char *f)
{
  struct csv_parser p;
  size_t len = strlen(f);
  if (csv_init(&p, CSV_STRICT|CSV_STRICT_FINI))
    err("Failed to initialize csv parser");

  if (csv_parse(&p, f, len, field_spec_cb1, field_spec_cb2, NULL) != len)
    err("Invalid field spec");

  if (csv_fini(&p, field_spec_cb1, field_spec_cb2, NULL))
    err("Invalid field spec");

  if (field_spec_size == 0)
    err("Field list cannot be empty");
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
field_spec_cb1(void *s, size_t len, void *data)
{
  size_t left_size = 0, right_size = 0;
  long unsigned left_value = 0, right_value = 0;
  char *left = NULL, *right = NULL;
  char *ptr;
  int left_ended = 0;

  left = ptr = Strndup(s, len);

  while (*ptr) {
    if (*ptr == '-') {
      if (left_ended || left_size == 0)
        err("Invalid field spec");
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
  if (left_value != 0) {
    free(left);
    left = NULL;
  }
  if (right_value != 0) {
    free(right);
    right = NULL;
  }

  add_field_spec(left, right, left_value, right_value);
}

void
field_spec_cb2(int c, void *data)
{
  /* Field spec should not contain newlines */
  if (c >= 0)
    err("Invalid field spec");
}

void
print_record(void)
{
  int first_field = 1;
  size_t idx = 0;

  if (print_filenames)
    printf("%s:", cur_filename);

  if (print_line_no)
    printf("%lu:", (unsigned long)current_record);

  while (idx < current_field) {

    if (first_field)
      first_field = 0;
    else
      fputc(delimiter, stdout); 

    csv_fwrite2(stdout,
                entry_array[idx].size ? entry_array[idx].data : "",
                entry_array[idx].size, quote);
    idx++;
  }
  fputc('\n', stdout);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help for more information.\n", program_name);
  else {
    printf("\
Usage: %s [OPTIONS]... PATTERN [FILE]...\n\
Search for PATTERN in the provided field of CSV FILES or standard input\n\
\n\
  -f, --fields=FIELD_LIST      search fields in FIELD_LIST\n\
  -c, --count                  print only a count of matching records\n\
  -P, --perl-regexp            interpret PATTERN as a pcre regular expression\n\
  -E, --extended-regexp        interpret PATTERN as an extended regex,\n\
                               this is the default\n\
  -i, --ignore-case            perform a case insensitive match\n\
", program_name);
    printf("\
  -d, --delimiter=DELIM_CHAR   use DELIM_CHAR instead of comma as delimiter\n\
  -q, --quote=QUOTE_CHAR       use QUOTE_CHAR instead of double quote as quote\n\
                               character\n\
  -r, --reresolve-fields       re-resolve the field names specified for each\n\
                               file processed instead of using the positions\n\
                               resolved from the first file\n\
  -v, --invert-match           select all records that do not match pattern\n\
");
    printf("\
  -h, --no-filename            suppress printing of filenames when searching\n\
                               multiple files\n\
  -H, --with-filename          prefix each matching record with the filename\n\
  -l, --files-with-matches     print the name of each file which contains a\n\
                               match instead of the actual matching records\n\
  -L, --files-without-match    print only the name of each file which doesn't\n\
                               contain a match\n\
");
    printf("\
  -s, --strict                 enforce strict mode, mal-formed CSV files will\n\
                               cause an error\n\
  -n, --record-number          prefix matched records with record numbers\n\
");

    printf("\
  -F, --fixed-strings          interpret pattern as a fixed literal string\n\
                               instead of a regular expression\n\
      --print-header           print CSV header, this is the default when\n\
                               non-numeric field names are specified\n\
      --no-print-header        do not print a header\n\
      --version                display version information and exit\n\
      --help                   display this help and exit\n\
");
  }
  exit(status);
}

int
matches_pattern (const char *pat, char *data, size_t len)
{
  char *temp;
  int retval;

  temp = Strndup(data, len);
  if (match_type == FIXED) {
    if (ignore_case) {
      Strupper(temp);  /* uppercase string to match uppercased pattern */
      retval = !strstr(temp, pat);
    } else {
      retval = !strstr(temp, pat);
    }
    free(temp);
    return !retval;
  } else if (match_type == PCRE) {
    #ifndef WITHOUT_PCRE
    return !pcre_exec(re, NULL, data, (int)len, 0, 0, NULL, 0);
    #endif
  } else {
    #ifndef WITHOUT_POSIX
    temp = Strndup(data, len);
    retval = regexec(&preg, temp, (size_t)0, NULL, 0);
    free(temp);
    return !retval;
    #endif
  }
}

void
cb1 (void *data, size_t len, void *vp)
{
  size_t i = 0;
  if (unresolved_fields) {
    /* Print CSV header if non-numeric fields provided and --no-print-header
     * not specified */
    if (no_print_header == 0) print_header = 1;
    if (first_record) {
      while (i < field_spec_size) {
        if (field_spec_array[i].start_value == 0
            && strlen(field_spec_array[i].start_name) == len
            && !strncmp(field_spec_array[i].start_name, data, len)) {
              field_spec_array[i].start_value = current_field+1;
              unresolved_fields--;
        }
        if (field_spec_array[i].stop_value == 0
            && strlen(field_spec_array[i].stop_name) == len
            && !strncmp(field_spec_array[i].stop_name, data, len)) {
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

  memcpy(entry_array[current_field].data, data, len);
  entry_array[current_field].size = len;

  current_field++;
}

void
cb2 (int c, void *vp) 
{
  size_t i, j;

  if (first_record && current_field > 0) {
    first_record = 0;
    if (print_header && !unresolved_fields) {
      print_record();
      goto end;
    }
    if (no_print_header && !unresolved_fields) {
      goto end;
    }
  }

  if (unresolved_fields && !first_record)
    print_unresolved_fields();

  if (cur_matches && (print_matching_filenames || print_nonmatching_filenames))
    goto end;

  for (i = 1; i <= current_field && !match; i++) {
    for (j = 0; j < field_spec_size && !match; j++) {
      if (i >= field_spec_array[j].start_value 
          && i <= field_spec_array[j].stop_value
          && matches_pattern(pattern, entry_array[i-1].data, entry_array[i-1].size))
        match = 1;
    }
  }

  if (match != invert_match) {
    cur_matches++;
    if (print_count || print_matching_filenames || print_nonmatching_filenames)
      ;
    else {
      print_record();
    }
  }

end:
  match = 0;
  current_field = 0;
  current_record++;
}

void
grep_file(char *filename)
{
  FILE *fp;
  struct csv_parser p;
  char buf[1024];
  size_t bytes_read;

  cur_matches = 0;

  if (csv_init(&p, strict ? CSV_STRICT|CSV_STRICT_FINI : 0) != 0)
    err("Failed to initialize csv parser");

  csv_set_delim(&p, delimiter);
  csv_set_quote(&p, quote);

  if (filename == NULL || !strcmp(filename, "-")) {
    fp = stdin;
    cur_filename = "(standard input)";
  } else {
    fp = fopen(filename, "rb");
    cur_filename = filename;
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

  if (print_matching_filenames && cur_matches) {
    printf("%s\n", filename);
  } else if (print_nonmatching_filenames && !cur_matches) {
    printf("%s\n", filename);
  } else if (print_count) {
    if (multiple_files && !noprint_filenames)
      printf("%s:", filename);
    printf("%lu\n", cur_matches);
  }
}

int
main (int argc, char *argv[])
{
  int optc;
  int rv;
  const char *err_ptr;

  program_name = argv[0];
  /* Default matching engine */
  #ifndef WITHOUT_POSIX
  match_type = EXTENDED; /* If posix supported, this is the default */
  #else
  #  ifndef WITHOUT_PCRE
  match_type = PCRE;  /* If pcre supported but not posix, pcre is default */
  #  else
  match_type = FIXED; /* If neither supported, fixed match is default */
  #  endif
  #endif

  while ((optc = getopt_long(argc, argv, "cd:f:hilnrq:svEFHLP", longopts, NULL)) != -1)
    switch (optc) {
      case 'c':
        print_count = 1;
        break;

      case 'd':
        delimiter_name = optarg;
        if (strlen(delimiter_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          delimiter = delimiter_name[0];
        break;

      case 'f':
        field_spec_arg = optarg;
        break;

      case 'h':
        noprint_filenames = 1;
        break;

      case 'H':
        print_filenames = 1;
        break;

      case 'i':
        ignore_case = 1;
        break;

      case 'l':
        print_matching_filenames = 1;
        break;

      case 'L':
        print_nonmatching_filenames = 1;
        break;

      case 'n':
        print_line_no = 1;
        break;

      case 'q':
        quote_name = optarg;
        if (strlen(quote_name) > 1)
          err("delimiter must be exactly one byte long");
        else
          quote = quote_name[0];
        break;

      case 'r':
        reresolve = 1;
        break;

      case 's':
        strict = 1;
        break;
 
      case 'v':
        invert_match = 1;
        break;

      case 'E':
        match_type = EXTENDED;
        break;

      case 'F':
        match_type = FIXED;
        break;

      case 'P':
        match_type = PCRE;
        break;

      case CHAR_MAX + 1:
        /* --version */
        print_version(PROGRAM_NAME POSIX_SUPPORT PCRE_SUPPORT);
        break;

      case CHAR_MAX + 2:
        /* --help */
        usage(EXIT_SUCCESS);
        break;

      case CHAR_MAX + 3:
        /* --print-header */
        print_header = 1;
        break;

      case CHAR_MAX + 4:
        /* --no-print-header */
        no_print_header = 1;
        break;

      default:
        usage(EXIT_FAILURE);
    }

  atexit(cleanup);

  if (!field_spec_arg)
    usage(EXIT_FAILURE);

  process_field_specs(field_spec_arg);

  pattern = argv[optind++];
  if (!pattern)
    usage(EXIT_FAILURE);

  /* Compile pattern */
  if (match_type == FIXED) {
    if (ignore_case) {
      /* Upcase string for case insensitive fixed match*/
      char *ptr = pattern;
      while (*ptr)
        { *ptr = toupper(*ptr), ptr++; }    /*lint !e734 */
    }
  } else if (match_type == PCRE) {
    #ifdef WITHOUT_PCRE
    err("not compiled with pcre support");
    #else
    re = pcre_compile(pattern, 0, &err_ptr, &rv, NULL);
    if (rv) {
      fprintf(stderr, "Error parsing pattern expression: %s\n", err_ptr);
      exit(EXIT_FAILURE);
    }
    #endif
  } else {
    #ifdef WITHOUT_POSIX
    err("not compiled with posix support");
    #else
    if ((rv = regcomp(&preg, pattern, REG_EXTENDED | REG_NOSUB )) != 0) {
      regerror(rv, &preg, errbuf, sizeof errbuf);
      fprintf(stderr, "Error parsing pattern expression: %s\n", errbuf);
      exit(EXIT_FAILURE);
    }
    #endif
  }

  if (argc - optind > 1)
    multiple_files = 1;

  if (optind < argc) {
    while (optind < argc) {
      grep_file(argv[optind++]);
      if (reresolve) {
        unresolve_fields();
        first_record = 1;
      }
    }
  } else {
    /* Process from stdin */
    grep_file(NULL);
  }

  exit(EXIT_SUCCESS);
}
  
