/* See COPYRIGHT file for copyright and license details */

#ifndef _STR_H
#define _STR_H

#include <ctype.h>
#include <sys/types.h>
#include <string.h>

/* Types and pseudo-types */
typedef struct {
	char *data;
	size_t len;
} String;

#define Array(TYPE) struct { TYPE *data; size_t len; }

/* String functions */
String toString(char *s);
int Strcmp(String a, String b);
int Strcmpc(String s1, char *s2);
ssize_t Strtok(String string, String *out, char c);
ssize_t Strtok2(String *i, String *o, char c);
String Striden(String string);
String Strtrim(String str);

/* Array functions */
int _inArray(char *data, size_t len, void *val, size_t vlen);
#define inArray(ARR, VAL) (_inArray( \
				   (void *)(ARR).data, (ARR).len, &(VAL), sizeof (VAL)))
void *_prepareArray(void *data, size_t siz);
#define prepareArray(ARR) (_prepareArray(&(ARR), sizeof (ARR)))

#define lastinArray(ARR) ((ARR).data[(ARR).len - 1])

/* Vector - dynamic Array */
#define newVector(ARR) ((ARR).data = malloc((ARR).len = 0))
#define pushVector(ARR, VAL) (((ARR).data = \
			realloc((ARR).data, \
				++((ARR).len) * (sizeof *((ARR).data)))), \
		(ARR).data[(ARR).len - 1] = (VAL))

#endif
