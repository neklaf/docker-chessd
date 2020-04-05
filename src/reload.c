/*
   load/unload local variables

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

#include "config.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "chessd_locale.h"
#include "command.h"
#include "eco.h"
#include "gics.h"
#include "globals.h"
#include "help.h"
#include "malloc.h"
#include "ratings.h"
#include "utils.h"
#include "config_genstrc.h"
#include "configuration.h"
#include "dbdbi.h"
#include "seekproc.h"

/*
 *	Maybe this is not the best place for this function. This is
 *	temporarily here just to test the server.
 */
int config_open(void)
{
	dbi_result res;
	int nstrs = NUM_CFG_STRS, nints = NUM_CFG_INTS,
		i, vartype, enumvalue, cvid, intvalue;
	char *strvalue, *name;

	/*
	 * gabrielsan: reads the configuration from the database instead
	 *	of a file; now, the configuration variable arrays are dynamic.
	 *	In this way, it is a lot easier to extend it if needed.
	 */
	if ( (res = dbi_conn_queryf(dbiconn, "SELECT MAX(enumvalue) "
										 "FROM ConfigVariable WHERE VarType=0"))
		 && dbi_result_next_row(res) )
	{
		nstrs = dbi_result_get_int_idx(res,1)+1;
	}
	if(res) dbi_result_free(res);

	if ( (res = dbi_conn_queryf(dbiconn, "SELECT MAX(enumvalue) "
										 "FROM ConfigVariable WHERE VarType=1"))
		 && dbi_result_next_row(res) )
	{
		nints = dbi_result_get_int_idx(res,1)+1;
	}
	if(res) dbi_result_free(res);

	// samuelm:
	// if it's a reload, we need to free config.strs and config.ints

	//gabrielsan: need to free the strings also; possible leak
	if (config.strs != NULL) {
		for (i = 0; i < nstrs; i++)
			FREE(config.strs[i]);
		FREE(config.strs);
		FREE(config.ints);
	}

	config.strs = malloc(nstrs * sizeof(char*));
	config.ints = malloc(nints * sizeof(int));

	config.ints[default_time]		= DEFAULT_TIME;
	config.ints[default_rating]		= DEFAULT_RATING;
	config.ints[default_rd]			= DEFAULT_RD;
	config.ints[default_increment]	= DEFAULT_INCREMENT;
	config.ints[idle_timeout]		= DEFAULT_IDLE_TIMEOUT;
	config.ints[guest_prefix_only]	= DEFAULT_GUEST_PREFIX_ONLY;
	config.ints[login_timeout]		= DEFAULT_LOGIN_TIMEOUT;
	config.ints[max_aliases]		= DEFAULT_MAX_ALIASES;
	config.ints[max_players]		= DEFAULT_MAX_PLAYERS;
	config.ints[max_players_unreg]	= DEFAULT_MAX_PLAYERS_UNREG;
	config.ints[max_sought]			= DEFAULT_MAX_SOUGHT;
	config.ints[max_user_list_size]	= DEFAULT_MAX_USER_LIST_SIZE;

    for (i = 0; i < nstrs; i++)
		config.strs[i] = strdup("");

	res = dbi_conn_queryf(dbiconn, "SELECT * FROM ConfigVariable ORDER BY EnumValue");
	if (!res)
        return -1;

	d_printf("LOADING CONFIG VARIABLES:\n");
	if (!dbi_result_get_numrows(res)) {
		d_printf(_("warning: no records in ConfigVariable table"));
		// oops, could not read from base; fill it with defaults
		/* a few important defaults */
		config.strs[default_prompt] = strdup("chessd%");
		config.strs[guest_login] = strdup("guest");
		/* this allows an initial admin connection */
		config.strs[head_admin] = strdup("admin");
		dbi_result_free(res);
		return 0;
	}

	// read the values from database
	dbi_result_bind_fields(res, "vartype.%i enumvalue.%i configvariableid.%i "
								"strvalue.%s intvalue.%i name.%s",
						   &vartype, &enumvalue, &cvid, &strvalue, &intvalue, &name);
	while (dbi_result_next_row(res)) {
		if (vartype == 0) { // string variable
			// gabrielsan: make sure the "" string is freed
			FREE(config.strs[enumvalue]);
			config.strs[enumvalue] = strdup(strvalue);
			d_printf("  %s loaded (value \"%s\")\n", name, strvalue);
		}
		else if(vartype == 1) { // integer variable
			config.ints[enumvalue] = intvalue;
			d_printf("  %s loaded (value %d)\n", name, intvalue);
		}else
			d_printf("  warning: bad vartype, name=%s enumvalue=%d\n",
					 name, enumvalue);
	}
	dbi_result_free(res);

	return 0;
}

static void variable_reload(void)
{
	if (config_open() != 0) {
		d_printf(_("CHESSD: config database open failed\n"));
		exit(1);
	}

    help_init();
    commands_init();
	ratings_init();
	wild_init();
	book_open();
	init_userstat();
}

/* initialise variables that can be re-initialised on code reload */
void initial_load(void)
{
    InsertServerEvent( seSERVER_START, 0 );
    ClearServerActiveUsersAndGames();

	command_globals.startuptime = time(0);

	seek_globals.quota_time = 60;
	seekinit();
	command_globals.player_high = 0;
	command_globals.game_high = 0;
	srandom(command_globals.startuptime);

	// gabrielsan: just make sure it is NULL at the beginning
	config.ints = NULL;
	config.strs = NULL;

	variable_reload();
}

/* initialise variables that can be re-initialised on code reload */
void reload_open(void)
{
	// load_all_globals("globals.dat");
	variable_reload();
}

/* initialise variables that can be re-initialised on code reload */
void reload_close(void)
{
	book_close();
	lists_close();
	help_free();
	// save_all_globals("globals.dat");
	//m_free_all();
}
