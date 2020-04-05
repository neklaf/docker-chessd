/*
   Copyright 2002 Andrew Tridgell <tridge@samba.org>
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

/*   glue code that connects the chess server to the external timeseal
 *   decoder */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "utils.h"
#include "chessd_locale.h"
#include "globals.h"
#include "timeseal.h"
#include "command.h"

/*
  send a string to the decoder sub-process and return the modified
  (decoded) string the return value is the decoded timestamp. It will
  be zero if the decoder didn't recognise the input as valid timeseal
  data
 */
int pipeError = 0;
int setPipeError(int e)
{
	pipeError = e;
	return e;
}


static unsigned decode(unsigned char *s)
{
	char line[1024];
	char *p;
	unsigned t = 0;
	int ret;

	/* send the encoded data to the decoder process */
	ret = dprintf(timeseal_globals.decoder_conn, "%s\n", s);

	if (pipeError !=0 ) {
		d_printf("DEBUG: timeseal down\n");
		pipeError = 0;

		close(timeseal_globals.decoder_conn);
		timeseal_globals.decoder_conn = -1;

		timeseal_init();
		// just make sure the string is clean because it was not processed
		strcpy((char*) s, "timeseal_fail\n");

		return 0;
	}
	d_printf("DEBUG: RET = %d\n\n", ret);

	// make sure this is empty
	memset(line, 0, 1024);

	if (!fd_gets(line, sizeof(line), timeseal_globals.decoder_conn)) {
		d_printf(_("Bad result from timeseal decoder? (t=%u)\n"), t);
		close(timeseal_globals.decoder_conn);
		timeseal_globals.decoder_conn = -1;

		// restart timeseal
		timeseal_init();
		return 0;
	}
	line[strlen(line)-1] = 0;

	p = strchr(line, ':');
	if (!p) {
		d_printf(_("Badly formed timeseal decoder line: [%s]\n"), line);
		close(timeseal_globals.decoder_conn);
		timeseal_globals.decoder_conn = -1;
		return 0;
	}

	t = atoi(line);
	strcpy((char*)s, p+2);

	return t;
}

/*
 * This command is executed when timeseal failure is detected
 */
int com_timeseal_fail(int p, param_list param)
{
	sys_announce(SYS_EVENT_TIMESEAL_F, ADMIN_ADMIN);
	return COM_OK;
}

/*
 * Comand atsreload --> Restart timeseal.
 */
int com_atsreload (int p, param_list param)
{
	timeseal_init();
	return COM_OK;
}

/*
 * timeseal_setpath --> set the path of the timeseal decoder
 */
void timeseal_setpath(char *s)
{
	timeseal_globals.path = strdup(s);
}

/*
   initialise the timeseal decoder sub-process
NOTE: timeseal_globals.path must be set before run this.
*/
void timeseal_init()
{
	int fd[2];
	pid_t pid;

	/* use a socketpair to get a bi-directional pipe with large buffers */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) {
		d_printf(_("Failed to create socket pair!\n"));
		exit(1);
	}

	pid = fork();

	if (pid == 0) {
		close(fd[1]);
		close(0);
		close(1);
		close(2);
		dup2(fd[0], 0);
		dup2(fd[0], 1);
		open("/dev/null", O_WRONLY); /* stderr */
		execl(timeseal_globals.path, "[timeseal]", NULL);
		exit(1);
	}

	timeseal_globals.decoder_conn = fd[1];
	close(fd[0]);
}

/*
   parse a command line from a user on *con that may be timeseal encoded.
   return 1 if the command should be processed further, 0 if the command
   should be discarded
 */
int timeseal_parse(char *command, struct connection_t *con)
{
	unsigned t;
	unsigned char l;

	/* do we have a decoder sub-process? */
	if (timeseal_globals.decoder_conn <= 0) return 1;

	/* are they using timeseal on this connection? */
	if (!con->timeseal_init && !con->timeseal) return 1;

	// just make sure the string has a 0 (the timeseal buffer is not larger
	// than 1024); absence of 0 might crash the decoder
	l = command[1023];
	command[1023] = 0;

	t = decode((unsigned char*) command);

	// restore the character, after all it might be useful to the server
	command[1023] = l;

	if (t == 0) {
		/* this wasn't encoded using timeseal */
		d_printf(_("Non-timeseal data [%s]\n"), command);
		con->timeseal_init = 0;
		return 1;
	}

	if (con->timeseal_init) {
		con->timeseal_init = 0;
		con->timeseal = 1;
		d_printf(_("Connected with timeseal %s\n"), command);
		if (strncmp(command, "TIMESTAMP|", 10) == 0)
			return 0;
	}

	con->time = t;

	/* now check for the special move time tag */
	if (strcmp(command, "9") == 0) {
		int p = player_find(con->fd);
		struct player *pp = &player_globals.parray[p];
		if ((p >= 0) && (pp->game >= 0) && (pp->game < game_globals.g_num)) {
			int g = pp->game;
			if (game_globals.garray[g].game_state.onMove !=
			    pp->side) {
				return 0;
			}
			if (pp->side == WHITE) {
				if (game_globals.garray[g].wTimeWhenReceivedMove == 0)
					game_globals.garray[g].wTimeWhenReceivedMove = t;
			} else {
				if (game_globals.garray[g].bTimeWhenReceivedMove == 0)
					game_globals.garray[g].bTimeWhenReceivedMove = t;
			}
			if (game_globals.garray[g].flag_pending != FLAG_NONE) {
				ExecuteFlagCmd(p, net_globals.con[pp->socket]);
			}
		}
		/* we have processed this special tag - don't process it further */
		return 0;
	}

	return 1;
}

/*
  used to call flag on players with timeseal
 */
void ExecuteFlagCmd(int p, struct connection_t *con)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;

	/* Extra validation, weird errors on line 215 */
	if (pp->game < 0 || pp->game >= game_globals.g_num ||
		game_globals.garray[pp->game].status != GAME_ACTIVE)
		return;

	gg = &game_globals.garray[pp->game];

	if (pp->side == WHITE) {
		gg->wRealTime -= (int)con->time - gg->wTimeWhenReceivedMove;
		gg->wTimeWhenReceivedMove = con->time;
		if (gg->wRealTime < 0)
			pcommand(pp->opponent, "flag");
	} else if (pp->side == BLACK) {
		gg->bRealTime -= (int)con->time - gg->bTimeWhenReceivedMove;
		gg->bTimeWhenReceivedMove = con->time;
		if (gg->bRealTime < 0)
			pcommand(pp->opponent, "flag");
	}

	game_update_time(pp->game);
	gg->flag_pending = FLAG_NONE;
}
