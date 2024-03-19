/*

    File: lib.c


*/
#include <FreeRTOS.h>
#include <task.h>

#include "platform_opts.h"

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "strproc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
//#include <ctype.h>
//#include <unistd.h>

#include "config.h"
#include "lib.h"
#include "common.h"
/*
#define	DEBUG(x)
*/
extern void *pvPortReAlloc(void *pv,  size_t xWantedSize);
#define islower(c)	(((c)>='a')&&((c)<='z'))
#define func_toupper(c) (islower(c) ? 'A' + ((c) - 'a') : (c))
#define isupper(c)	(((c)>='A')&&((c)<='Z'))
static inline unsigned char func_tolower(unsigned char c)
{
	if (isupper(c)) {
		c -= 'A' - 'a';
	}
	return c;
}

void *allocate(size_t size)
{
	void	*p;

	if ((p = malloc(size)) == NULL) {
		log_msg("memory allocation error");
		return NULL;
	}

	memset(p, 0, size);
	return (p);
}

void *reallocate(void *p, size_t size)
{
	p = (void *)realloc(p, size);
	if (p == NULL) {
		log_msg("memory allocation error");
	}

	return (p);
}


char *strlwr(char *string)
{
	unsigned int c;
	char *p;
	p = string;
	while ((c = *p) != 0) {
		*p++ = func_tolower(c);
	}
	return (string);
}

char *strupr(char *string)
{
	unsigned int c;
	char *p;

	p = string;
	while ((c = *p) != 0) {
		*p++ = func_toupper(c);
	}

	return (string);
}


char *skip_ws(char *string)
{
	unsigned int c;

	while ((c = *string) == ' '  ||  c == '\t') {
		string++;
	}

	return (string);
}

char *noctrl(char *buffer)
{
	int	len, i;
	char *p;

	if ((p = buffer) == NULL) {
		return (NULL);
	}

	len = strlen(p);
	for (i = len - 1; i >= 0; i--) {
		if (p[i] <= 32) {
			p[i] = '\0';
		} else {
			break;
		}
	}

	return (p);
}

char *get_word(char **from, char *to, int maxlen)
{
	unsigned int c;
	char *p;
	int	k;

	maxlen -= 2;
	while ((c = **from) != 0  &&  c <= 32) {
		*from += 1;
	}

	*(p = to) = k = 0;
	while ((c = **from) != 0) {
		if (c == ' '  ||  c == '\t'  ||  c < 32) {
			break;
		}

		*from += 1;
		if (k < maxlen) {
			p[k++] = c;
		}
	}

	p[k] = 0;
	return (to);
}

char *get_quoted(char **from, int delim, char *to, int max)
{
	int c;
	int	k;

	to[0] = k = 0;
	max -= 2;

	while ((c = **from) != 0) {
		*from += 1;
		if (c == delim) {
			break;
		}

		if (k < max) {
			to[k++] = c;
		}
	}

	to[k] = 0;
	return (to);
}

char *copy_string(char *y, char *x, unsigned int len)
{
	x = skip_ws(x);
	noctrl(x);

	len -= 2;
	if (strlen(x) >= len) {
		x[len] = 0;
	}

	if (y != x) {
		strcpy(y, x);
	}

	return (y);
}


unsigned int get_stringcode(char *string)
{
	unsigned int c, code;
	int	i;
	code = 0;
	for (i = 0; (c = (unsigned char) string[i]) != 0; i++) {
		if (isupper(c)) {
			c = func_tolower(c);
		}

		code = code + c;
	}
	code = (code & 0xFF) + (strlen(string) << 8);
	return (code);
}


/* in case we dont have strnlen */
#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
	size_t len = 0;
	while (*s++ && len < maxlen) {
		len++;
	}
	return (len);
}
#endif

#ifndef HAVE_USLEEP
/*

  usleep -- support routine for 4.2BSD system call emulations
  last edit:  29-Oct-1984     D A Gwyn
*/

extern int        select();

int usleep(usec)                /* returns 0 if ok, else -1 */
long       usec;           /* delay in microseconds */
{
	static struct {               /* `timeval' */
		long        tv_sec;         /* seconds */
		long        tv_usec;        /* microsecs */
	}   delay;                    /* _select() timeout */

	delay.tv_sec = usec / 1000000L;
	delay.tv_usec = usec % 1000000L;

	return select(0, (long *)0, (long *)0, (long *)0, &delay);
}
#endif

char *dnrd_strdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if (!(copy = (char *)malloc(len))) {
		return 0;
	}
	memset(copy, 0x00, len);
	memcpy(copy, str, len);
	return copy;
}

