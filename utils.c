#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "utils.h"

static void reverse(char s[])
{
    char c;

    for (int i = 0, j = strlen(s) - 1; i < j; ++i, --j)
    {
	c = s[i];
	s[i] = s[j];
	s[j] = c;
    }
}

int snprintf_with_alloc(char **buffer, char *format, ...)
{
    int len, len_prev = 0;
    va_list args, args2;

    va_start(args, format);
    va_copy(args2, args);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (*buffer)
	len_prev = strlen(*buffer);

    if (!(*buffer = realloc(*buffer, len_prev + len + 1)))
	return -1;

    vsnprintf(*buffer + len_prev, len + 1, format, args2);
    va_end(args2);

    return len_prev + len;
}

void itoa(int n, char s[])
{
    int i = 0, sign;

    if ((sign = n) < 0)
	n = -n;

    do {
	s[i++] = n % 10 + '0';
    } while((n /= 10) > 0);

    if (sign < 0)
	s[++i] = '-';

    s[i] = '\0';

    reverse(s);
}
