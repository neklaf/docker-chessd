/*
   Copyright (c) 1993 Richard V. Nash.
   Copyright (c) 2000 Dan Papasian
   Copyright (c) Andrew Tridgell 2002
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
   Foundation, Inc., 675 Mass Ave, Cambridge, M 02139, USA.
*/

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_STRINGS_H
  #include <strings.h>  // for bcopy(), bzero()
#endif

#include "configuration.h"
#include "adminproc.h"
#include "chessd_locale.h"
#include "common.h"
#include "config_genstrc.h"
#include "eco.h"
#include "chessd_main.h"
#include "globals.h"
#include "help.h"
#include "iset.h"
#include "malloc.h"
#include "movecheck.h"
#include "seekproc.h"
#include "shutdown.h"
#include "timeseal.h"
#include "utils.h"
#include "crypt.h"
#include "protocol.h"
#include "setup.h"
#include "command_network.h"
#include "gameproc.h"
#include "talkproc.h"
#include "logkeeper.h"
#include "comproc.h"
#include "obsproc.h"
#include "ratings.h"
#include "variable.h"
#include "dbdbi.h"

/*
  Parameter string format
  w - a word
  o - an optional word
  d - integer
  p - optional integer
  i - word or integer
  n - optional word or integer
  s - string to end
  t - optional string to end

  If the parameter option is given in lower case then the parameter is
  converted to lower case before being passed to the function. If it is
  in upper case, then the parameter is passed as typed.
 */

/* Try to keep this list in alpha order */

 /* Name	Options	Functions	Security */
static struct command_type command_list[] = {

  {"abort",         "",     com_abort,      	ADMIN_USER, LOG_GAME },
  {"accept",		"n",	com_accept,			ADMIN_USER, LOG_GAME },
  {"addlist",       "ww",   com_addlist,    	ADMIN_USER, LOG_VAR  },
  {"adjourn",       "",     com_adjourn,    	ADMIN_USER, LOG_GAME },
//  {"admins",        "",     com_admins,    		ADMIN_USER, LOG_SRV  },
  {"alias",			"oT",	com_alias,			ADMIN_USER, LOG_SRV },
  {"allobservers",	"n",	com_allobservers,	ADMIN_USER, LOG_SRV  },
  {"assess",		"noo",	com_assess,			ADMIN_USER, LOG_SRV  },
  {"backward",      "p",    com_backward,   	ADMIN_USER, LOG_SRV  },
  {"bell",			"",		com_bell,			ADMIN_USER, LOG_SRV  },
  {"bname",	        "O",    com_bname,      	ADMIN_USER, LOG_GAME },
  {"boards",		"o",	com_boards,			ADMIN_USER, LOG_SRV  },
  {"bsetup",        "oT",  	com_setup,      	ADMIN_USER, LOG_SRV  },
  {"clrsquare",     "w",    com_clrsquare,  	ADMIN_USER, LOG_SRV  },
  {"cshout",        "S",    com_cshout,     	ADMIN_USER, LOG_TALK },
  {"date",			"",		com_date,			ADMIN_USER, LOG_SRV  },
  {"decline",		"n",	com_decline,		ADMIN_USER, LOG_GAME },
  {"draw",			"",		com_draw,			ADMIN_USER, LOG_GAME },
  {"eco",           "n",    com_eco,        	ADMIN_USER, LOG_SRV  },
  {"examine",       "on",   com_examine,    	ADMIN_USER, LOG_SRV  },
//  {"finger",		"oo",	com_stats,	ADMIN_USER },

  // finger will only show the URL of user info page
  {"finger",		"w",	com_finger,			ADMIN_USER, LOG_SRV },	//rem

  {"flag",			"",		com_flag,			ADMIN_USER, LOG_GAME },
  {"flip",			"",		com_flip,			ADMIN_USER, LOG_SRV  },
  {"forward",       "p",    com_forward,    	ADMIN_USER, LOG_SRV  },
  {"games",			"o",	com_games,			ADMIN_USER, LOG_SRV  },
  {"getgi",         "w",    com_getgi,      	ADMIN_USER, LOG_SRV  },
  {"getpi",         "w",    com_getpi,     		ADMIN_USER, LOG_SRV  },
  {"goboard",       "i",    com_goboard,   		ADMIN_USER, LOG_SRV  },
  {"help",			"t",	com_help,			ADMIN_USER, LOG_SRV  },
  {"inchannel",		"n",	com_inchannel,		ADMIN_USER, LOG_SRV  },
  {"iset",			"wS",	com_iset,			ADMIN_USER, LOG_SRV  },
  {"it", 			"T",	com_it,				ADMIN_USER, LOG_TALK },
  {"kibitz",		"S",	com_kibitz,			ADMIN_USER, LOG_TALK },
  {"limits",        "",     com_limits,     	ADMIN_USER, LOG_SRV  },
  {"llogons",       "",     com_llogons,    	ADMIN_USER, LOG_SRV  },
  {"logons",		"o",	com_logons,			ADMIN_USER, LOG_USER },
  {"match",			"wt",	com_match,			ADMIN_USER, LOG_USER },
  {"rmatch",		"wwt",	com_rmatch,			ADMIN_USER, LOG_SRV  },

