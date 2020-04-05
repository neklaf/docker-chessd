#ifndef _CHESSD_LOCALE_H
#define _CHESSD_LOCALE_H
#include <locale.h>
#include "libintl.h"

/* gettext macros */
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)
#define U_(String) String /* Macro for marking untranslatable strings */

/* macro for getting the current value of LC_MESSAGES */
#define LOCALE_MESSAGES setlocale(LC_MESSAGES, "")

#endif
