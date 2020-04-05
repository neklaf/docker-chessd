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

#define MY_ENCODING "UTF-8"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "logkeeper.h"
#include "dbdbi.h"
#include "utils.h"
#include "malloc.h"
#include "chessd_locale.h"

#include "globals.h"
#include "command.h"
#include "playerdb.h"

/*
 * Describes the data sent in the "parameter" field
 *
 */
// d = integer f = float s = string
static struct log_entry log_entry_desc[] = {
	{"NONE"		   , NULL, NULL, {} },
	//
	{"ALOG_SERVER_CMD"  , "ActionDetails", "sss",
		{"TypedText", "Command",  "Result" }
	},
	{"ALOG_ON_USER"  , "ActionDetails", "ssss",
		{"TypedText", "Command",  "Result", "Target" }
	},

	//
	{"ALOG_USER_CHANGE" , "ActionDetails", "dssss",
		{"TargetID", "TargetUser", "AffectedAttribute",
		 "OldValue", "MewValue"}
	},
	//
	{"ALOG_VAR_CHANGE"  , "ActionDetails", "sssss",
		{"TargetUserID", "TargetUser", "VariableName", "OldValue", "MewValue"}
	},

	//
	{"ALOG_ADJUDICATION", "ActionDetails", "ds",
		{"GameID", "GameResult" }
	},
	//
	{"ALOG_TOPIC_CHANGE", "ActionDetails", "ds",
		{"TopicID", "Operation" }
	},
	//
	{"ALOG_NEWS_CHANGE" , "ActionDetails", "ds",
		{"NewsID", "Operation" }
	},
	//
	{"ALOG_CLASS_CHANGE", "ActionDetails", "ds",
		{"ClassID", "Operation" }
	},
	{"ALOG_ADMIN_LAST"		   , NULL, NULL, {} },

	{"LOG_TALK", "ActionDetails", "sssss",
		{"TypedText", "Command",  "Result", "Target", "Message" },
	},
	{"LOG_USER", "ActionDetails", "ssss",
		{"TypedText", "Command",  "Result", "Target"},
	},
	{"LOG_GAME", "ActionDetails", "sssss",
		{"TypedText", "Command",  "Result", "Opponent", "GameID"},
	},
	{"LOG_VAR", "ActionDetails", "sss",
		{"TypedText", "Command",  "Result" },
	},
	{"LOG_SRV", "ActionDetails", "sss",
		{"TypedText", "Command",  "Result"},
	},
	{"LOG_CMD_ERROR", "ActionDetails", "sss",
		{"TypedText", "Command", "Result"},
	}
};

int get_userid(char *player_name)
{
    int ret;
    dbi_result res;

    res = dbi_conn_queryf( dbiconn, "SELECT chessduserid FROM ChessdUser "
    								"WHERE UPPER(username) = UPPER('%s')",
						   player_name );
    if(res && dbi_result_next_row(res))
    	ret = dbi_result_get_int_idx(res,1);
    else
    	ret = -1;
    if(res) dbi_result_free(res);
    return ret;
}