  {"mexamine",      "w",    com_mexamine,   	ADMIN_USER, LOG_SRV  },
  {"moretime",      "d",    com_moretime,   	ADMIN_USER, LOG_GAME },
  {"moves",			"n",	com_moves,			ADMIN_USER, LOG_SRV  },
  {"next", 			"",		com_more, 			ADMIN_USER, LOG_SRV  },
  {"observe",		"n",	com_observe,		ADMIN_USER, LOG_GAME },
  {"open",			"",		com_open,			ADMIN_USER, LOG_SRV  },
  {"partner",		"o",	com_partner,		ADMIN_USER, LOG_USER },
  {"password",		"WW",	com_password,		ADMIN_USER, LOG_SRV  },
  {"pause",			"",		com_pause,			ADMIN_USER, LOG_GAME },
  {"play",			"i",	com_play,			ADMIN_USER, LOG_SRV  },
  {"pending",		"",		com_pending,		ADMIN_USER, LOG_SRV  },
  {"prefresh",		"",		com_prefresh,		ADMIN_USER, LOG_GAME },
  {"promote",		"w",	com_promote,		ADMIN_USER, LOG_GAME },
  {"ptell",			"S",	com_ptell,			ADMIN_USER, LOG_TALK },
  {"ptime",         "",     com_ptime,     		ADMIN_USER, LOG_GAME },
  {"qtell",         "iS",   com_qtell,      	ADMIN_USER, LOG_TALK },
  {"quit",			"",		com_quit,			ADMIN_USER, LOG_SRV  },
  {"refresh",		"n",	com_refresh,		ADMIN_USER, LOG_GAME },
  {"revert",        "",     com_revert,     	ADMIN_USER, LOG_GAME },
  {"resign",		"o",	com_resign,			ADMIN_USER, LOG_GAME },
  {"say",			"S",	com_say,			ADMIN_USER, LOG_TALK },
  {"seek",			"ooooooooo",	com_seek,	ADMIN_USER, LOG_SRV  },
  {"unseek",		"p",	com_unseek,			ADMIN_USER, LOG_SRV  },
  {"set",			"wT",	com_set,			ADMIN_USER, LOG_SRV  },
  {"shout",			"T",	com_shout,			ADMIN_USER, LOG_TALK },
  {"showlist",      "o",    com_showlist, 		ADMIN_USER, LOG_SRV  },
  {"simabort",		"",		com_simabort,		ADMIN_USER, LOG_GAME },
  {"simallabort",	"",		com_simallabort,	ADMIN_USER, LOG_GAME },
  {"simadjourn",	"",		com_simadjourn,		ADMIN_USER, LOG_GAME },
  {"simalladjourn",	"",		com_simalladjourn,	ADMIN_USER, LOG_GAME },
  {"simgames",		"o",	com_simgames,		ADMIN_USER, LOG_SRV  },
  {"simmatch",		"woo",	com_simmatch,		ADMIN_USER, LOG_GAME },
  {"simnext",		"",		com_simnext,		ADMIN_USER, LOG_GAME },
  {"simopen",		"",		com_simopen,		ADMIN_USER, LOG_SRV  },
  {"simpass",		"",		com_simpass,		ADMIN_USER, LOG_GAME },
  {"simprev",       "",     com_simprev,    	ADMIN_USER, LOG_GAME },
  {"sought",		"o",	com_sought,			ADMIN_USER, LOG_SRV  },
  {"style",			"d",	com_style,			ADMIN_USER, LOG_SRV  },
  {"sublist",       "ww",   com_sublist,    	ADMIN_USER, LOG_VAR  },
  {"switch",		"",		com_switch,			ADMIN_USER, LOG_GAME },
  {"takeback",		"p",	com_takeback,		ADMIN_USER, LOG_GAME },
  {"tell",			"nS",	com_tell,			ADMIN_USER, LOG_TALK },
  {"ltell",			"wS",	com_ltell,			ADMIN_USER, LOG_TALK },
  {"time",			"n",	com_time,			ADMIN_USER, LOG_GAME },
  {"tomove",        "w",    com_tomove,     	ADMIN_USER, LOG_SRV  },
  {"toggle",        "ww",   com_togglelist, 	ADMIN_USER, LOG_VAR  },
  {"tournset",      "wd",   com_tournset,   	ADMIN_USER, LOG_USER },
  {"unalias",		"w",	com_unalias,		ADMIN_USER, LOG_SRV  },
  {"unexamine",     "",     com_unexamine,  	ADMIN_USER, LOG_GAME },
  {"unobserve",		"n",	com_unobserve,		ADMIN_USER, LOG_GAME },
  {"unpause",		"",		com_unpause,		ADMIN_USER, LOG_GAME },
  {"uptime",		"",		com_uptime,			ADMIN_USER, LOG_SRV  },
  {"variables",		"o",	com_variables,		ADMIN_USER, LOG_SRV  },
  {"whisper",		"S",	com_whisper,		ADMIN_USER, LOG_TALK },
  {"who",           "T",    com_who,        	ADMIN_USER, LOG_SRV  },
  {"withdraw",		"n",	com_withdraw,		ADMIN_USER, LOG_GAME },
  {"wname",         "O",    com_wname,      	ADMIN_USER, LOG_GAME },
  {"xtell",         "wS",   com_xtell,      	ADMIN_USER, LOG_TALK },
  {"znotify",		"",		com_znotify,		ADMIN_USER, LOG_SRV  },

#if 0
  {"ustat",		"",	com_ustat,	ADMIN_USER },
#endif

#if 0
  {"reply",		"S",	com_reply,	ADMIN_USER },
#endif
//  {"pstat",		"oo",	com_pstat,	ADMIN_USER },

  {"ahelp",			"t",	com_ahelp,	   	ADMIN_ADMIN,  LOG_SRV 			},
  {"acheckhelp",	"",		com_acheckhelp,	ADMIN_ADMIN,  LOG_SRV 			},
  {"announce",		"S",	com_announce,   ADMIN_ADMIN,  LOG_TALK 			},
  {"annunreg",      "S",    com_annunreg,   ADMIN_ADMIN,  LOG_TALK 			},
  {"areload",       "",    	com_areload,    ADMIN_GOD  ,  ALOG_SRV_CMD 		},
  {"atsreload",		"",		com_atsreload,	ADMIN_ADMIN,  ALOG_SRV_CMD		},
  {"asetv",         "wS",   com_asetv,      ADMIN_ADMIN,  ALOG_SRV_CMD 		},
  {"asetadmin",     "wd",   com_asetadmin,  ADMIN_MASTER, ALOG_ON_USER		},
  {"chkip",         "w",    com_checkIP,    ADMIN_ADMIN,  ALOG_ON_USER		},
  {"ftell",         "o",    com_ftell,      ADMIN_ADMIN,  ALOG_ON_USER		},
  {"light",         "",     com_admin,      ADMIN_ADMIN,  LOG_SRV 			},
  {"nuke",          "w",    com_nuke,       ADMIN_HELPER, ALOG_ON_USER		},
  {"pose",			"wS",	com_pose,	   	ADMIN_DEMIGOD,ALOG_ON_USER		},
  {"quota",         "p",    com_quota,      ADMIN_ADMIN,  ALOG_SRV_CMD		},
  {"shutdown",      "oT",   com_shutdown,   ADMIN_GOD,	  ALOG_SRV_CMD 		},
//  {"getlocale",			"",		com_getlocale,		ADMIN_GOD},
//  {"setlocale",			"w",	com_setlocale,		ADMIN_GOD},
  {NULL, 			NULL, 	NULL, 			ADMIN_USER,	  ALOG_NOTHING	}
};

static struct alias_type g_alias_list[] = {
  {"comment",   "addcomment"},
  {"setup",		"bsetup"},
  {"sc",        "showcomment"},
  {"b",			"backward"},
  {"w",			"who"},
  {"t",			"tell"},
  {"m",			"match"},
  {"go",        "goboard"},
  {"goto",      "goboard"},
  {"gonum",     "goboard"}, /* dumping an obsolete command */
  {"f",			"finger"},
  {"hi",        "history"},
  {"a",			"accept"},
  {"saa",       "simallabort"},
  {"saab",		"simallaabort"},
  {"sab",       "simabort"},
  {"sadj",      "simadjourn"},
  {"saadj",     "simalladjourn"},
  {"sh",		"shout"},
  {"sn",		"simnext"},
  {"sp",        "simprev"},
  {"vars",		"variables"},
  {"g",			"games"},
  {"players",	"who a"},
  {"player",   	"who a"},
  {"p",			"who a"},
  {"pl",		"who a"},
  {"o",			"observe"},
  {"r",			"refresh"},
  {"re",        "refresh"}, /* So r/re doesn't resign! */
  {"ch",		"channel"},
  {"in",		"inchannel"},
  {".",			"tell ."},
  {",",			"tell ,"},
  {"`",         "tell ."},
  {"!",			"shout"},
  {"I",			"it"},
  {"i",			"it"},
  {":",			"it"},
  {"exit",		"quit"},
  {"logout",	"quit"},
  {"bye",       "quit"},
  {"*",      	"kibitz"},
  {"#",         "whisper"},
  {"ma",		"match"},
  {"more",      "next"},
  {"n",         "next"},
  {"znotl",     "znotify"},
  {"+",         "addlist"},
  {"-",         "sublist"},
  {"=",         "showlist"},
  {"mam",     	"tell mamer"},
  {"tom",    	"tell tomato"},
  {"egg",     	"tell eggoi"},
  {"follow",  	"toggle follow"},
  {"channel", 	"toggle channel"},
  {NULL, NULL}
};

