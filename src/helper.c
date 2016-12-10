#include "helper.h"

void *
xmalloc(size_t size)
{
  void *ptr = malloc(size);
  if (!ptr)
    err("Out of memory");
  return ptr;
}

void *
xrealloc(void *ptr, size_t size)
{
  ptr = realloc(ptr, size);
  if (!ptr)
    err("Out of memory");
  return ptr;
}

void
err (char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

int
Is_numeric(char *s)
{
  /* Returns true if string consist entirely of digits */
  while (*s)
    if (!isdigit((int)(unsigned char)*(s++))) return 0;
  return 1;
}

char *
Strdup(char *s)
{
  char *tmp = xmalloc(strlen(s) + 1);
  strcpy(tmp, s);
  return tmp;
}

char *
Strndup(char *s, size_t len)
{
  char *tmp = xmalloc(len + 1);
  memcpy(tmp, s, len);
  tmp[len] = '\0';
  return tmp;
}

void
Strupper(char *s)
{
  while (*s) {
    *s = toupper((int)(unsigned char)*s);
    s++;
  }
}
