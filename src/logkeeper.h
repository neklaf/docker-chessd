/*
   Copyright (c) 2004 Federal University of Parana

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
#ifndef _LOG_KEEPER_H
#define _LOG_KEEPER_H

/*	Log Keeper module - implements logging functions and constants
 */

#define LOG_FAILED	-1
#define LOG_OK		0

// Users interactions log (talk interactions)
#define TLOG_TELL			0x01
#define TLOG_CHANNEL		0x02
#define TLOG_SHOUT			0x03
#define TLOG_CSHOUT			0x04
#define TLOG_IT				0x05

#include "command.h"
#include "playerdb.h"

// Admin's action log
enum {
	// just server
	ALOG_NOTHING = 0,
	ALOG_SRV_CMD,
	ALOG_ON_USER,
	ALOG_USER_CHANGE,	/* user data edited by admin; includes
						   admin promotion/demotion, rating change, etc. */
	ALOG_VAR_CHANGE, 	/* change in variables; includes ban */

	// these actions are on the site
	ALOG_ADJUDICATION,	/* adjudicated game */
	ALOG_TOPIC_CHANGE,	/* change/removal of forum/help topic */
	ALOG_NEWS_CHANGE,	/* change/removal of news topic  */
	ALOG_CLASS_CHANGE,	/* change/removal of class */

	ALOG_ADMIN_LAST,	// used to separate admin from user

// User actions
	LOG_TALK,				// talk
	LOG_USER,				// user is a target
	LOG_GAME,				// change in games
	LOG_VAR,				// change in lists or variables
	LOG_SRV,				// the not so important commands
	LOG_CMD_ERROR
};

int log_event(int action_type,
			  int fromuserid, 		// if this is set, the next may be null
			  char *fromusername,	// just set this whem fromuserid is not set
			  char *fromip,
			  char *targetname,
			  char **parameters,
			  int par_count
			  );

int fillLogPar(struct command_type *cmd_def,
			   struct player *pp,
			   char *raw_cmd,
			   char *prc_cmd,
			   int result_status,
			   param_list params,
			   char *logParameters[],
			   char **targetUser
			   );


typedef struct log_entry {
	char *name;
	char *cmd_xmltag;
	char *par_types;
	char *param_names[10];

} log_entry;

#endif