static int lastCommandFound = -1;

static char *guest_name(void);
static char *abuser_name(void);

/* Copies command into comm, and returns pointer to parameters in
 * parameters
 */
static int parse_command(char *com_string, char **comm, char **parameters)
{
	*comm = com_string;
	*parameters = eatword(com_string);
	if (**parameters != '\0') {
		**parameters = '\0';
		(*parameters)++;
		*parameters = eatwhite(*parameters);
	}
	if (strlen(*comm) >= MAX_COM_LENGTH)
		return COM_BADCOMMAND;
	return COM_OK;
}

/* numalias is the maximum number to search through */
int alias_lookup(char *tmp, struct alias_type *alias_list, int numalias)
{
	int i;

	for (i = 0; i < numalias && alias_list[i].comm_name; i++) {
		if (!strcasecmp(tmp, alias_list[i].comm_name))
			return i;
	}
	return -1;  /* not found */
}

/* Puts alias substitution into alias_string */
static void alias_substitute(struct alias_type *alias_list, int num_alias,
			     char *com_str, char outalias[])
{
	char *name, *atpos;
	int i, n, at_offset=0;

	/* handle punctuation commands like '+' */
	if (ispunct(*com_str)) {
		for (n = 0; ispunct(com_str[n]); n++)
			;
		name = strndup(com_str, n);
	} else
		name = strndup(com_str, strcspn(com_str, " \t"));

	com_str += strlen(name);

	while (isspace(*com_str)) com_str++;

	i = alias_lookup(name, alias_list, num_alias);

	if (i >= 0) {
		FREE(name);
		name = strdup(alias_list[i].alias);
	}
	if (i < 0 || !strcmp(name, "@")) {
		if (!strcmp(name, "@"))
			sprintf(outalias,"%s", com_str);
		else
			if (*com_str)
	        	sprintf(outalias, "%s %s", name, com_str);
			else
				sprintf(outalias, "%s", name);
		free(name);
		return;
	}
	/* now substitute '@' (com_str) values in name  */
	while (name && (atpos = strchr(name+at_offset, '@'))) {
		char *name2 = NULL;
        int size = strlen(name) - strlen(atpos);
		asprintf(&name2, "%*.*s%s%s", size, size, name, com_str, atpos+1);
		/* try to prevent loops */
		at_offset = (atpos - name) + strlen(com_str);

		free(name);
		name = name2;
	}

	/* there is an implicit @ after each alias */
	if (at_offset == 0 && *com_str)
	  sprintf(outalias, "%s %s", name, com_str);
	else
	  strcpy(outalias, name);

	free(name);
}

/* Returns pointer to command that matches */
static int match_command(char *comm, int p)
{
	int i = 0;
	int gotIt = -1;
	int len = strlen(comm);

	while (command_list[i].comm_name) {
		if (strncmp(command_list[i].comm_name, comm, len) == 0 &&
		    check_admin(p, command_list[i].adminLevel)) {
			if (gotIt >= 0)
				return -COM_AMBIGUOUS;
			gotIt = i;
		}
		i++;
	}

	if (gotIt == -1) {
		return -COM_FAILED;
	}

	if (in_list(p, L_REMOVEDCOM, command_list[gotIt].comm_name)) {
		pprintf(p, _("Due to a bug - this command has been temporarily removed.\n"));
		return -COM_FAILED;
	}
	lastCommandFound = gotIt;
	return gotIt;
}

/* Gets the parameters for this command */
static int get_parameters(int command, char *parameters, param_list params)
{
	int i, parlen, paramLower;
	char c;
	static char punc[2];

	punc[1] = '\0';		/* Holds punc parameters */
	for (i = 0; i < MAXNUMPARAMS; i++) {
		(params)[i].type = TYPE_NULL;	/* Set all parameters to NULL */
		(params)[i].val.string = 0;	        /* Set union val to NULL */
	}
	parlen = strlen(command_list[command].param_string);
	for (i = 0; i < parlen; i++) {
		c = command_list[command].param_string[i];
		if (isupper(c)) {
			paramLower = 0;
			c = tolower(c);
		} else {
			paramLower = 1;
		}
		switch (c) {
			case 'w':
			case 'o':			/* word or optional word */
				parameters = eatwhite(parameters);
				if (!*parameters)
					return c == 'o' ? COM_OK : COM_BADPARAMETERS;
				(params)[i].val.word = parameters;
				(params)[i].type = TYPE_WORD;
				if (ispunct(*parameters)) {
					punc[0] = *parameters;
					(params)[i].val.word = punc;
					parameters++;
					if (*parameters && isspace(*parameters))
						parameters++;
				} else {
					parameters = eatword(parameters);
					if (*parameters != '\0') {
						*parameters = '\0';
						parameters++;
					}
				}
				if (paramLower)
					stolower((params)[i].val.word);
				break;

			case 'd':
			case 'p':			/* optional or required integer */
				parameters = eatwhite(parameters);
				if (!*parameters)
					return (c == 'p' ? COM_OK : COM_BADPARAMETERS);
				if (sscanf(parameters, "%d", &(params)[i].val.integer) != 1)
					return COM_BADPARAMETERS;
				(params)[i].type = TYPE_INT;
				parameters = eatword(parameters);
				if (*parameters != '\0') {
					*parameters = '\0';
					parameters++;
				}
				break;

			case 'i':
			case 'n':			/* optional or required word or integer */
				parameters = eatwhite(parameters);
				if (!*parameters)
					return (c == 'n' ? COM_OK : COM_BADPARAMETERS);
				if (sscanf(parameters, "%d", &(params)[i].val.integer) != 1) {
					(params)[i].val.word = parameters;
					(params)[i].type = TYPE_WORD;
				} else {
					(params)[i].type = TYPE_INT;
				}
				if (ispunct(*parameters)) {
					punc[0] = *parameters;
					(params)[i].val.word = punc;
					(params)[i].type = TYPE_WORD;
					parameters++;
					if (*parameters && isspace(*parameters))
						parameters++;
				} else {
					parameters = eatword(parameters);
					if (*parameters != '\0') {
						*parameters = '\0';
						parameters++;
					}
				}
				if ((params)[i].type == TYPE_WORD)
					if (paramLower)
						stolower((params)[i].val.word);
				break;

			case 's':
			case 't':			/* optional or required string to end */
				if (!*parameters)
					return (c == 't' ? COM_OK : COM_BADPARAMETERS);
				(params)[i].val.string = parameters;
				(params)[i].type = TYPE_STRING;
				while (*parameters)
					parameters = nextword(parameters);
				if (paramLower)
					stolower((params)[i].val.string);
				break;
		}
	}

	return *parameters ? COM_BADPARAMETERS : COM_OK;
}

