#ifndef CSVUTILS_HELPER_H__
#define CSVUTILS_HELPER_H__

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

void * xmalloc(size_t size);
void * xrealloc(void *p, size_t size);
void err(char *msg);
char *Strdup(char *s);
char *Strndup(char *s, size_t len);
int Is_numeric(char *s);
void Strupper(char *s);

#endif
