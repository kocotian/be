/* See COPYRIGHT file for copyright and license details */

#include "str.h"

/* String functions */

String
toString(char *s)
{
	String ret;
	ret.len = strlen(ret.data = s);
	return ret;
}

int
Strcmp(String a, String b)
{
	if (a.len != b.len) return -1;
	return strncmp(a.data, b.data, a.len);
}

int
Strcmpc(String s1, char *s2)
{
	if (s1.len != strlen(s2)) return -1;
	return strncmp(s1.data, s2, s1.len);
}

ssize_t
Strtok(String string, String *out, char c)
{
	char *tmpptr = string.data;

	if (!string.len) return *out = string, 0;
	if (*(tmpptr) != c)
		while ((unsigned)(tmpptr - string.data) <= string.len) {
			if (*(++tmpptr) == c) break;
		}
	string.len = ++tmpptr - string.data;
	*out = string;
	return (tmpptr - string.data);
}

ssize_t
Strtok2(String *i, String *o, char c)
{
	ssize_t n;

	if (!i->len) return 0;

	*o = *i;

	for (n = 0; n < i->len; ++n) {
		if (i->data[n] == c) break;
	}

	o->len = n;
	if (n == i->len) {
		i->data += ++n;
		i->len = 0;
	} else {
		i->data += ++n;
		i->len -= n;
	}

	return n;
}

String
Striden(String str)
{
	size_t i;
	for (i = 0; ((str.data[i] >= 'a' && str.data[i] <= 'z')
				|| (str.data[i] >= 'A' && str.data[i] <= 'Z')
				|| (str.data[i] && str.data[i] >= '0' && str.data[i] <= '9'))
				&& (i < str.len); ++i);
	str.len = i;
	return str;
}

String
Strtrim(String str)
{
	size_t i = 0;
	while (isspace(*(str.data)) && i < str.len) {
		++str.data;
		--str.len;
		++i;
	}
	while (isspace(*(str.data + (str.len - 1))))
		--str.len;
	return str;
}

/* Array functions */
int
_inArray(char *data, size_t len, void *val, size_t vlen)
{
	size_t i, n;
	for (n = i = 0; i < len; ++i, data += vlen)
		if (!memcmp(data, val, vlen)) ++n;
	return (int)n;
}

void *
_prepareArray(void *data, size_t siz)
{
	return memset(data, 0, siz);
}