static void printusage(int p, char *command_str)
{
	int i, parlen, command;
	char c, *usage_dir = NULL, *filenames[1000]; /* enough for all usage names */

	if ((command = match_command(command_str, p)) < 0) {
		pprintf(p, _("  UNKNOWN COMMAND\n"));
		return;
	}

	/* First let's check if we have a text usage file for it */

	/* current locale */
	i = search_directory(locale_usage_dir, command_str, filenames, 1000);
	if (i > 0) {
		usage_dir = malloc(sizeof(USAGE_DIR) + sizeof(LOCALE_MESSAGES) + 1);
		strcpy(usage_dir, USAGE_DIR);
		strcat(usage_dir, LOCALE_MESSAGES);
	}
	/* nothing, but maybe there is something on the "C" default locale */
	else if ((i == 0) && (strcpy(LOCALE_MESSAGES, "C") != 0)) {
		i += search_directory(USAGE_DIR "/C", command_str, filenames, 1000);
		if (i > 0) {
			pprintf(p, _("No usage available for %s locale; using default (C).\n"),
					LOCALE_MESSAGES);
			usage_dir = malloc(sizeof(USAGE_DIR) + 3);
			strcpy(usage_dir, USAGE_DIR);
			strcat(usage_dir, "/C");
		}
	}

	if (i != 0) {
		if (i == 1 || !strcmp(*filenames, command_str)) { /* found it? then send */
			if (psend_file(p, usage_dir, *filenames)) {
				/* we should never reach this unless the file was just deleted */
				pprintf(p, _("Usage file %s could not be found! "
							 "Please inform an admin of this. Thank you.\n"),
						*filenames);
				/* no need to print 'system' usage - should never happen */
			}
			pprintf(p, _("\nSee '%s %s' for a complete description.\n"),
					((command_list[lastCommandFound].adminLevel > ADMIN_USER) ? "ahelp" :
					 "help"),
					command_list[lastCommandFound].comm_name);

			// gabrielsan: possible leak source fixed
			FREE(usage_dir);
			return;
		}
	}
	// gabrielsan: possible leak source fixed
	FREE(usage_dir);

	/* print the default 'system' usage files (which aren't much help!) */

	pprintf(p, _("Usage: %s"), command_list[lastCommandFound].comm_name);

	parlen = strlen(command_list[command].param_string);
	for (i = 0; i < parlen; i++) {
		c = command_list[command].param_string[i];
		switch (tolower(c)) {
			case 'w':			/* word */
				pprintf(p, _(" word"));
				break;
			case 'o':			/* optional word */
				pprintf(p, _(" [word]"));
				break;
			case 'd':			/* integer */
				pprintf(p, _(" integer"));
				break;
			case 'p':			/* optional integer */
				pprintf(p, _(" [integer]"));
				break;
			case 'i':			/* word or integer */
				pprintf(p, _(" {word, integer}"));
				break;
			case 'n':			/* optional word or integer */
				pprintf(p, _(" [{word, integer}]"));
				break;
			case 's':			/* string to end */
				pprintf(p, _(" string"));
				break;
			case 't':			/* optional string to end */
				pprintf(p, _(" [string]"));
				break;
		}
	}
	pprintf(p, _("\nSee '%s %s' for a complete description.\n"),
		command_list[lastCommandFound].adminLevel > ADMIN_USER ? "ahelp" : "help",
		command_list[lastCommandFound].comm_name);
	return;
}

static int one_command(int p, char *command, char *cmd)
{
	struct player *pp = &player_globals.parray[p];
	int which_command, retval;
	char *comm=NULL, *parameters=NULL;
	param_list params;

#ifdef NEW_LOG_ENABLED
	char *raw_command, *targetName, *logParameters[MAXNUMPARAMS];
	int logParCount, i, logType;
#endif

	if ((retval = parse_command(command, &comm, &parameters)))
		return retval;

	strcpy(cmd, comm);
	if (pp->game >= 0 && game_globals.garray[pp->game].status == GAME_SETUP
	 && is_drop(comm))
		return COM_ISMOVE;

	if (is_move(comm)) {
		if (pp->game == -1) {
			pprintf(p, _("You are not playing or examining a game.\n"));
			return COM_OK;
		}

		if (game_globals.garray[pp->game].status == GAME_SETUP)
			return COM_ISMOVE_INSETUP;
		else
			return COM_ISMOVE;
	}

	stolower(comm);		/* All commands are case-insensitive */
	stolower(cmd);

#ifndef NEW_LOG_ENABLED
	if ((which_command = match_command(comm, p)) < 0)
		return -which_command;

	if (!check_admin(p, command_list[which_command].adminLevel))
		return COM_RIGHTS;

	if ((retval = get_parameters(which_command, parameters, params)))
		return retval;

	return command_list[which_command].comm_func(p, params);
#else
	// USING DB LOG
	//
	// check for errors and fill the result for the log
	if ((which_command = match_command(comm, p)) < 0) {
		// error: command not found
		retval = -which_command;
		which_command = -1;

		logType = LOG_CMD_ERROR;
	}
	else
		logType = command_list[which_command].logType;

	if (!retval && !check_admin(p, command_list[which_command].adminLevel))
		retval = COM_RIGHTS;

	if (!retval)
		retval = get_parameters(which_command, parameters, params);

	if (!retval) {
		pp->last_cmd_error = CMD_OK;
		retval = command_list[which_command].comm_func(p, params);
	}

	bzero(logParameters, sizeof(char*) * MAXNUMPARAMS);

	logParCount =
		fillLogPar(
				which_command == -1 		// command information
					? NULL							// command does not exist
					: &command_list[which_command],	// pass the cmd def
				pp,							// the player
				raw_command,				// the full typed line
				comm,						// processed command
				retval,						// result
				params,						// the parsed parameters
				logParameters,				// results go here
				&targetName);				// the target is stored here

	log_event(logType,
	 		  pp->chessduserid,
			  pp->name,
			  inet_ntoa(pp->thisHost),
			  targetName,
			  logParameters,
			  logParCount);

	// free the strings
	for (i=0; i < logParCount; i++)
		FREE(logParameters[i]);
	FREE(targetName);
	return retval;
#endif
}

