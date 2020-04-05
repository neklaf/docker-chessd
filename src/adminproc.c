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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/**
 *     adminproc.c - All administrative commands and related functions
 */

#include <stdlib.h>
#include <string.h>
#include "comproc.h"
#include "crypt.h"
#include "gamedb.h"
#include "globals.h"
#include "playerdb.h"
#include "utils.h"
#include "chessd_locale.h"
#include "gameproc.h"
#include "ratings.h"
#include "configuration.h"
#include "malloc.h"
#include "command_network.h"

#define PASSLEN 4

/*
 * This function is used to announce system events;
 * 		event_type: the type of event (used to find out which message to use)
 * by GabrielSan
 */

char* sysEventMsg[] = {
	N_("The database has gone down. Until further notice, the games are unrated.\n"),
	N_("The database has been restored. Everything is back to normal. Sorry for the inconvenience."),
	N_("While the system is down, any modification to lists won't be saved, that means banishment won't work."),
	N_("There was a temporary failure in timeseal. It was already fixed.")
};

int sys_announce(int event_type, int admin_level)
{
	int p1;

	d_printf("CHESSD: SYSTEM ANNOUNCE %s\n", sysEventMsg[event_type]);

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_globals.parray[p1].status == PLAYER_PROMPT
			&& check_admin(p1, admin_level))
		{
			pprintf_prompt(p1, _("\n\n    **SYSTEM ANNOUNCEMENT**: %s\n\n"),
						   _(sysEventMsg[event_type]) );
		}
	}
	return COM_OK;
}

/*
  check that a player has sufficient rights for an operation
*/
int check_admin(int p, int level)
{
	struct player *pp = &player_globals.parray[p];
	/* gods and head admins always get what they want */
	return pp->adminLevel >= ADMIN_GOD || player_ishead(p) || pp->adminLevel >= level;
}

int check_admin_level(int p, int level)
{
	struct player *pp = &player_globals.parray[p];
	return pp->adminLevel == level;
}

/*
  check that player p1 can do an admin operation on player p2
*/
int check_admin2(int p1, int p2)
{
	return check_admin(p1, player_globals.parray[p2].adminLevel+1);
}

/*
 * adjudicate
 *
 * Usage: adjudicate white_player black_player result
 *
 *   Adjudicates a saved (stored) game between white_player and black_player.
 *   The result is one of: abort, draw, white, black.  "Abort" cancels the game
 *   (no win, loss or draw), "white" gives white_player the win, "black" gives
 *   black_player the win, and "draw" gives a draw.
 */
