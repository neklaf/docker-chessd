#ifndef __WN_UTILS_
#define __WN_UTILS_
#include <stdarg.h>

int wn_vpprintf(int p, char *format, va_list ap);
int wn_pprintf(int p, char *format,...);
int wn_data(int p, int count, ...);

#endif