char* make_xml_desc(int  action_type,
					char *fromusername,
					char *fromip,
					char *targetname,
					char **par,
					char *when,
					int  par_count)
{

// let's make things easier

#define TEST_AND_FREE_ON_ERROR(ACTION_TYPE, ELEMENT) \
	if (rc < 0) {		\
		d_printf(U_("Action %s could not be performed. Element: '%s'"), \
					ACTION_TYPE, (ELEMENT)?(ELEMENT):"(null)");	\
		xmlBufferFree(buf);					\
		xmlFreeTextWriter(writer);			\
		return NULL;						\
	}

    xmlBufferPtr buf;		// buffer used to keep the generated xml
    xmlTextWriterPtr writer;// the xml writer
	char *xml_desc;
    int i, rc;
	int cmd_max_par;

	buf = xmlBufferCreate();
	// creates the writer
	writer = xmlNewTextWriterMemory(buf, 0);
	if (writer == NULL) {
		d_printf("CHESSD: Error creating the xml writer\n");
		return NULL;
	}

	// tries to initialize the xml writer
	rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
	if (rc < 0) {
		d_printf(U_("Could not initialize start xml writer"));
		return NULL;
	}

	// creates the first node, which will always be the same for every
	// log entry
	rc = xmlTextWriterStartElement(writer, BAD_CAST "AdminActionLog");
	TEST_AND_FREE_ON_ERROR("WriteTag", "AdminActionLog");

	rc = xmlTextWriterWriteFormatElement(writer,
			BAD_CAST "ActionType", "%s", log_entry_desc[action_type].name);
	TEST_AND_FREE_ON_ERROR("WriteTagElement", "action_type");

	// who issued the command
	rc = xmlTextWriterWriteFormatElement(writer,
			BAD_CAST "FromUser", "%s", fromusername);
	TEST_AND_FREE_ON_ERROR("WriteTagElement", "FromUser");

	// "who"'s IP
	rc = xmlTextWriterWriteFormatElement(writer,
			BAD_CAST "FromIP", "%s", fromip);
	TEST_AND_FREE_ON_ERROR("WriteTagElement", "FromIP");


	// when
	rc = xmlTextWriterWriteFormatElement(writer,
			BAD_CAST "ActionTime", "%s", when);
	TEST_AND_FREE_ON_ERROR("WriteTagElement", "ActionTime");

	// grab the log entry parameters and write the xml
	rc = xmlTextWriterStartElement(writer,
			BAD_CAST log_entry_desc[action_type].cmd_xmltag);
	TEST_AND_FREE_ON_ERROR("WriteTag",
			log_entry_desc[action_type].cmd_xmltag);

	cmd_max_par = strlen(log_entry_desc[action_type].par_types);

	// write the tags and the values to xml
	for (i=0; (i < par_count) && (i < cmd_max_par); i++) {
		rc = xmlTextWriterWriteFormatElement(writer,
				BAD_CAST log_entry_desc[action_type].param_names[i],
				"%s",
				par[i]?par[i]:"(null)");

		TEST_AND_FREE_ON_ERROR("WriteTagElement",
				log_entry_desc[action_type].param_names[i]);
	}
	rc = xmlTextWriterEndElement(writer);
	TEST_AND_FREE_ON_ERROR("end_element",
			log_entry_desc[action_type].cmd_xmltag);


	// again, back to the default tags
	rc = xmlTextWriterEndElement(writer);
	TEST_AND_FREE_ON_ERROR("end_element", "AdminActionLog");

	rc = xmlTextWriterEndDocument(writer);
	TEST_AND_FREE_ON_ERROR("end_writer", "");

	// clean up everything
	xmlFreeTextWriter(writer);

	xml_desc = strdup((char*) buf->content);
	xmlBufferFree(buf);

	return xml_desc;
}

int log_event(int action_type,
			  int fromuserid, 		// if this is set, the next may be null
			  char *fromusername,	// just set this whem fromuserid is not set
			  char *fromip,
			  char *targetname,
			  char **parameters,
			  int par_count
			  )
{
	time_t now = time(NULL);
	char *sql = "INSERT INTO AdminActionLog (action_type, fromuserid, fromip,"
											"targetname, xml_desc, eventtime) "
								   " VALUES (%i, %i, '%s', '%s', %s, %m)";
	char *xml_desc, *esc_xml_desc;

	if (fromuserid == -1) {
		// use fromusername
		if (fromusername == NULL) // no,no,no: someone _must_ be responsible
			return LOG_FAILED;
		// get the user id using the name
		fromuserid = get_userid(fromusername);
		// from hereon, it may be a guest
	}

	xml_desc = make_xml_desc(action_type, fromusername, fromip, targetname, 
							 parameters, ctime(&now), par_count);

	if (dbi_conn_quote_string_copy(dbiconn, xml_desc, &esc_xml_desc))
	{
		dbi_result_free( dbi_conn_queryf( dbiconn, sql, 
										  action_type, fromuserid, fromip, 
										  targetname, esc_xml_desc, now ) );
		free(esc_xml_desc);
	}else{ // better keep it anyway. FIXME: Do we reformat parameters ourselves?!
		dbi_result_free( dbi_conn_queryf( dbiconn, sql,
										  action_type, fromuserid, fromip, 
										  targetname, "NULL", now) );
	}

#ifdef DEBUG
	d_printf("%s\n", xml_desc);
#endif

	FREE(xml_desc);
	
	return LOG_OK;
}

