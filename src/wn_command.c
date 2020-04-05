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
#include "chessd_locale.h"
#include "common.h"
#include "globals.h"
#include "malloc.h"
#include "shutdown.h"
#include "crypt.h"
#include "wn_utils.h"
#include "wn_command_network.h"
#include "configuration.h"
#include "config_genstrc.h"
#include "seekproc.h"
#include "command.h"

#define WEB_SITE_INTERFACE 	"webuserinterface"
#define WN_GUEST 			"guest"

#define BOT_EMPTY 		0
#define BOT_NEW 		1
#define BOT_INQUEUE 	2
#define BOT_LOGIN 		3
#define BOT_PASSWORD 	4
#define BOT_PROMPT 		5

// non interactive commands
#define BOT_EXEC_CMD	6

const wn_command_def wn_com_def[] = {
	{ADMIN_USER, COP_NOP, 			0, wn_com_nothing},
	{ADMIN_GOD,	 COP_LIST_UPDATE, 	2, wn_com_list_update},
	{ADMIN_GOD,	 COP_INCOMING_MSG, 	3, wn_com_incoming_msgs},
	{ADMIN_GOD,	 COP_ADJUDICATE, 	4, wn_com_adjudicate},
	{ADMIN_GOD,	 COP_KICK, 			3, wn_com_kick},
	{ADMIN_USER, COP_LOGOUT,		0, wn_com_logout},
	{ADMIN_GOD,	 COP_MSG_READ, 		3, wn_com_read_msgs},
	{ADMIN_USER, COP_GET_USR_LEVEL, 1, wn_com_get_usr_level},
	{ADMIN_USER, COP_GET_USR_ID, 	1, wn_com_get_usr_id}
};

static char *guest_name(void)
{
	static char name[20];
	int found;

#define RANDLET ((char)('A' + (random() % 26)))
	srandom(time(0));

	found = 0;
	while(!found) {
		snprintf(name,sizeof(name), "Guest%c%c%c%c", RANDLET, RANDLET, RANDLET, RANDLET);
		found = check_user(name);
	}
#undef RANDLET
	return name;
}


static int wn_process_login(int p, char *loginname)
{
	struct player *pp = &player_globals.parray[p];
	int is_guest;

	loginname = eatwhite(loginname);

	// gabrielsan: debug
	// d_printf("Login received: %s\n", loginname);

	if (!*loginname) {
		// don't accept empty login

		//d_printf("error: empty\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_LOGIN_EMPTY);
		return COM_LOGOUT;
	}

	/*	test whether the login is valid, characterwise */
	if (!alphastring(loginname)) {
		// don't accept invalid login
		//d_printf("error: invalid(alpha)\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_LOGIN_INVALID);
		return COM_LOGOUT;
	}

	if (strlen(loginname) < 3) {
		// don't accept short logins

		//d_printf("error: short\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_LOGIN_SHORT);

		return COM_LOGOUT;
	}

	if (strlen(loginname) > (MAX_LOGIN_NAME - 1)) {
		// don't accept long logins

		//d_printf("error: long\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_LOGIN_LONG);
		return COM_LOGOUT;
	}

	/* if 'guest' was specified then pick a random guest name */
	if (strcasecmp(loginname, config.strs[guest_login]) == 0) {
		loginname = guest_name();
		is_guest = 1;
	}

	// check whether it is an acceptable user for this port;
	// we are only accepting webuserinterface or guests

	if (player_read(p, loginname) != 0) {
		is_guest = 1;
		strcpy(pp->name, loginname);
		pp->chessduserid = -1;

		pp->status = BOT_PASSWORD;

		// tell the other side to send the password
		wn_data(p, 2, SRO_REQUEST, SR_PASSWD);

		return COM_OK;

	}
	else {
		// if the user exists, it will only accept if this is the
		// website user for now (this will be changed later)
		if (strcmp(loginname, WEB_SITE_INTERFACE)) {
			// no other user is allowed

			//d_printf("error: incorrect user\n");
			//fflush(stderr);
			wn_data(p, 2, SRO_STATUS_RESULT, SCR_LOGIN_INVALID);

			return COM_LOGOUT;
		}
	}

	// check whether the player information is corrupt
	if (strcasecmp(loginname, pp->name)) {
		// bad name field

		//d_printf("error: corrupt name\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_PROFILE_NAME);
		return COM_LOGOUT;
	}

	if (CheckPFlag(p, PFLAG_REG)
			&& (pp->fullName == NULL)) {

		// bad full name
		//d_printf("error: corrupt full name\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_PROFILE_FULLNAME);
		return COM_LOGOUT;
	}

	if (CheckPFlag(p, PFLAG_REG)
			&& (pp->emailAddress == NULL)) {
		// bad email

		//d_printf("error: corrupt email\n");
		//fflush(stderr);
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_PROFILE_EMAIL);

		return COM_LOGOUT;
	}

	// if it is here, process the user password; this is to protect
	// the backdoor from smartasses

	pp->status = BOT_PASSWORD;

	// tell the other side to send the password
	wn_data(p, 2, SRO_REQUEST, SR_PASSWD);

	return COM_OK;
}

