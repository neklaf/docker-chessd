/*
 * Some special commands for debugging
 *
 */
#include "config.h"

#include <locale.h>
#include <string.h>
#include <libintl.h>

#include "command.h"
#include "debug_com.h"
#include "utils.h"

/* command used to get the current locale */
int com_getlocale(int p, param_list param)
{
	pprintf(p, "%s\n", setlocale(LC_ALL, NULL));
	return COM_OK;
}

/* command used to set the locale */
int com_setlocale(int p, param_list param)
{
	char *which_locale;
	which_locale = setlocale(LC_ALL, param[0].val.word);

	if (strcmp(which_locale, param[0].val.word)) {
		// failed to modify the locale
		pprintf(p, "Failed to modify to locale %s. Locale is %s\n",
				param[0].val.word, which_locale);
	}
	else {
		bindtextdomain(PACKAGE, LOCALEDIR);
		textdomain(PACKAGE);
		pprintf(p, "Locale modified to %s\n", param[0].val.word);
	}

	return COM_OK;
}