int fillLogPar(
		struct command_type *cmd_def,
		struct player *pp,
		char *raw_cmd,
		char *prc_cmd,
		int result_status,
		param_list params,
		char *logParameters[],
		char **targetUser
	   )
{
	struct game *gg;
	int logParCount, i, logType;

	// LOG 1: fill what every log entry expects from commands in the server:
	// 	  the command, what the user typed and the result (successful or not)

	logParCount = 3;
	// the fulltext typed by the user, no expansion
	logParameters[0] = strdup(raw_cmd);
	// the command
	logParameters[1] = strdup(prc_cmd);

	// assume error, try to prove it is not
	logType = LOG_CMD_ERROR;

	if (cmd_def != NULL) {
		switch(result_status) {
			case COM_BADCOMMAND:
			case COM_FAILED:
				logParameters[2] = strdup("Command does not exist.");
				break;
			case COM_BADPARAMETERS:
				logParameters[2] = strdup("Bad Parameters");
				break;
			case COM_AMBIGUOUS:
				logParameters[2] = strdup("Ambiguous Command");
				break;
			case COM_RIGHTS:
				logParameters[2] = strdup("Access Denied");
				logType = cmd_def->logType;
				break;
			default:
				logParameters[2] = strdup("Success");
				logType = cmd_def->logType;
		}
	}
	// ok, now the log specifics
	switch(logType) {
		case LOG_TALK:
			// some types of talk requires a target user, some doesn't
			logParCount = 5;

			i = 0;
			if (params[i].type == TYPE_WORD) {
				if (pp->last_cmd_error == CMD_OK) {
					// this is the target (probally a user)
					logParameters[3] = strdup(pp->last_tell);
					*targetUser = strdup(pp->last_tell);
				}
				else {
					FREE(logParameters[2]);

					logParameters[3] = strdup(params[i].val.string);
					*targetUser = strdup(params[i].val.string);
					switch(pp->last_cmd_error) {
						case CMD_NO_USER:
							logParameters[2] = strdup("No such user");
							break;
						case CMD_TELL_CENSOR:
							logParameters[2] = strdup("User censored");
							break;
						case CMD_TELL_INVALID_CHAR:
							logParameters[2] = strdup("Invalid characters");
							break;
						case CMD_TELL_NO_REG:
							logParameters[2] = strdup("Not listening unreg");
							break;
						default:
							logParameters[2] = strdup("Unknown Error");
					}
				}
				i++;
			}
			else
				if (params[i].type == TYPE_INT) {
					// this is the target (probably a channel)
					asprintf(&logParameters[3], "%d", params[i].val.integer);
					*targetUser = strdup("");
					i++;
				}
				else
					logParameters[3] = strdup("");

			// whatever remains is the message
			logParameters[4] = strdup(params[i].val.string);

			break;
		case LOG_USER:
		case ALOG_ON_USER:
			// the target user is always the 1st par in this type of comm
			logParCount = 4;

			logParameters[3] = strdup(params[0].val.string);

			*targetUser = strdup(params[0].val.string);

			if (pp->last_cmd_error == CMD_NO_USER) {
				FREE(logParameters[2]);
				logParameters[2] = strdup("No such user");
			}
			break;
		case LOG_GAME:
			logParCount = 5;
			if (is_valid_game_handle(pp->game)) {
				asprintf(&logParameters[3], "%s",
						player_globals.parray[pp->opponent].name);

				gg = &game_globals.garray[pp->game];
				asprintf(&logParameters[4], "%d", gg->gameID);
			}
			else {
				logParameters[3] = strdup("No Opponent");
				logParameters[4] = strdup("No Game");
			}
			*targetUser = strdup("");
			break;
		// these codes are just used to make it easier to spot them; no
		// extra parameter is required
		case LOG_VAR:
		case LOG_SRV:
		case ALOG_SRV_CMD:
			*targetUser = strdup("");
			break;
	}

	return logParCount;
}