static int wn_process_password(int p, char *password)
{
	struct player *pp = &player_globals.parray[p];
	char salt[3];
	int fd;
	struct in_addr fromHost;
	int dummy; /* (to hold a return value) */


	//d_printf("got this pass: %s\n", password);

	if (pp->passwd && CheckPFlag(p, PFLAG_REG)) {
		salt[0] = pp->passwd[3];
		salt[1] = pp->passwd[4];
		salt[2] = '\0';

		if (strcmp(chessd_crypt(password,salt), pp->passwd) ) {
			fd = pp->socket;
			fromHost = pp->thisHost;
			if (*password) {
				wn_data(p, 2, SRO_STATUS_RESULT, SCR_PASSWD_WRONG);
				d_printf(_("FICS (process_password): "
						   "Bad password for %s [%s] [%s] [%s]\n"),
						pp->login,
						password,
						salt,
						pp->passwd);
			}
			player_clear(p);
			pp->logon_time = pp->last_command_time = time(0);
			pp->status = BOT_LOGIN;
			pp->socket = fd;
			if (fd >= webnet_globals.no_file)
				d_printf(_("FICS (process_password): Out of range fd!\n"));

			pp->thisHost = fromHost;

			return COM_LOGOUT;
		}
	}

	pp->status = BOT_EXEC_CMD;
	wn_data(p, 2, SRO_STATUS_RESULT, SCR_OK);

	pp->lastHost = pp->thisHost;
	pp->logon_time = pp->last_command_time = time(0);

	if (CheckPFlag(p, PFLAG_REG) && !pp->timeOfReg)
		pp->timeOfReg = pp->last_command_time;
	dummy = check_and_print_shutdown(p);

	return 0;
}

int wn_process_command(int p, char *command)
{
	int *cmd;
	int opcode;
	int res = 0;


	cmd = (int*)command;
	opcode = *cmd;
	cmd++;

	// excute the command using cmd as parameters and tell the other
	// side what happened
	if ((opcode > COP_NOP) && (opcode < COP_SENTINEL)) {
		if (wn_com_def[opcode].wn_com_proc != NULL)
			res = wn_com_def[opcode].wn_com_proc(p, cmd);
		wn_data(p, 2, SRO_STATUS_RESULT, res);
	}
	else {
		// command opcode out of valid range
		wn_data(p, 2, SRO_STATUS_RESULT, SCR_INVALID_COMMAND);
	}

	return COM_OK;
}

/* Return 1 to disconnect */
int wn_process_input(int fd, char *com_string)
{
	int p = player_find(fd);
	int retval = 0;
	struct player *pp;

	if (p < 0) {
		d_printf( _("CHESSD: Input from a player not in array!\n"));
		return -1;
	}

	pp = &player_globals.parray[p];

	command_globals.commanding_player = p;
	pp->totalTime += time(0) - pp->last_command_time;
	pp->last_command_time = time(0);

	switch (pp->status) {
		case BOT_EMPTY:
			d_printf( _("WN_CHESSD: Command from an empty player!\n"));
			break;
		case BOT_NEW:
			d_printf( _("WN_CHESSD: Command from a new player!\n"));
			break;
		case BOT_INQUEUE:
			/* Ignore input from player in queue */
			break;
		case BOT_LOGIN:
			retval = wn_process_login(p, com_string);
			if (retval == COM_LOGOUT && com_string != NULL)
				d_printf(_("%s tried to log in and failed.\n"), com_string);
			break;
		case BOT_PASSWORD:
			retval = wn_process_password(p, com_string);
			break;
		case BOT_EXEC_CMD:
			FREE(pp->busy);
			retval = wn_process_command(p, com_string);
			break;
	}

	command_globals.commanding_player = -1;
	return retval;
}

int wn_process_new_connection(int fd, struct in_addr fromHost)
{
	struct player *pp;
	int p = player_new();

	pp = &player_globals.parray[p];
	pp->status = BOT_LOGIN;
	if (fd >= webnet_globals.no_file)
		d_printf(_("FICS (process_new_connection): Out of range fd!\n"));

	pp->socket = fd;
	pp->thisHost = fromHost;
	pp->logon_time = time(0);

	// tell the other side to send the login
	// debug
	//d_printf("sending login request\n");
	//fflush(stderr);
	wn_data(p, 2, SRO_REQUEST, SR_LOGIN);

	return 0;
}

int wn_process_disconnection(int fd)
{
	int p = player_find(fd);
	struct player *pp;

	if (p < 0) {
		d_printf( _("CHSSD: Disconnect from a player not in array!\n"));
		return -1;
	}

	pp = &player_globals.parray[p];

	//  withdraw_seeks(p);
	seekremoveall(p);

	player_remove(p);
	return 0;
}

/* Called every few seconds */
int wn_process_heartbeat(int *fd)
{
	//int p;

	/* Check for timed out connections */

	// don't do that now
	return COM_OK;
}

