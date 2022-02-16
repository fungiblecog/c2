#ifndef _PRINTER_H
#define _PRINTER_H

#include <stdarg.h>
#include "types.h"

#define UNREADABLY 0
#define READABLY 1

char *pr_str(MalType *mal_val, int readably);
char *snprintfbuf(long initial_size, char *fmt, ...);
#endif
