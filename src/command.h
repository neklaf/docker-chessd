/*
   Copyright (c) 1993 Richard V. Nash.
   Copyright (c) 2000 Dan Papasian
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

#ifndef _COMMAND_H
#define _COMMAND_H

// #include "genparser.h" gabrielsan: TODO

/* Maximum length of a login name +1 */
#define MAX_LOGIN_NAME 18

/* Maximum number of parameters per command */
#define MAXNUMPARAMS 10

/* Maximum string length of a single command word */
#define MAX_COM_LENGTH 50

/* Maximum number of commands per line */
#define MAX_COM_LINE	5

/* Maximum string length of the whole command line */
#define MAX_STRING_LENGTH 1024

#define COM_OK 				0
#define COM_FAILED 			1
#define COM_ISMOVE 			2
#define COM_AMBIGUOUS 		3
#define COM_BADPARAMETERS 	4
#define COM_BADCOMMAND 		5
#define COM_LOGOUT 			6
#define COM_FLUSHINPUT 		7
#define COM_RIGHTS 			8
#define COM_OK_NOPROMPT 	9
#define COM_ISMOVE_INSETUP 10
#define COM_FAILED_QUIETLY 11

#define CMD_OK					0
#define CMD_TELL_CENSOR			1
#define CMD_NO_USER				2
#define CMD_TELL_INVALID_CHAR	3
#define CMD_TELL_NO_REG			4

#define ADMIN_USER			0
#define ADMIN_HELPER		5
#define ADMIN_ADJUDICATOR	7
#define ADMIN_PUNISHER		9
#define ADMIN_ADMIN			10
#define ADMIN_MASTER		30
#define ADMIN_MAJOR			50
#define ADMIN_DEMIGOD   	95
#define ADMIN_GOD			100


/// system event types
#define SYS_EVENT_DBDOWN 		0
#define SYS_EVENT_DBUP			1
#define SYS_EVENT_DBDOWN_ADM	2
#define SYS_EVENT_TIMESEAL_F	3


#define TYPE_NULL 0
#define TYPE_WORD 1
#define TYPE_STRING 2
#define TYPE_INT 3
typedef struct u_parameter {
  int type;
  union {
    char *word;
    char *string;
    int integer;
  } val;
} parameter;

typedef parameter param_list[MAXNUMPARAMS];

typedef struct command_type {
	char *comm_name;
	char *param_string;
	int (*comm_func)();
	int adminLevel;

	// this describes how this command should be log'ed
	int logType;
} command_type;

/*GENSTRUCT*/ struct alias_type {
	char *comm_name;
	char *alias;
};


void commands_init(void);
void TerminateCleanup(void);
int check_user(char *user);
#endif /* _COMMAND_H */