static int process_command(int p, char *com_string, char *cmd)
{
	struct player *pp = &player_globals.parray[p];
	char *tok, *ptr = NULL;
	char astring1[MAX_STRING_LENGTH * 4];
	char astring2[MAX_STRING_LENGTH * 4];
	int status;

#ifdef DEBUG
	if (strcasecmp(pp->name, pp->login)) {
		d_printf( _("CHESSD: PROBLEM Name=%s, Login=%s\n"),
			pp->name, pp->login);
	}
#endif
	if (!com_string)
		return COM_FAILED;

	log_printf( "IP %s :%s, %s, %d: >%s<\n", inet_ntoa(pp->thisHost),
				pp->name, pp->login, pp->socket, com_string );

	/* don't expand the alias command */
	if (strncmp(com_string, "alias ", 6) == 0) {
#ifdef DEBUG
	// mark the time before executing the command
		toggle_clock();
#endif
		status = one_command(p, com_string, cmd);
#ifdef DEBUG
	// compute time after the command was executed
		d_printf(_("Time spent in command: %lf s\n"), toggle_clock());
#endif
		return status;
	}

	alias_substitute(pp->alias_list, pp->numAlias, com_string, astring1);
	alias_substitute(g_alias_list, 999, astring1, astring2);
#ifdef DEBUG
	if (strcmp(com_string, astring2) != 0) {
		log_printf( "%s -alias-: >%s< >%s<\n",
			pp->name, com_string, astring2);
	}
#endif

	for (tok=strtok_r(astring2, ";", &ptr); tok;
		 tok=strtok_r(NULL, ";", &ptr))
	{
		char alias_string1[MAX_STRING_LENGTH * 4];
		char alias_string2[MAX_STRING_LENGTH * 4];
		int retval;

		while (*tok && isspace(*tok))
			tok++;

		// better check if it is not at the end of the string
		if (*tok) {
			alias_substitute(pp->alias_list, pp->numAlias, tok, alias_string1);
			alias_substitute(g_alias_list, 999, alias_string1, alias_string2);

#ifdef DEBUG
			if (alias_string2 && strcmp(tok, alias_string2) != 0) {
				log_printf( "%s -alias2-: >%s<\n", pp->name, alias_string2);
			}
#endif
#ifdef DEBUG
			// mark the time before executing the command
			toggle_clock();
#endif
			retval = one_command(p, alias_string2, cmd);
#ifdef DEBUG
				// compute time after the command was executed
			d_printf(_("Time spent in command: %lf s\n"), toggle_clock());
#endif
				/* stop on first error */
			if (retval != COM_OK)
				return retval;
		}
	}
	return COM_OK;
}

static int process_login(int p, char *loginname)
{
	struct player *pp = &player_globals.parray[p];
	char loginnameii[80];
	int is_guest = 0;

	loginname = eatwhite(loginname);

	if (!*loginname)
		goto new_login;

	/* if 'guest' was specified then pick a random guest name */
	if (strcasecmp(loginname, config.strs[guest_login]) == 0) {
		loginname = guest_name();
		is_guest = 1;
		pprintf(p, _("\nCreated temporary login '%s'\n"), loginname);
	}

	strlcpy(loginnameii, loginname, sizeof(loginnameii));

	/*	test whether the login is valid, characterwise */
	if (!alphastring(loginnameii)) {
		// only letters are allowed
		pprintf(p, _("\nSorry, names can only consist of letters, digits or"
					 " underscores.  Try again.\n"));
		goto new_login;
	}
	if (strlen(loginnameii) < 3) {
		// at least 3 chars long
		pprintf(p, _("\nA name should be at least three characters long!"
					 "  Try again.\n"));
		goto new_login;
	}

	if (strlen(loginnameii) > (MAX_LOGIN_NAME - 1)) {
		// at most MAX_LOGIN_NAME chars long
		pprintf(p, _("\nSorry, names may be at most %d characters long.  "
					 "Try again.\n"), MAX_LOGIN_NAME - 1);
		goto new_login;
	}

	if (in_list(p, L_BAN, loginnameii)) {
		/* player is banned */
		d_printf(_("rejected banned login %s"), loginnameii);
		pprintf(p, _("\nPlayer \"%s\" is banned.\n"), loginnameii);
		return COM_LOGOUT;
	}

	if (!in_list(p, L_ADMIN, loginnameii) &&
		player_count(0) >= config.ints[max_players]) {
		/* the server is full */
		psend_raw_file(p, MESS_DIR, MESS_FULL);
		return COM_LOGOUT;
	}

	// this may be a good moment to check database connection and reconnect.
	sql_check_reconnect();

	if (player_read(p, loginnameii) != 0) {
		if (!is_guest && config.ints[guest_prefix_only])
			goto new_login;

		/* at this point, the player is a guest */
		strcpy(pp->name, loginnameii);
		if (in_list(p, L_FILTER, dotQuad(pp->thisHost))) {
			d_printf(_("rejected login %s from %s (site is abuse filtered)"),
					 loginnameii, dotQuad(pp->thisHost));
			pprintf(p, _("\nDue to abusive behavior, nobody from your site may login.\n"
						 "If you wish to use this server please email %s\n"
						 "Include details of a nick-name to be called here, e-mail address "
						 "and your real name.\nWe will send a password to you. Thanks.\n"),
					config.strs[registration_address]);
			return COM_LOGOUT;
		}

		if (player_count(0) >= (config.ints[max_players_unreg])) {
			/* server full */
			psend_raw_file(p, MESS_DIR, MESS_FULL_UNREG);
			return COM_LOGOUT;
		}

		/* xboard */
		pprintf_noformat(p, U_("\n\"%s\" is not a registered name.  "),
						 loginnameii);
		pprintf_noformat(p, _("You may play unrated games as a guest.\n"
							  "(After logging in, do 'help register' "
							  "for more info on how to register.)\n\n"
							  "Press return to enter the chessd as \"%s\": "),
						 loginnameii);
		pp->status = PLAYER_PASSWORD;

		// if the user is a "name abuser", his/her name should be erased here
		if (in_list(p, L_NAME_ABUSER, pp->login)) {
			// ok, we won't take bullsh* from guests
			d_printf(_("abusive name rejected"));
			pprintf(p, _("The name you choose in considered \n"
						 "abusive in this server. If you want to log \n"
						 "in, choose another nickname" ));
			return COM_LOGOUT;
		}

		// gabrielsan: (21/01/2004)
		// just make sure the userid is -1 for guests
		pp->chessduserid = -1;
		turn_echo_off(pp->socket);
		return COM_OK;
	}

	/* handle registered players */

	/* xboard */
	pprintf_noformat(p, U_("\n\"%s\" is a registered name.  "), pp->name);
	pprintf_noformat(p, _("If it is yours, type the password.\n"
						  "If not, just hit return to try another name.\n\n"
						  "password: "));
	pp->status = PLAYER_PASSWORD;
	turn_echo_off(pp->socket);

	// check whether the player information is corrupt
	if (strcasecmp(loginnameii, pp->name)) {
		d_printf(_("mismatching name field for %s/%s\n"),
				 loginnameii, pp->name);
		pprintf(p, _("\nYou've got a bad name field in your playerfile -- "
					 "please report this to an admin!\n"));
		return COM_LOGOUT;
	}

	if (CheckPFlag(p, PFLAG_REG)) {
		if (pp->fullName == NULL) {
			d_printf(_("NULL fullname for %s\n"), loginnameii);
			pprintf(p, _("\nYou've got a bad playerfile -- please report "
						 "this to an admin!\nYour FullName is missing.\n"
						 "Please log on as an unreg until an admin can correct this.\n"));
			return COM_LOGOUT;
		}
		if (pp->emailAddress == NULL) {
			d_printf(_("NULL emailaddress for %s\n"), loginnameii);
			pprintf(p, _("\nYou've got a bad playerfile -- please report "
						 "this to an admin!\nYour Email address is missing.\n"
						 "Please log on as an unreg until an admin can correct this.\n"));
			return COM_LOGOUT;
		}
	}

	// if the user is a "name abuser", his/her name should be erased here
	if (in_list(p, L_NAME_ABUSER, pp->login))
		pp->name = strdup(abuser_name());

	return COM_OK;

new_login:
	/* give them a new prompt */
	psend_raw_file(p, MESS_DIR, MESS_LOGIN);
	pprintf(p, U_("login: ")); /* xboard */
	return COM_OK;
}

