#ifndef _COMMAND_WN_NETWORK_H
#define _COMMAND_WN_NETWORK_H

#include "wn_network.h"
#include "wn_com_proc.h"

/*
 *	protocol:
 *
 *	incoming:
 *		requests to the server:
 *			CMD|WHO|PAR1|PAR2|....
 *		alive:
 *			ACK = 0
 *
 *	outgoing:
 *		replies:
 *			Type Of Msg|Parameter
 *			TM STATUS:  Parameter is a result status
 *						(see enums for more)
 *			TM REQUEST: Paramenter is a request (see enums for more)
 *			TM XML:     Parameter is a XML text
 */

enum command_opcodes
{
	 COP_NOP = 0,
	 COP_LIST_UPDATE,
	 COP_INCOMING_MSG,
	 COP_ADJUDICATE,
	 COP_KICK,
	 COP_LOGOUT,
	 COP_MSG_READ,
	 COP_GET_USR_LEVEL,
	 COP_GET_USR_ID,
	 COP_SENTINEL
};

enum server_reply_opcode
{
	SRO_STATUS_RESULT = 0,
	SRO_REQUEST,
	SRO_XML
};

enum server_requests {
	SR_ALIVE = 0,	// request answer to know whether the other side is alive
	SR_LOGIN,		// request login
	SR_PASSWD,		// request password
	SR_CMD			// tell it is ready to process any commands; ?
};

enum server_cmd_result {
	SCR_OK = 0,			// command/operation successfully executed
	// from here, error reporting

	// bad login
	SCR_LOGIN_INVALID,	// login is not valid, charwise
	SCR_LOGIN_SHORT,	// too short
	SCR_LOGIN_LONG,		// too long
	SCR_LOGIN_EMPTY,	// empty login

	// corrupted profile
	SCR_PROFILE_NAME,		//
	SCR_PROFILE_FULLNAME,
	SCR_PROFILE_EMAIL,

	// bad password
	SCR_PASSWD_WRONG,	// incorrect pass

	SCR_INVALID_COMMAND,// invalid command

	SCR_NOT_LOGGED,		// tried to interact with a non logged player
	SCR_OBJ_NOT_EXIST	// tried to handle a non existent object
						// (might be a player or a game, for ex)
};

typedef struct {
	int adminLevel;		// level required to execute this command
	int opcode;
	int par_count;		// count of parameters; right now, only int params
	// TODO: eventually, implement a param type description

	int (*wn_com_proc)(int, int* );
} wn_command_def;

// finally, the command array

extern const wn_command_def wn_com_def[];

int wn_process_input(int fd, char *com_string);
int wn_process_new_connection(int fd, struct in_addr fromHost);
int wn_process_disconnection(int fd);
int wn_process_heartbeat(int *fd);
void wn_TerminateCleanup(void);
int wn_process_command(int p, char *command);

#endif