int com_adjudicate(int p, param_list param)
{
	int wp, wconnected, bp, bconnected, g, inprogress, gameFounded, badSyntax;
	struct player *pgw, *pgb;

	gameFounded = 1;
	badSyntax = 0;

	if (!FindPlayer(p, param[0].val.word, &wp, &wconnected))
		return COM_OK;
	if (!FindPlayer(p, param[1].val.word, &bp, &bconnected)) {
		if (!wconnected)
			player_remove(wp);
		return COM_OK;
	}

	pgw = &player_globals.parray[wp];
	pgb = &player_globals.parray[bp];

	inprogress = pgw->game >=0 && pgw->opponent == bp;

	if (inprogress)
		g = pgw->game;
	else {
		g = game_new();
		/*
		 *	gabrielsan: game read is restored now
		 */
		if (game_read(g, wp, bp) < 0) {
			gameFounded = 0;
			pprintf(p, _("There is no stored game %s vs. %s\n"),
					pgw->name, pgb->name);
		} else {
			game_globals.garray[g].white = wp;
			game_globals.garray[g].black = bp;
		}
	}

	if (gameFounded) {
		if (strstr("abort", param[2].val.word) != NULL) {
			game_ended(g, WHITE, END_ADJABORT);
			if (wconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been aborted."),
						 pgw->name, pgw->name, pgb->name);
			if (bconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been aborted."),
						 pgb->name, pgw->name, pgb->name);
		} else
		if (strstr("draw", param[2].val.word) != NULL) {
			game_ended(g, WHITE, END_ADJDRAW);
			if (wconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a draw"), pgw->name, pgw->name, pgb->name);
			if (bconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a draw"), pgb->name, pgw->name, pgb->name);

		} else if (strstr("white", param[2].val.word) != NULL) {
			game_ended(g, WHITE, END_ADJWIN);
			if (wconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a win"), pgw->name, pgw->name, pgb->name);
			if (bconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a loss"), pgb->name, pgw->name, pgb->name);
		} else if (strstr("black", param[2].val.word) != NULL) {
			game_ended(g, BLACK, END_ADJWIN);
			if (wconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a loss"), pgw->name, pgw->name, pgb->name);
			if (bconnected)
				pcommand(p, _("message %s Your game \"%s vs. %s\" has been adjudicated "
							  "as a win"), pgb->name, pgw->name, pgb->name);
		} else {
			badSyntax = 1;
			pprintf(p, _("Result must be one of:  abort  draw  white  black\n"));
		}
	}
	if (gameFounded) {
		if(!badSyntax) {
			pprintf(p, _("Game adjudicated.\n"));
			if (!inprogress) {
				/*	gabrielsan:
				 *		instead of player numbers, use now player IDs
				 *		to remove a game
				 */
				game_delete(wp, bp);
			}
		}
		d_printf("Game removal started in: com_adjudicate\n");
		game_remove(g);
	}

	if (!wconnected)
		player_remove(wp);
	if (!bconnected)
		player_remove(bp);
	return COM_OK;
}

int com_pose(int p, param_list param)
{
	int p1;

	if ((p1 = player_find_part_login(param[0].val.word)) < 0) {
		pprintf(p, _("%s is not logged in.\n"), param[0].val.word);
		return COM_OK;
	}
	if (!check_admin2(p, p1)) {
		pprintf(p, _("You can only pose as players below your adminlevel.\n"));
		return COM_OK;
	}
	pprintf(p, _("Command issued as %s\n"), player_globals.parray[p1].name);
	pcommand(p1, "%s\n", param[1].val.string);
	return COM_OK;
}

/*
 * asetv
 *
 * Usage: asetv user instructions
 *
 *   This command executes "set" instructions as if they had been made by the
 *   user indicated.  For example, "asetv DAV shout 0" would set DAV's shout
 *   variable to 0.
 */
int com_asetv(int p, param_list param)
{
	int p1;

	if ((p1 = player_find_part_login(param[0].val.word)) < 0) {
		pprintf(p, _("%s is not logged in.\n"), param[0].val.word);
		return COM_OK;
	}
	if (!check_admin2(p, p1)) {
		pprintf(p, _("You can only aset players below your adminlevel.\n"));
		return COM_OK;
	}
	pprintf(p, _("Command issued as %s\n"), player_globals.parray[p1].name);
	pcommand(p1, "set %s\n", param[1].val.string);
	return COM_OK;
}

/*
 * announce
 *
 * Usage: announce message
 *
 *   Broadcasts your message to all logged on users.  Announcements reach all
 *   users and cannot be censored in any way (such as by "set shout 0").
 */
int com_announce(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count;

	if (!printablestring(param[0].val.string)) {
		pprintf(p, _("Your message contains unprintable character(s).\n"));
		return COM_OK;
	}
	for (p1 = count = 0; p1 < player_globals.p_num; p1++) {
		if (p1 != p && player_globals.parray[p1].status == PLAYER_PROMPT){
			count++;
			pprintf_prompt(p1, _("\n\n    **ANNOUNCEMENT** from %s: %s\n\n"), 
						   pp->name, param[0].val.string);
		}
	}
	pprintf(p, _("\n(%d) **ANNOUNCEMENT** from %s: %s\n\n"), 
			count, pp->name, param[0].val.string);
	return COM_OK;
}

/*
 * annunreg
 *
 * Usage:  annunreg message
 *
 *   Broadcasts your message to all logged on unregistered users, and admins,
 *   too.  Announcements reach all unregistered users and admins and cannot be
 *   censored in any way (such as by "set shout 0").
 */
int com_annunreg(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count;

	if (!printablestring(param[0].val.string)) {
		pprintf(p, _("Your message contains unprintable character(s).\n"));
		return COM_OK;
	}
	for (p1 = count = 0; p1 < player_globals.p_num; p1++) {
		if (p1 != p && player_globals.parray[p1].status == PLAYER_PROMPT
			&& (!CheckPFlag(p1, PFLAG_REG) || check_admin(p1, ADMIN_ADMIN)) )
		{
			count++;
			pprintf_prompt(p1, _("\n\n    **UNREG ANNOUNCEMENT** from %s: %s\n\n"),
						   pp->name, param[0].val.string);
		}
	}
	pprintf(p, _("\n(%d) **UNREG ANNOUNCEMENT** from %s: %s\n\n"),
			count, pp->name, param[0].val.string);
	return COM_OK;
}

/*
 * asetadmin
 *
 * Usage: asetadmin player AdminLevel
 *
 *   Sets the admin level of the player with the following restrictions.
 *   1. You can only set the admin level of players lower than yourself.
 *   2. You can only set the admin level to a level that is lower than
 *      yourself.
 */
int com_asetadmin(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, connected, oldlevel;

	if (!FindPlayer(p, param[0].val.word, &p1, &connected))
		return COM_OK;

	if (!check_admin2(p, p1)) {
		pprintf(p, _("You can only set adminlevel for players below your adminlevel.\n"));
		if (!connected)
			player_remove(p1);
		return COM_OK;
	}
	if (!strcmp(player_globals.parray[p1].login, pp->login)) {
		pprintf(p, _("You can't change your own adminlevel.\n"));
		return COM_OK;
	}
	if (!check_admin(p, param[1].val.integer+1)) {
		pprintf(p, _("You can't promote someone to or above your adminlevel.\n"));
		if (!connected)
			player_remove(p1);
		return COM_OK;
	}
	oldlevel = player_globals.parray[p1].adminLevel;
	player_globals.parray[p1].adminLevel = param[1].val.integer;
	pprintf(p, _("Admin level of %s set to %d.\n"), 
			player_globals.parray[p1].name, player_globals.parray[p1].adminLevel);
	player_save(p1,0);

	if (connected)
		pprintf_prompt(p1, _("\n\n%s has set your admin level to %d.\n\n"), 
					   pp->name, player_globals.parray[p1].adminLevel);
	else
		player_remove(p1);

	return COM_OK;
}

/* ftell
 *
 * Usage: ftell user
 *
 *   This command forwards all further conversation between an admin and a
 *   user to all those listening in channel 0.
 *   It is unset as soon as the user logs off or ftell is typed without a
 *   parameter.
 */

int com_ftell(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1;
	char command[1024];

	if (param[0].type == TYPE_WORD) {

		if ((p1 = player_find_part_login(param[0].val.word)) < 0) {
			pprintf(p, _("%s isn't logged in.\n"), param[0].val.word);
			return COM_OK;
		}
	
		if (p1 == p) {
			pprintf(p, _("Nobody wants to listen to you talking to yourself! :-)\n"));
			return COM_OK;
		}
	
		if (pp->ftell != -1) {
			sprintf(command, _("tell 0 I will no longer be forwarding the conversation between *%s* and myself."), 
					player_globals.parray[pp->ftell].name);
			pcommand(p,command);
		}
	
		sprintf(command, _("tell 0 I will be forwarding the conversation between *%s* and myself to channel 0."), 
				player_globals.parray[p1].name);
		pcommand(p,command);
	
		pp->ftell = p1;
		return COM_OK_NOPROMPT;

	} else {
		if (pp->ftell != -1) {
			pprintf(p, _("Stopping the forwarding of the conservation with %s.\n"),
					player_globals.parray[pp->ftell].name);
			pcommand(p, _("tell 0 I will no longer be forwarding the conversation between *%s* and myself."),
					 player_globals.parray[pp->ftell].name);
			pp->ftell = -1;
			return COM_OK_NOPROMPT;
		} else
			pprintf(p, _("You were not forwarding a conversation.\n"));
	}

	return COM_OK;
}

/*
 * nuke
 *
 * Usage: nuke user
 *
 *   This command disconnects the user from the server.  The user is informed
 *   that she/he has been nuked by the admin named and a comment is
 *   automatically placed in the user's files (if she/he is a registered
 *   user, of course).
 */
int com_nuke(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, fd;

	if ((p1 = player_find_part_login(param[0].val.word)) < 0) {
		pprintf(p, _("%s isn't logged in.\n"), param[0].val.word);
		return COM_OK;
	}

	if (!check_admin2(p, p1)) {
		pprintf(p, _("You need a higher adminlevel to nuke %s!\n"),
				param[0].val.word);
		return COM_OK;
	}

	if (!check_admin(p, ADMIN_ADMIN) && CheckPFlag(p1, PFLAG_REG) ) {
		pprintf(p, _("You can't nuke %s. You can only nuke guests!\n"),
				param[0].val.word);
		return COM_OK;
	}

	pprintf(p, _("Nuking: %s\n"), param[0].val.word);
	/*
	 * no need for that anymore.
	pprintf(p, _("Please leave a comment explaining why %s was nuked.\n"), player_globals.parray[p1].name);
	pcommand(p, _("addcomment %s Nuked\n"), player_globals.parray[p1].name);
	*/
	pprintf(p1, _("\n\n**** You have been kicked out by %s! ****\n\n"), pp->name);

	fd = player_globals.parray[p1].socket;
	process_disconnection(fd);
	net_close_connection(fd);
	return COM_OK;
}

/*
 * admin
 *
 * Usage: admin
 *
 *   This command toggles your admin symbol (*) on/off.  This symbol appears
 *   in who listings.
 */
int com_admin(int p, param_list param)
{
	TogglePFlag(p, PFLAG_ADMINLIGHT);
	pprintf(p, CheckPFlag(p, PFLAG_ADMINLIGHT) ? _("Admin mode (*) is now shown.\n")
											   : _("Admin mode (*) is now not shown.\n"));
	return COM_OK;
}

/*
 * quota
 *
 * Usage: quota [n]
 *
 *   The command sets the number of seconds (n) for the shout quota, which
 *   affects only those persons on the shout quota list.  If no parameter
 *   (n) is given, the current setting is displayed.
 */
int com_quota(int p, param_list param)
{
	if (param[0].type == TYPE_NULL) {
		pprintf(p, _("The current shout quota is 2 shouts per %d seconds.\n"), 
				seek_globals.quota_time);
		return COM_OK;
	}
	seek_globals.quota_time = param[0].val.integer;
	pprintf(p, _("The shout quota is now 2 shouts per %d seconds.\n"), 
			seek_globals.quota_time);
	return COM_OK;
}

int com_realname(int p, param_list param)
{
	int p1;

	if ((p1 = player_find_part_login(param[0].val.word)) < 0) {
		pprintf(p, _("%s is not logged in.\n"), param[0].val.word);
		return COM_OK;
	}

	pprintf_prompt(p, _("%s real name is: %s\n"),
				   player_globals.parray[p1].name, player_globals.parray[p1].login);
	return COM_OK;
}

int com_invisible(int p, param_list param)
{
	return COM_OK;
}

/*
   force a server reload
*/
int com_areload(int p, param_list param)
{
	extern unsigned chessd_reload_flag;

	chessd_reload_flag = 1;

	InsertServerEvent( seSERVER_RELOAD, 0 );

	pprintf(p, _("Server reload started\n"));

	return COM_OK;
}