static void boot_out(int p, int p1)
{
	struct player *pp = &player_globals.parray[p];
	int fd;

	pprintf(p, _("\n **** %s is already logged in - kicking them out. ****\n"), pp->name);
	pprintf(p1, _("**** %s has arrived - you can't both be logged in. ****\n"), pp->name);
	fd = player_globals.parray[p1].socket;
	process_disconnection(fd);
	net_close_connection(fd);
}

static int process_password(int p, char *password)
{
	struct player *pp = &player_globals.parray[p];
	struct in_addr fromHost;
	static int Current_ad;
	int p1, fd, nmsg;
	char fname[10];

	turn_echo_on(pp->socket);

	if (pp->passwd && CheckPFlag(p, PFLAG_REG)) {
		if (strlen(pp->passwd) <= 4) {
			// gabrielsan: the stored password may be smaller than 5 and
			// this should not happen.
			// force an error
			password[0] = '\0';
			pprintf(p, _("\n\n**** Corrupted password! Connect as guest "
						 "and inform an admin! ****\n\n"));
			d_printf(_("password too short (<=4 chars) for %s\n"), pp->name);
		}

		if (!matchpass(pp->passwd, password))
		{
			// password did not match
			fd = pp->socket;
			fromHost = pp->thisHost;
			if (*password) {
				pprintf(p, _("\n\n**** Invalid password! ****\n\n"));
				d_printf(_("FICS (process_password): Bad password for %s [%s] [%s]\n"),
						 pp->login, password, pp->passwd);
			}
			player_clear(p);
			pp->logon_time = pp->last_command_time = time(0);
			pp->status = PLAYER_LOGIN;
			pp->socket = fd;
			if (fd >= net_globals.no_file)
				d_printf(_("FICS (process_password): Out of range fd!\n"));

			pp->thisHost = fromHost;

			psend_raw_file(p, MESS_DIR, MESS_LOGIN);
			pprintf(p, U_("login: ")); /* xboard */
			return COM_OK;
		}
	}

	// check whether user registration is under evaluation
	if (CheckPFlag(p, PFLAG_UNDER_EVAL)) {
		pprintf(p, _("\nYour registration is under evaluation. Until approved,"
					 "\nyou can't play rated games and can only talk in channel 1.\n"));
		// make the user a 'guest'
		TogglePFlag(p, PFLAG_REG);
	}

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if ( player_globals.parray[p1].name != NULL
		  && !strcasecmp(pp->name, player_globals.parray[p1].name)
		  && p != p1 )
		{
			if (!CheckPFlag(p, PFLAG_REG)) {
				pprintf(p, _("\n*** Sorry %s is already logged in ***\n"), pp->name);
				return COM_LOGOUT;
			}
			boot_out(p, p1);
		}
	}

	if (player_ishead(p)) {
		pprintf(p,_("\n  ** LOGGED IN AS HEAD ADMIN **\n"));
		pp->adminLevel = ADMIN_GOD;
	}

	// choose which 'motd' file to send to this user
	pprintf(p, "\n");
	psend_raw_file(p, MESS_DIR, pp->adminLevel > 0 ? MESS_ADMOTD : MESS_MOTD);

	if (!CheckPFlag(p, PFLAG_REG)) {
		// guests
		psend_raw_file(p, MESS_DIR, MESS_UNREGISTERED);

		if (MAX_ADVERTS >= 0) {
			pprintf(p, "\n");
			sprintf(fname, "%d", Current_ad);
			Current_ad = (Current_ad + 1) % MAX_ADVERTS;
			psend_raw_file(p, ADVERT_DIR, fname);
		}
	}
	else if (!pp->passwd || !pp->passwd[0])
		pprintf(p, _("\n*** You have no password. Please set one with the 'password' command."));

	pp->status = PLAYER_PROMPT;

	// log his login in the database
	player_write_login(p);

	if( (nmsg = player_count_messages(p)) )
		pprintf(p, ngettext("\nYou have %d unread message in the site.\n",
							"\nYou have %d unread messages in the site.\n", nmsg), nmsg);

	// arrival notifications
	player_notify_pin(p);
	player_notify_present(p);
	player_notify(p, _("arrived"), _("arrival"));
	showstored(p);

	// tells the user which was his last login
	if (CheckPFlag(p, PFLAG_REG)
	&& pp->lastHost.s_addr != 0 && pp->lastHost.s_addr != pp->thisHost.s_addr)
	{
		pprintf(p, _("\nPlayer %s: Last login: %s "), pp->name,
				dotQuad(pp->lastHost));
		pprintf(p, _("This login: %s"), dotQuad(pp->thisHost));
	}

	pp->lastHost = pp->thisHost;
	pp->logon_time = pp->last_command_time = time(0);
	if (CheckPFlag(p, PFLAG_REG) && !pp->timeOfReg)
		pp->timeOfReg = pp->last_command_time;

	check_and_print_shutdown(p);

	if (CheckPFlag(p, PFLAG_REG))
		announce_avail(p);
	else {
		char tmp[10];
		sprintf(tmp, "%d", HELP_CHANNEL);
		if (!in_list(p, L_CHANNEL, tmp)) // every guest should be in channel 1
			pcommand(p, "+channel 1");
	}

	pprintf_prompt(p, _("\nLogged in as: %s\n"), pp->name);

	return 0;
}

