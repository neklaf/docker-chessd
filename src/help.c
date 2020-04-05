/*
   Copyright (C) Andrew Tridgell 2002
   Copyright (c) 2003 Federal University of Parana

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*
  support for the 'help' and 'ahelp' commands
 */
#include "config.h"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <string.h>

#include "adminproc.h"
#include "chessd_locale.h"
#include "command_network.h"
#include "configuration.h"
#include "malloc.h"
#include "utils.h"

char *locale_help_dir;
char *locale_ahelp_dir;
char *locale_usage_dir;

/*
  Given a directory name, returns name of a subdirectory identical
  to current locale.  If there is no such directory, returns the subdirectory
  "C".

  Returns NULL on error
*/

static char* find_locale_subdir(char* dirname)
{
    DIR *dir;
    struct dirent *dirent;
    char *buf;

    if ((dir = opendir(dirname)) == NULL) {
        d_printf(strerror(errno));
        return NULL;
    }

    /* breaks mysteriously */
    asprintf(&buf, "%s/C", dirname);
//        buf = malloc(strlen(dirname) + 3);
//        strcat(buf,dirname);
//        strcat(buf,"/C");

    while ((dirent = readdir(dir))) {
        if (strcmp(LOCALE_MESSAGES, dirent->d_name) == 0) {
//                        buf = malloc(strlen(LOCALE_MESSAGES) + strlen(dirname) + 2);
//                        strcat(buf,dirname);
//                        strcat(buf,"/");
//                        strcat(buf,LOCALE_MESSAGES);
            /* breaks mysteriously */
            asprintf(&buf, "%s/%s", dirname, LOCALE_MESSAGES);
            break;
        }
    }
    closedir(dir);
    return buf;
}

int help_init()
{
	d_printf("CHESSD: HELP_DIR = %s\n", HELP_DIR);
    locale_help_dir = strdup(find_locale_subdir(HELP_DIR));
    if (!locale_help_dir)
        return -1;

	d_printf("CHESSD: AHELP_DIR = %s\n", AHELP_DIR);
    locale_ahelp_dir = strdup(find_locale_subdir(AHELP_DIR));
    if (!locale_ahelp_dir)
        return -1;

	d_printf("CHESSD: USAGE_DIR = %s\n", USAGE_DIR);
    locale_usage_dir = strdup(find_locale_subdir(USAGE_DIR));
    if (!locale_usage_dir)
        return -1;

    return 0;
}

int help_free()
{
	FREE(locale_help_dir);
	FREE(locale_ahelp_dir);
	FREE(locale_usage_dir);
	return 0;
}

static int help_page(int p, const char *dir, const char *cmd)
{
	int count;
	char *filenames[1000];

	/* try for help dir exact match */
	if (psend_file(p, dir, cmd) == 0) {
		return COM_OK;
	}

	/* search help dir for a partial match */
	count = search_directory(dir, cmd, filenames, 1000);
	d_printf("HELP.C: dir %s, cmd %s\n", dir, cmd);
	if (count == 1) {
		psend_file(p, dir, filenames[0]);
		return COM_OK;
	}

	if (count > 0) {
		pprintf(p,_("-- Matches: %u help topics --"), count);
		display_directory(p, filenames, count);
		return COM_OK;
	}

	/* allow admins to use 'help' for admin pages */
	if (dir != locale_ahelp_dir && check_admin(p, ADMIN_ADMIN)) {
		return help_page(p, locale_ahelp_dir, cmd);
	}

	pprintf(p,_("No help found for topic '%s'\n"), cmd);

	return COM_OK;
}

/*
  public help
 */
int com_help(int p, param_list param)
{
	char *cmd;
	int i, n;
	const struct alias_type *list;

	if (param[0].type == TYPE_STRING) {
		cmd = param[0].val.string;
	} else {
		cmd = "help";
	}

	/* see if its a global alias */
	list = alias_list_global();
	for (i=0;list && list[i].comm_name;i++) {
		if (strcasecmp(cmd, list[i].comm_name) == 0) {
			pprintf(p,_("'%s' is a global alias for '%s'\n"),
				cmd, list[i].alias);
			return COM_OK;
		}
	}

	/* or a personal alias */
	list = alias_list_personal(p, &n);
	for (i=0;i<n;i++) {
		if (strcasecmp(cmd, list[i].comm_name) == 0) {
			pprintf(p,_("'%s' is a personal alias for '%s'\n"),
				cmd, list[i].alias);
			return COM_OK;
		}
	}

	return help_page(p, locale_help_dir, cmd);
}

/*
  admin only help
 */
int com_ahelp(int p, param_list param)
{
	char *cmd;

	if (param[0].type == TYPE_STRING) {
		cmd = param[0].val.string;
	} else {
		cmd = "commands";
	}

	return help_page(p, locale_ahelp_dir, cmd);
}