static int process_prompt(int p, char *command)
{
	struct player *pp = &player_globals.parray[p];
	int retval;
	static char cmd[MAX_COM_LENGTH];
	static int level = 0;

	command = eattailwhite(eatwhite(command));
	if (!*command) {
		send_prompt(p);
		return COM_OK;
	}
	retval = process_command(p, command, cmd);
	switch (retval) {
		case COM_OK:
			retval = COM_OK;
			send_prompt(p);
			break;
		case COM_OK_NOPROMPT:
			retval = COM_OK;
			break;
		case COM_ISMOVE:
			retval = COM_OK;

			// just make sure this is indeed a valid game number
			if (is_valid_and_active(pp->game)
				&& pp->side == game_globals.garray[pp->game].game_state.onMove
				&& game_globals.garray[pp->game].flag_pending != FLAG_NONE)
			{	// executes a flag
				ExecuteFlagCmd(p, net_globals.con[pp->socket]);
			}
			else {
				if (!is_valid_and_active(pp->game))
					d_printf("CHESSD_ERROR: attempt to move in invalid game\n");
			}
			process_move(p, cmd);
			send_prompt(p);
			break;
		case COM_ISMOVE_INSETUP:
			pprintf(p, _("You are still setting up the position.\n"));
			pprintf_prompt(p, _("Type: 'setup done' when you are finished editing.\n"));
			retval = COM_OK;
			break;
		case COM_RIGHTS:
			pprintf_prompt(p, _("%s: Insufficient rights.\n"), cmd);
			retval = COM_OK;
			break;
		case COM_AMBIGUOUS:
			/*    pprintf(p, "%s: Ambiguous command.\n", cmd); */
			{
				int len = strlen(cmd), i;
				pprintf(p, _("Ambiguous command. Matches:"));
				for (i = 0; command_list[i].comm_name; ++i) {
					if (!strncmp(command_list[i].comm_name, cmd, len)
					 && check_admin(p, command_list[i].adminLevel))
						pprintf(p, " %s", command_list[i].comm_name);
				}
			}
			pprintf_prompt(p, "\n");
			retval = COM_OK;
			break;
		case COM_BADPARAMETERS:
			printusage(p, command_list[lastCommandFound].comm_name);
			send_prompt(p);
			retval = COM_OK;
			break;
		case COM_FAILED:
		case COM_BADCOMMAND:
			if (level == 0 && CheckPFlag(p, PFLAG_HELPME)) {
				char ncmd[2048];
				sprintf(ncmd, "t 1 [help] %s", cmd);
				level++;
				process_prompt(p, ncmd);
				level--;
			}
			else {
				pprintf_prompt(p, _("%s: Command not found.\n"), cmd);
				d_printf(_("Command not found [%s]\n"), cmd);
			}
			retval = COM_OK;
			break;
		case COM_LOGOUT:
			retval = COM_LOGOUT;
			break;
	}

	return retval;
}

int process_net_notification(int fd, int error_type)
{
	// Some errors in the network module may indicate an attempt
	// to abuse the server; this module will handle it

	if (error_type == -2) {// input too long
		// might be an attempt to break timeseal;
		d_printf("process_net_notification(): input too long, abuse?");
	}
	return COM_OK;
}

int process_invalid_input(int fd, char *com_string)
{
	int p = player_find(fd);
	struct player *pp;
	pp = &player_globals.parray[p];

	switch (pp->status) {
	case PLAYER_PROMPT:
		pprintf(p, _("You are being nuked. Reason: sending invalid text.\n"));
		return COM_LOGOUT;
	default:
		pprintf(p, _("Invalid Input\n"));
	}

	return COM_OK;
}

/* Return 1 to disconnect */
int process_input(int fd, char *com_string)
{
	int p = player_find(fd);
	int retval = 0;
	struct player *pp;

	if (p < 0) {
		d_printf(_("CHESSD: Input from a player not in array!\n"));
		return -1;
	}

	pp = &player_globals.parray[p];

	command_globals.commanding_player = p;
	pp->totalTime += time(0) - pp->last_command_time;
	pp->last_command_time = time(0);

	switch (pp->status) {
	case PLAYER_EMPTY:
		d_printf(_("CHESSD: Command from an empty player!\n"));
		break;
	case PLAYER_NEW:
		d_printf(_("CHESSD: Command from a new player!\n"));
		break;
	case PLAYER_INQUEUE:
		/* Ignore input from player in queue */
		break;
	case PLAYER_LOGIN:
		retval = process_login(p, com_string);
		if (retval == COM_LOGOUT && com_string != NULL)
			d_printf(_("%s tried to log in and failed.\n"), com_string);
		break;
	case PLAYER_PASSWORD:
		retval = process_password(p, com_string);
		break;
	case PLAYER_PROMPT:
		FREE(pp->busy);
		retval = process_prompt(p, com_string);
		break;
	}

	command_globals.commanding_player = -1;
	return retval;
}

int process_new_connection(int fd, struct in_addr fromHost)
{
	struct player *pp;
	int p = player_new();

	pp = &player_globals.parray[p];
	pp->status = PLAYER_LOGIN;
	if (fd >= net_globals.no_file)
		d_printf(_("FICS (process_new_connection): Out of range fd!\n"));

	pp->socket = fd;
	pp->thisHost = fromHost;
	pp->logon_time = time(0);

	psend_raw_file(p, MESS_DIR, MESS_WELCOME);
	if(config.strs[head_admin][0])
		pprintf(p, _("Head admin: %s"), config.strs[head_admin]);
	if(config.strs[head_admin_email][0])
		pprintf(p, _("    Complaints to: %s\n"), config.strs[head_admin_email]);
	if(config.strs[server_location][0])
		pprintf(p, _("Server location: %s"), config.strs[server_location]);
	if(config.strs[server_hostname][0])
		pprintf(p, _("Server name: %s\n"), config.strs[server_hostname]);
	psend_raw_file(p, MESS_DIR, MESS_LOGIN);
	pprintf(p, U_("login: ")); /* xboard */

	return 0;
}

int process_disconnection(int fd)
{
	int p = player_find(fd);
	int p1;
	char command[1024];
	struct player *pp;

	if (p < 0) {
		d_printf( _("CHESSD: Disconnect from a player not in array!\n"));
		return -1;
	}

	pp = &player_globals.parray[p];

	if (CheckPFlag(p, PFLAG_REG) && CheckPFlag(p, PFLAG_OPEN) && pp->game < 0)
		announce_notavail(p);
	if ( pp->game >=0 && pp->game < game_globals.g_num &&
			(game_globals.garray[pp->game].status == GAME_EXAMINE ||
			 game_globals.garray[pp->game].status == GAME_SETUP) )
		pcommand(p, "unexamine");
	if ( pp->game >=0 && (in_list(p, L_ABUSER, pp->name)
				|| game_globals.garray[pp->game].link >= 0) )
		pcommand(p, "resign");

	if (pp->ftell != -1)
		pcommand(p, _("tell 0 I am logging out now - conversation forwarding stopped."));

	//  withdraw_seeks(p);
	seekremoveall(p);

	if (pp->status == PLAYER_PROMPT) {
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT)
				continue;

			if (player_globals.parray[p1].ftell == p) {
				player_globals.parray[p1].ftell = -1;
				sprintf(command,
						_("tell 0 *%s* has logged out - conversation forwarding stopped."),
						pp->name);

				pcommand(p1,command);
				pprintf_prompt(p1,_("%s, whose tells you were forwarding, has logged out.\n"),
						pp->name);
			}

			if (CheckPFlag(p1, PFLAG_PIN))
			{
				protocol_user_online_status(p1,
						pp->name,
						dotQuad(pp->thisHost),
						0,// no need for this field
						CheckPFlag(p, PFLAG_REG),
						1 // out
						);
			}
			/* mamer (?) */
		}
		player_notify(p, _("departed"), _("departure"));
		player_notify_departure(p);
		if (CheckPFlag(p, PFLAG_REG)) {
			pp->totalTime += time(0) - pp->last_command_time;
			player_save(p,0);
		} else {			/* delete unreg history file */
			char fname[MAX_FILENAME_SIZE];
			sprintf(fname, "%s/player_data/%c/%s.games", STATS_DIR,
					pp->login[0], pp->login);
			unlink(fname);
		}
	}

	player_write_logout(p);
	player_remove(p);
	return 0;
}

/* Called every few seconds */
int process_heartbeat(int *fd)
{
	struct tm *nowtm;
	int p;
	time_t now = time(0);

	/* Check for timed out connections */
	for (p = 0; p < player_globals.p_num; p++) {
		struct player *pp = &player_globals.parray[p];

		if ((pp->status == PLAYER_LOGIN || pp->status == PLAYER_PASSWORD)
			&& player_idle(p) > config.ints[login_timeout])
		{
			pprintf(p, "\n**** LOGIN TIMEOUT ****\n");
			*fd = pp->socket;
			return COM_LOGOUT;
		}
		if (pp->status == PLAYER_PROMPT &&
		    player_idle(p) > config.ints[idle_timeout] &&
		    !check_admin(p, ADMIN_ADMIN) &&
		    !in_list(p, L_TD, pp->name) &&
			!in_list(p, L_COMPUTER, pp->name)) {
			pprintf(p, _("\n**** Auto-logout - you were idle more than %u minutes. ****\n"),
				config.ints[idle_timeout]/60);
			*fd = pp->socket;
			return COM_LOGOUT;
		}
	}
	nowtm = localtime(&now);
	if (nowtm->tm_min==0) {
		gics_globals.userstat.users[nowtm->tm_hour*2]=player_count(1);
		save_userstat();
	}
	if (nowtm->tm_min==30) {
		gics_globals.userstat.users[nowtm->tm_hour*2+1]=player_count(1);
		save_userstat();
	}
	if (command_globals.player_high > gics_globals.userstat.usermax) {
		gics_globals.userstat.usermax=command_globals.player_high;
		gics_globals.userstat.usermaxtime=now;
		save_userstat();
	}
	if (command_globals.game_high > gics_globals.userstat.gamemax) {
		gics_globals.userstat.gamemax=command_globals.game_high;
		gics_globals.userstat.gamemaxtime=now;
		save_userstat();
	}

	ShutHeartBeat();
	return COM_OK;
}

/* helper function for sorting command list */
static int command_compare(struct command_type *c1, struct command_type *c2)
{
	return strcasecmp(c1->comm_name, c2->comm_name);
}

void commands_init(void)
{
	FILE *fp, *afp;
	int i = 0;
	int count=0, acount=0;
        char *filename;

	/* sort the command list */
	qsort(command_list,
	      (sizeof(command_list)/sizeof(command_list[0])) - 1,
	      sizeof(command_list[0]),
	      (COMPAR_FN_T)command_compare);

	command_globals.commanding_player = -1;

    filename = malloc(strlen(locale_help_dir) + 10);
    strcpy(filename, locale_help_dir);
    strcat(filename, "/commands");
	fp = fopen_s(filename, "w");
	// gabrielsan: another potential leak spot
	free(filename);
	if (!fp) {
		d_printf(_("CHESSD: Could not write commands help file \"%s\".\n"),
				 filename);
		return;
	}

    filename = malloc(strlen(locale_ahelp_dir) + 10);
    strcpy(filename, locale_ahelp_dir);
    strcat(filename, "/commands");
	afp = fopen_s(filename, "w");
    free(filename);
	if (!afp) {
		d_printf(_("CHESSD: Could not write admin commands help file \"%s\".\n"),
				 filename);
		fclose(fp);
		return;
	}

	while (command_list[i].comm_name) {
		if (command_list[i].adminLevel >= ADMIN_ADMIN) {
			fprintf(afp, "%-19s", command_list[i].comm_name);
			acount++;
			if (acount % 4 == 0)
				fprintf(afp,"\n");
		} else {
			fprintf(fp, "%-19s", command_list[i].comm_name);
			count++;
			if (count % 4 == 0)
				fprintf(fp,"\n");
		}
		i++;
	}

	fprintf(afp,"\n");
	fprintf(fp,"\n");

	fclose(fp);
	fclose(afp);

	d_printf(_("CHESSD: Loaded %d commands (admin=%d normal=%d)\n"), i, acount, count);
}

int check_user(char *user)
{
	int i;

	for (i = 0; i < player_globals.p_num; i++) {
		if (player_globals.parray[i].name != NULL
		&& !strcasecmp(user, player_globals.parray[i].name))
			return 0;
	}
	return 1;
}

/* Need to save rated games */
void TerminateCleanup(void)
{
	int p1, g;

	save_userstat();

	for (g = 0; g < game_globals.g_num; g++) {
		if (game_globals.garray[g].status == GAME_ACTIVE
		 && game_globals.garray[g].rated)
			game_ended(g, WHITE, END_ADJOURN);
	}
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_globals.parray[p1].status != PLAYER_EMPTY){
			pprintf(p1, _("\n    **** Server shutting down immediately. ****\n\n"));
			if (player_globals.parray[p1].status != PLAYER_PROMPT) {
				close(player_globals.parray[p1].socket);
			} else {
				pprintf(p1, _("Logging you out.\n"));
				psend_raw_file(p1, MESS_DIR, MESS_LOGOUT);
				player_write_logout(p1);

				if (CheckPFlag(p1, PFLAG_REG))
					player_globals.parray[p1].totalTime +=
						time(0) - player_globals.parray[p1].last_command_time;

				player_save(p1,0);
			}
		}
	}
	destruct_pending();
}

static int randlet(void){
	return 'A' + (random() % 26);
}

static char *rand_name(char *prefix)
{
   static char name[50];

   srandom(time(0));
   do {
       sprintf(name, "%s%c%c%c%c", prefix, randlet(), randlet(), randlet(), randlet());
   }while(!check_user(name));
   return name;
}

static char *guest_name(void)
{
   return rand_name("Guest");
}

static char *abuser_name(void)
{
   return rand_name("ABUS");
}

/* return the global alias list */
const struct alias_type *alias_list_global(void)
{
	return g_alias_list;
}

/* return a personal alias list */
const struct alias_type *alias_list_personal(int p, int *n)
{
	struct player *pp = &player_globals.parray[p];

	*n = pp->numAlias;
	return pp->alias_list;
}

/*
  report on any missing help pages
*/
int com_acheckhelp(int p, param_list param)
{
	int i, count;

	for (count=i=0; command_list[i].comm_name; i++) {
		char *fname;
		asprintf(&fname, "%s/%s",
			 command_list[i].adminLevel ? locale_ahelp_dir : locale_help_dir,
			 command_list[i].comm_name);
		if (!file_exists(fname)) {
			pprintf(p, _("Help for command '%s' is missing%s\n"),
				command_list[i].comm_name,
				command_list[i].adminLevel ? " (admin)" : "");
			count++;
		}
		free(fname);
	}

	pprintf(p, _("%d commands are missing help files\n"), count);
	d_printf(_("%d commands are missing help files\n"), count);

	for (count=i=0; command_list[i].comm_name; i++) {
		char *fname;
		asprintf(&fname, "%s/%s", locale_usage_dir, command_list[i].comm_name);
		if (!file_exists(fname)) {
			pprintf(p, _("Usage for command '%s' is missing%s\n"),
				command_list[i].comm_name,
				command_list[i].adminLevel ? " (admin)" : "");
			count++;
		}
		free(fname);
	}

	pprintf(p, _("%d commands are missing usage files\n"), count);
	d_printf(_("%d commands are missing usage files\n"), count);

	return COM_OK;
}
