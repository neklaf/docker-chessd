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

#include <string.h>
#include "globals.h"
#include "playerdb.h"
#include "utils.h"
#include "chessd_locale.h"
#include "lists.h"
#include "talkproc.h"
#include "gamedb.h"
#include "malloc.h"
#include "comproc.h"

const char * gl_tell_name[] =
    {"tells", "says", "whispers", "kibitzes", "channel", "ltells"};

static int on_channel(int ch, int p);

static int CheckShoutQuota(int p)
{
	struct player *pp = &player_globals.parray[p];
	int timeleft = time(0) - pp->lastshout_a;

	if ((timeleft < seek_globals.quota_time) && (pp->adminLevel == 0))
		return (seek_globals.quota_time - timeleft);

	return 0;
}

int com_shout(int p, param_list param)
{
	int p1, count = 0, timeleft = CheckShoutQuota(p);
	struct player *pp = &player_globals.parray[p];
	
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Only registered players can use the shout command.\n"));
		return COM_OK;
	}
	if (in_list(p, L_MUZZLE, pp->login)) {
		pprintf(p, _("You are muzzled.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_NULL) {
		if (timeleft)
			pprintf(p, _("Next shout available in %d seconds.\n"), timeleft);
		else
			pprintf(p, _("Your next shout is ready for use.\n"));
		return COM_OK;
	}
	if (timeleft) {
		pprintf(p, _("Shout not sent. Next shout in %d seconds.\n"), timeleft);
		return COM_OK;
	}
	
	pp->lastshout_a = pp->lastshout_b;
	pp->lastshout_b = time(0);
	if (!printablestring(param[0].val.string)) {
		pprintf(p, _("Your message contains some unprintable character(s).\n"));
		return COM_OK;
	}
	
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT ||
			!CheckPFlag(p1, PFLAG_SHOUT) || player_censored(p1, p))
			continue;
		
		/*
		 *	Control the shout:
		 */
		
		if (check_admin(p, ADMIN_ADMIN)) {
			count++;
			pprintf_prompt(p1, U_("\n%s shouts: %s\n"),
			pp->name,param[0].val.string);
		}
		else {
			if (on_channel(SHOUT_CHANNEL, p1)) {
				count++;
				pprintf_prompt(p1, U_("\n%s shouts: (ch %d) %s\n"),
				pp->name, SHOUT_CHANNEL, param[0].val.string);
			}
		}
		/* xboard */
	}
	
	if (check_admin(p, ADMIN_ADMIN)) {
		pprintf(p, U_("(%d) %s shouts: %s\n"), count, pp->name,  /* xboard */
				param[0].val.string);
	}
	else {
		pprintf(p, U_("(%d) %s shouts: (ch %d) %s\n"), count,
				pp->name,  /* xboard */
				SHOUT_CHANNEL, param[0].val.string);
	}
	
	if (!CheckPFlag(p, PFLAG_SHOUT))
		pprintf (p, _("You are not listening to shouts.\n"));
	if ((timeleft = CheckShoutQuota(p)))
		pprintf(p, _("Next shout in %d second(s).\n"), timeleft);
	
	return COM_OK;
}

int com_cshout(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count = 0;
	int reach_all = 0;
	
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Only registered players can use the cshout command.\n"));
		return COM_OK;
	}
	if (in_list(p, L_CMUZZLE, pp->login)) {
		pprintf(p, _("You are c-muzzled.\n"));
		return COM_OK;
	}
	if (!printablestring(param[0].val.string)) {
		pprintf(p, _("Your message contains some unprintable character(s).\n"));
		return COM_OK;
	}
	
	if (check_admin(p, ADMIN_HELPER) || in_list(p, L_MANAGER, pp->login))
		reach_all = 1;
	
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT ||
			!CheckPFlag(p1, PFLAG_CSHOUT) || player_censored(p1, p))
			continue;
		
		if (reach_all) {
			// managers and admin's c-shout reach every one
			count++;
			pprintf_prompt(p1, U_("\n%s c-shouts: %s\n"),
			pp->name,param[0].val.string);
		}
		else {
			if (on_channel(CSHOUT_CHANNEL, p1)) {
				count++;
				pprintf_prompt(p1, U_("\n%s c-shouts: (ch %d) %s\n"),
							   pp->name, CSHOUT_CHANNEL, param[0].val.string);
			}
		}
	}
	
	if (reach_all) {
		pprintf(p, U_("(%d) %s c-shouts: %s\n"), count, pp->name, /* xboard */
				param[0].val.string);
	}
	else {
		pprintf(p, U_("(%d) %s c-shouts: (ch %d) %s\n"), count,
				pp->name, /* xboard */
				CSHOUT_CHANNEL, param[0].val.string );
	}
	
	if (!CheckPFlag(p, PFLAG_CSHOUT))
		pprintf (p, _("You are not listening to c-shouts.\n"));
	
	return COM_OK;
}

int com_it(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count = 0, timeleft = CheckShoutQuota(p);
	
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Only registered players can use the it command.\n"));
		return COM_OK;
	}
	if (in_list(p, L_MUZZLE, pp->login)) {
		pprintf(p, _("You are muzzled.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_NULL) {
		if (timeleft)
			pprintf(p, _("Next shout available in %d seconds.\n"), timeleft);
		else
			pprintf(p, _("Your next shout is ready for use.\n"));
		return COM_OK;
	}
	if (timeleft) {
		pprintf(p, _("Shout not sent. Next shout in %d seconds.\n"), timeleft);
		return COM_OK;
	}
	pp->lastshout_a = pp->lastshout_b;
	pp->lastshout_b = time(0);
	
	if (!printablestring(param[0].val.string)) {
		pprintf(p, _("Your message contains some unprintable character(s).\n"));
		return COM_OK;
	}
	
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT ||
			!CheckPFlag(p1, PFLAG_SHOUT) || player_censored(p1, p))
			continue;
		count++;
		if (param[0].val.string[0] == '\'' ||
			param[0].val.string[0] == ',' ||
			param[0].val.string[0] == '.')
			pprintf_prompt(p1, "\n--> %s%s\n", pp->name, param[0].val.string);
		else
			pprintf_prompt(p1, "\n--> %s %s\n", pp->name, param[0].val.string);
	}
	if (param[0].val.string[0] == '\'' ||
		param[0].val.string[0] == ',' ||
		param[0].val.string[0] == '.')
		pprintf(p, "--> %s%s\n", pp->name, param[0].val.string);
	else
		pprintf(p, "--> %s %s\n", pp->name, param[0].val.string);
	
	if ((timeleft = CheckShoutQuota(p)))
		pprintf(p, _("Next shout in %d second(s).\n"), timeleft);
	
	return COM_OK;
}

static int tell(int p, int p1, const char *msg, int why, int ch)
{
	struct player *pp = &player_globals.parray[p];
	char tmp[MAX_LINE_SIZE];
	int rating, rating1, p2;

	if (!printablestring(msg)) {
		pprintf(p, _("Your message contains some unprintable character(s).\n"));

		pp->last_cmd_error = CMD_TELL_INVALID_CHAR;
		return COM_OK;
	}

	if (!CheckPFlag(p1, PFLAG_TELL) && !CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Player \"%s\" isn't listening to unregistered tells.\n"),
				player_globals.parray[p1].name);

		pp->last_cmd_error = CMD_TELL_NO_REG;
		return COM_OK;
	}
	if (player_censored(p1, p)) {
		if (pp->adminLevel == 0) {
			if (why != TELL_KIBITZ && why != TELL_WHISPER && why != TELL_CHANNEL)
				pprintf(p, _("Player \"%s\" is censoring you.\n"),
						player_globals.parray[p1].name);

			pp->last_cmd_error = CMD_TELL_CENSOR;

			return COM_OK;
		} else
			pprintf(p, _("Told \"%s\", who is censoring you.\n"),
					player_globals.parray[p1].name);
	}

	/*	Check which kind of tell is been issued
	*/

	switch (why) {
		case TELL_SAY:
			pprintf_highlight(p1, "\n%s", pp->name);
			pprintf_prompt(p1, U_(" says: %s\n"), msg); /* xboard */
			break;
		case TELL_WHISPER:
		case TELL_KIBITZ:
			rating = GetRating(&player_globals.parray[p], TYPE_BLITZ);
			rating1 = GetRating(&player_globals.parray[p], TYPE_STAND);

			if (rating < rating1)
				rating = rating1;
			if (in_list(p, L_FM, pp->name))
				pprintf(p1, "\n%s(FM)", pp->name);
			else if (in_list(p, L_IM, pp->name))
				pprintf(p1, "\n%s(IM)", pp->name);
			else if (in_list(p, L_GM, pp->name))
				pprintf(p1, "\n%s(GM)", pp->name);
			else if (in_list(p, L_WGM, pp->name))
				pprintf(p1, "\n%s(WGM)", pp->name);
			else if (pp->adminLevel >= 10
					&& CheckPFlag(p, PFLAG_ADMINLIGHT))
				pprintf(p1, "\n%s(*)", pp->name);
			else if (rating >= player_globals.parray[p1].kiblevel ||
					(pp->adminLevel >= 10
					 && CheckPFlag(p, PFLAG_ADMINLIGHT)))
				if (!CheckPFlag(p, PFLAG_REG))
					pprintf(p1, "\n%s(++++)", pp->name);
				else if (rating != 0)
					if (in_list(p, L_COMPUTER, pp->name))
						pprintf(p1, "\n%s(%d)(C)", pp->name, rating);
					else
						pprintf(p1, "\n%s(%d)", pp->name, rating);
				else
					pprintf(p1, "\n%s(----)", pp->name);
			else break;

			if (why == TELL_WHISPER)
				pprintf_prompt(p1, U_(" whispers: %s\n"), msg); /* xboard */
			else
				pprintf_prompt(p1, U_(" kibitzes: %s\n"), msg); /* xboard */

			break;
		case TELL_CHANNEL:
			pprintf(p1, "\n%s", pp->name);

			// admin level based tag
			switch (pp->adminLevel) {
			case ADMIN_HELPER:
				pprintf(p1, "(H)");
				break;
			case ADMIN_ADJUDICATOR:
				pprintf(p1, "(AD)");
				break;
			default:
				if ((pp->adminLevel >= ADMIN_ADMIN) &&
						CheckFlag(pp->Flags, PFLAG_ADMINLIGHT))
					pprintf(p1, "(*)");
			}

			pprintf_prompt(p1, "(%d): %s\n", ch, msg);
			break;
		case TELL_LTELL:
			pprintf_prompt(p1, U_("\n%s tells you: %s\n"), pp->name, msg); /* xboard */
			break;

		case TELL_TELL:
		default:
			if (player_globals.parray[p1].highlight)
				pprintf_highlight(p1, "\n%s", pp->name);
			else
				pprintf(p1, "\n%s", pp->name);

			// admin level based tag
			switch (pp->adminLevel) {
			case ADMIN_HELPER:
				pprintf(p1, "(H)");
				break;
			case ADMIN_ADJUDICATOR:
				pprintf(p1, "(AD)");
				break;
			default:
				if (pp->adminLevel >= ADMIN_ADMIN &&
						CheckFlag(pp->Flags, PFLAG_ADMINLIGHT))
					pprintf(p1, "(*)");
			}

			pprintf_prompt(p1, U_(" tells you: %s\n"), msg); /* xboard */

			/* let's test for forwarding of tells */

			if (pp->ftell == p1 || player_globals.parray[p1].ftell == p) {
				for (p2 = 0; p2 < player_globals.p_num; p2++) {
					// if the player is one of:
					// 	- the user issuing the tell
					// 	- the user forwarding the tell
					// then, it won't get the forwarded message
					if (p2 == p || p2 == p1 ||
							player_globals.parray[p2].status != PLAYER_PROMPT)
						continue;

					if (on_channel(0, p2))
						pprintf(p2, _("\nFwd tell: %s told %s: %s\n"), pp->name,
								player_globals.parray[p1].name, msg);
				}
			}

			break;
	}// end switch

	tmp[0] = '\0';

	if (!(player_globals.parray[p1].busy == NULL))
		sprintf(tmp, _(", who %s (idle: %s)"), player_globals.parray[p1].busy,
				hms_desc(player_idle(p1)));
	else
		if (((player_idle(p1) % 3600) / 60) > 2)
			sprintf(tmp, _(", who has been idle %s"), hms_desc(player_idle(p1)));

	if ((why == TELL_SAY) || (why == TELL_TELL) || (why == TELL_LTELL)) {
		pprintf(p, _("(told %s%s)\n"), player_globals.parray[p1].name,
				(((player_globals.parray[p1].game>=0) && (game_globals.garray[player_globals.parray[p1].game].status == GAME_EXAMINE))
				 ? _(", who is examining a game") :
				 ((player_globals.parray[p1].game>=0) && (game_globals.garray[player_globals.parray[p1].game].status == GAME_SETUP))
				 ? _(", who is setting up a position") :
				 (player_globals.parray[p1].game >= 0 && (player_globals.parray[p1].game != pp->game))
				 ? _(", who is playing") : tmp));
	}

	if (why == TELL_TELL || why == TELL_SAY) {
		FREE(pp->last_tell);
		pp->last_tell = strdup(player_globals.parray[p1].login);
	}

	return COM_OK;
}

int com_ptell(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p], *pp1;
	char tmp[MAX_LINE_SIZE];
	int p1;
	
	if (pp->partner < 0) {
		pprintf (p, _("You do not have a partner at present.\n"));
		return COM_OK;
	}
	
	p1 = pp->partner;
	pp1 = &player_globals.parray[p1];
	if (p1 < 0 || pp1->status == PLAYER_PASSWORD || pp1->status == PLAYER_LOGIN) {
		pprintf(p, _("Your partner is not logged in.\n"));
		return COM_OK;
	}
	
	if (pp1->highlight)
		pprintf_highlight(p1, "\n%s", pp->name);
	else
		pprintf(p1, "\n%s", pp->name);
	
	pprintf_prompt(p1, U_(" (your partner) tells you: %s\n"), param[0].val.string); /* xboard */
	tmp[0] = '\0';
	if (pp1->busy != NULL)
		sprintf(tmp, _(", who %s (idle: %s)"), pp1->busy,
				hms_desc(player_idle(p1)));
	else
		if (((player_idle(p1) % 3600) / 60) > 2)
			sprintf(tmp, _(", who has been idle %s"), hms_desc(player_idle(p1)));
	
	pprintf(p, _("(told %s%s)\n"), pp1->name,
			((pp1->game>=0 && game_globals.garray[pp1->game].status == GAME_EXAMINE)
			? _(", who is examining a game") :
			(pp1->game>=0 && game_globals.garray[pp1->game].status == GAME_SETUP)
			? _(", who is setting up a position") :
			(pp1->game >= 0 && pp1->game != pp->game)
			? _(", who is playing") : tmp));
	
	return COM_OK;
}

static int chtell(int p, int ch, char *msg)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count = 0, muzzle_list=-1;

	if (in_list(p, L_CHMUZZLE , pp->login)) {
		pprintf(p, _("You have been prevented from sending tells to any channel.\n"));
		return COM_OK;
	}

	if ((ch == 0) && (pp->adminLevel < ADMIN_ADMIN)) {
		pprintf(p, _("Only admins may send tells to channel 0.\n"));
		return COM_OK;
	}

	// no more bull* from unregistered plagues
	if (!CheckPFlag(p, PFLAG_REG)) {
	  if (ch != HELP_CHANNEL) {
		pprintf(p, _("Guests and users under approval can only talk in channel %d\n"), HELP_CHANNEL);
		return COM_OK;
	  }
	}

	// gabrielsan: the master channel;
	if (ch == MASTER_CHANNEL) {
		if (!check_admin(p, ADMIN_ADMIN)) {
			pprintf(p, _("You cannot talk in channel %d\n"), MASTER_CHANNEL);
		}
		else {
			pprintf(p,
					_("You cannot talk in %d channel, you can only hear it\n"),
					MASTER_CHANNEL);
		}
		return COM_OK;
	}

	// gabrielsan: helper channel
	if ((ch == HELPER_CHANNEL) && (pp->adminLevel < ADMIN_HELPER)) {
		pprintf(p, _("Only admins or helpers may send tells to channel %d.\n"),
				HELPER_CHANNEL);
		return COM_OK;
	}

	// manager channel.. just because it is really easy to program
	if ((ch == MANAGER_CHANNEL) && !in_list(p,L_MANAGER,pp->name) &&
			!check_admin(p, ADMIN_HELPER)) {
		pprintf(p, _("Only managers, admins or helpers may send tells "
					 "to channel %d.\n"), MANAGER_CHANNEL);
		return COM_OK;
	}

	if (ch < 0) {
		pprintf(p, _("The lowest channel number is 0.\n"));
		return COM_OK;
	}else if (ch >= MAX_CHANNELS) {
		pprintf(p, _("The maximum channel number is %d.\n"), MAX_CHANNELS - 1);
		return COM_OK;
	}

	switch (ch) {
		case  1: muzzle_list = L_C1MUZZLE;  break;
		case  2: muzzle_list = L_C2MUZZLE;  break;
		case 24: muzzle_list = L_C24MUZZLE; break;
		case 46: muzzle_list = L_C46MUZZLE; break;
		case 49: muzzle_list = L_C49MUZZLE; break;
		case 50: muzzle_list = L_C50MUZZLE; break;
		case 51: muzzle_list = L_C51MUZZLE; break;
	}
	if (muzzle_list != -1 ) {
		if ((in_list(p, muzzle_list, pp->login))) {
			pprintf (p, _("You have been prevented from sending tells to"
						  " channel %d.\n"), ch);
			return COM_OK;
		}
	}

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT)
			continue;
		if (on_channel(ch, p1) && !player_censored(p1, p)
				&& (CheckPFlag(p, PFLAG_REG) || CheckPFlag(p1, PFLAG_TELL))) {
			tell(p, p1, msg, TELL_CHANNEL, ch);
			if (!player_censored(p1, p))
				count++;
		}
		else {
			// if the user does is not listening to channel and is admin,
			// she/he'll get the forwarded msg
			// 	- gabrielsan

			if (on_channel(MASTER_CHANNEL, p1) &&
					check_admin(p1, ADMIN_ADMIN))
			{
				pprintf_prompt(p1,
						U_("Master Channel(%d): In channel %d %s says: %s\n"),
						MASTER_CHANNEL,
						ch,
						pp->name,
						msg
						);
			}
		}
	}

	if (count)
		pp->last_channel = ch;

	pprintf(p, "%s(%d): %s\n", pp->name, ch, msg);
	if (!on_channel(ch, p))
		pprintf(p, _(" (You're not listening to channel %d.)\n"), ch);

	return COM_OK;
}

int com_whisper(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, p1, count = 0;
	
	if (!pp->num_observe && pp->game < 0) {
		pprintf(p, _("You are not playing or observing a game.\n"));
		return COM_OK;
	}
	if (!CheckPFlag(p, PFLAG_REG) && pp->game == -1) {
		pprintf(p, _("You must be registered to whisper other people's games.\n"));
		return COM_OK;
	}
	g = pp->game >= 0 ? pp->game : pp->observe_list[0];
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT)
			continue;
		if (player_is_observe(p1, g) ||
			(game_globals.garray[g].link >= 0 && player_is_observe(p1, game_globals.garray[g].link)
			&& player_globals.parray[p1].game != game_globals.garray[g].link))
		{
			tell(p, p1, param[0].val.string, TELL_WHISPER, 0);
			if ((pp->adminLevel >= ADMIN_ADMIN) || !game_globals.garray[g].private 
				|| !player_censored(p1, p))
				count++;
		}
	}
	pprintf(p, _("whispered to %d.\n"), count);
	return COM_OK;
}

int com_kibitz(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, p1, count = 0;
	
	if (!pp->num_observe && pp->game < 0) {
		pprintf(p, _("You are not playing or observing a game.\n"));
		return COM_OK;
	}
	if (!CheckPFlag(p, PFLAG_REG) && pp->game == -1) {
		pprintf(p, _("You must be registered to kibitz other people's games.\n"));
		return COM_OK;
	}
	g = pp->game >= 0 ? pp->game : pp->observe_list[0];
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT)
			continue;
		if ((player_is_observe(p1, g) || player_globals.parray[p1].game == g
			|| (game_globals.garray[g].link >= 0
			&& (player_is_observe(p1, game_globals.garray[g].link)
			|| player_globals.parray[p1].game == game_globals.garray[g].link)))
			&& CheckPFlag(p1, PFLAG_KIBITZ))
		{
			tell(p, p1, param[0].val.string, TELL_KIBITZ, 0);
			if (pp->adminLevel >= ADMIN_ADMIN || !game_globals.garray[g].private 
				|| player_globals.parray[p1].game == g || !player_censored(p1, p))
				count++;
		}
	}
	pprintf(p, _("kibitzed to %d.\n"), count);
	return COM_OK;
}

/*
   like tell() but takes a player name
*/
static int tell_s(int p, const char *who, const char *msg, int why, int ch)
{
	int p1 = player_find_part_login(who);
	if (p1 < 0 || player_globals.parray[p1].status == PLAYER_PASSWORD
	    || player_globals.parray[p1].status == PLAYER_LOGIN) {
		if (why != TELL_LTELL) {
			pprintf(p, _("No user named '%s' is logged in.\n"), who);
			player_globals.parray[p].last_cmd_error = CMD_NO_USER;
		}
		return COM_OK;
	}
	return tell(p, p1, msg, why, ch);
}

int com_tell(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	char *who;

	if (param[0].type == TYPE_NULL)
		return COM_BADPARAMETERS;
	else if (param[0].type != TYPE_WORD)
		return chtell(p, param[0].val.integer, param[1].val.string);

	who = param[0].val.word;
	stolower(who);

	if (strcmp(who, ".") == 0) {
		if (!pp->last_tell) {
			pprintf(p, _("No previous tell.\n"));
			return COM_OK;
		}
		who = pp->last_tell;
	}

	if (strcmp(who, ",") == 0) {
		if (pp->last_channel == -1) {
			pprintf(p, _("No previous channel.\n"));
			return COM_OK;
		}
		return chtell(p, pp->last_channel, param[1].val.string);
	}

	return tell_s(p, who, param[1].val.string, TELL_TELL, 0);
}


/* tell all players in a named list */
int com_ltell(int p, param_list param)
{
	struct List *gl;
	int i;

	gl = list_findpartial(p, param[0].val.word, 0);
	if (gl) {
		for (i=0;i<gl->numMembers;i++)
			tell_s(p, gl->m_member[i], param[1].val.string, TELL_LTELL, 0);
	}else
		pprintf(p,_("Can't find that list\n"));

	return COM_OK;
}


int com_xtell(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1;
	char *msg, tmp[2048];

	msg = param[1].val.string;
	p1 = player_find_part_login(param[0].val.word);
	if ((p1 < 0) || (player_globals.parray[p1].status == PLAYER_PASSWORD)
			|| (player_globals.parray[p1].status == PLAYER_LOGIN)) {
		pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
		pp->last_cmd_error = CMD_NO_USER;
		return COM_OK;
	}
	if (!printablestring(msg)) {
		pprintf(p, _("Your message contains some unprintable character(s).\n"));
		pp->last_cmd_error = CMD_TELL_INVALID_CHAR;
		return COM_OK;
	}
	if (!CheckPFlag(p1, PFLAG_TELL) && !CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Player \"%s\" isn't listening to unregistered tells.\n"),
				player_globals.parray[p1].name);
		pp->last_cmd_error = CMD_TELL_NO_REG;
		return COM_OK;
	}
	if ((player_censored(p1, p)) && (pp->adminLevel == 0)) {
		pprintf(p, _("Player \"%s\" is censoring you.\n"),
				player_globals.parray[p1].name);
		pp->last_cmd_error = CMD_TELL_CENSOR;
		return COM_OK;
	}

	if (player_globals.parray[p1].highlight)
		pprintf_highlight(p1, "\n%s", pp->name);
	else
		pprintf(p1, "\n%s", pp->name);

	pprintf_prompt(p1, U_(" tells you: %s\n"), msg); /* xboard */

	tmp[0] = '\0';
	if (!(player_globals.parray[p1].busy == NULL))
		sprintf(tmp, _(", who %s (idle: %s)"), player_globals.parray[p1].busy,
				hms_desc(player_idle(p1)));
	else
		if (((player_idle(p1) % 3600) / 60) > 2)
			sprintf(tmp, _(", who has been idle %s"), hms_desc(player_idle(p1)));

	pprintf(p, _("(told %s%s)\n"), player_globals.parray[p1].name,
			(((player_globals.parray[p1].game>=0) && (game_globals.garray[player_globals.parray[p1].game].status == GAME_EXAMINE))
			 ? _(", who is examining a game") :
			 ((player_globals.parray[p1].game>=0) && (game_globals.garray[player_globals.parray[p1].game].status == GAME_SETUP))
			 ? _(", who is setting up a position") :
			 (player_globals.parray[p1].game >= 0 && (player_globals.parray[p1].game != pp->game))
			 ? _(", who is playing") : tmp));
	return COM_OK;
}

int com_say(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	if (pp->opponent < 0) {
		if (!pp->last_opponent) {
			pprintf(p, _("No one to say anything to, try tell.\n"));
			return COM_OK;
		}
		return tell_s(p, pp->last_opponent, param[0].val.string, TELL_SAY, 0);
	}
	return tell(p, pp->opponent, param[0].val.string, TELL_SAY, 0);
}

int com_inchannel(int p, param_list param)
{
	int p1,count = 0;
	char tmp[18];

	if (param[0].type == TYPE_NULL) {
		pprintf (p,_("inchannel [no params] has been removed\n"
					 "Please use inchannel [name] or inchannel [number]\n"));
		player_globals.parray[p].last_cmd_error = CMD_NO_USER;
		return COM_OK;
	}

	if (param[0].type == TYPE_WORD) {
		p1 = player_find_part_login(param[0].val.word);
		if ((p1 < 0) || (player_globals.parray[p1].status != PLAYER_PROMPT)) {
			pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
			return COM_OK;
		}
		pprintf (p,_("%s is in the following channels:"),player_globals.parray[p1].name);
		if (list_channels (p,p1))
			pprintf (p, _(" No channels found for %s.\n"),player_globals.parray[p1].name);
		return COM_OK;
	} else if (param[0].val.integer >= MAX_CHANNELS) {
		pprintf (p, _("No such channel; the largest channel number is %d.\n"),
				MAX_CHANNELS - 1);
		return COM_OK;
	} else {
		sprintf(tmp, "%d", param[0].val.integer);
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (player_globals.parray[p1].status != PLAYER_PROMPT)
				continue;
			if (in_list(p1,L_CHANNEL,tmp)) {
				if (!count)
					pprintf(p, _("Channel %d:"),param[0].val.integer);
				pprintf(p, " %s", player_globals.parray[p1].name);
				if (player_globals.parray[p1].adminLevel >= 10
						&& CheckPFlag(p1, PFLAG_ADMINLIGHT)
						&& param[0].val.integer < 2)
					pprintf(p, "(*)");
				count++;
			}
		}
		if (!count)
			pprintf(p, _("Channel %d is empty.\n"), param[0].val.integer);
		else
			pprintf(p, ngettext("\n%d person is in channel %d.\n",
						"\n%d people are in channel %d.\n",
						count), count, param[0].val.integer);
		return COM_OK;
	}
}

static int com_sendmessage(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, connected = 1;
	
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("You are not registered and cannot send messages.\n"));
		return COM_OK;
	}
	
	if (!FindPlayer(p, param[0].val.word, &p1, &connected))
		return COM_OK;
	
	if (!CheckPFlag(p1, PFLAG_REG)) {
		pprintf(p, _("Player \"%s\" is unregistered and cannot receive messages.\n"),
		player_globals.parray[p1].name);
		return COM_OK;
	}
	
	if ((player_censored(p1, p)) && (pp->adminLevel == 0)) {
		pprintf(p, _("Player \"%s\" is censoring you.\n"), player_globals.parray[p1].name);
		if (!connected)
			player_remove(p1);
		return COM_OK;
	}
	if (player_add_message(p1, p, param[1].val.string)) {
		pprintf(p, _("Couldn't send message to %s. Message buffer full.\n"),
		player_globals.parray[p1].name);
	} else {
		if (connected) {
			pprintf(p1, U_("\n%s just sent you a message:\n"), pp->name); /* xboard */
			pprintf_prompt(p1, "    %s\n", param[1].val.string);
		}
	}
	if (!connected)
		player_remove(p1);
	return COM_OK;
}

int com_messages(int p, param_list param)
{
	int start;
	
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf (p, _("Unregistered players may not send or receive messages.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_NULL) {
		player_show_messages (p);
	} else if (param[0].type == TYPE_WORD) {
		if (param[1].type != TYPE_NULL)
			return com_sendmessage(p, param);
		else
			ShowMsgsBySender(p, param);
	} else {
		start = param[0].val.integer;
		ShowMsgRange(p, start, start);
	}
	return COM_OK;
}

int com_forwardmess(int p, param_list param)
{
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Unregistered players may not send or receive messages.\n"));
		return COM_OK;
	}
	ForwardMsgRange(param[0].val.word, p, param[1].val.integer, param[1].val.integer);
	return COM_OK;
}

int com_clearmessages(int p, param_list param)
{
	if (player_num_messages(p) <= 0) {
		pprintf(p, U_("You have no messages.\n")); /* xboard */
		return COM_OK;
	}
	if (param[0].type == TYPE_NULL) {
		pprintf(p, _("Messages cleared.\n"));
		player_clear_messages(p);
	} else if (param[0].type == TYPE_WORD)
		ClearMsgsBySender(p, param);
	else if (param[0].type == TYPE_INT)
		ClrMsgRange(p, param[0].val.integer, param[0].val.integer);
	
	return COM_OK;
}

int com_znotify(int p, param_list param)
{
	int p1, count = 0;
	
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_notified(p, p1) && player_globals.parray[p1].status == PLAYER_PROMPT) {
			if (!count)
				pprintf(p, _("Present company on your notify list:\n  "));
			pprintf(p, " %s", player_globals.parray[p1].name);
			count++;
		}
	}
	if (!count)
		pprintf(p, _("No one from your notify list is logged on."));
	pprintf(p, "\n");
	
	count = 0;
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_notified(p1, p) && player_globals.parray[p1].status == PLAYER_PROMPT) {
			if (!count)
				pprintf(p,
					_("The following players have you on their notify list:\n  "));
			pprintf(p, " %s", player_globals.parray[p1].name);
			count++;
		}
	}
	if (!count)
		pprintf(p, _("No one logged in has you on their notify list."));
	pprintf(p, "\n");
	
	return COM_OK;
}

int com_qtell(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1;
	char tmp[MAX_STRING_LENGTH];
	char dummy[2];
	char buffer1[MAX_STRING_LENGTH];	/* no highlight and no bell */
	char buffer2[MAX_STRING_LENGTH];	/* no highlight and bell */
	char buffer3[MAX_STRING_LENGTH];	/* highlight and no bell */
	char buffer4[MAX_STRING_LENGTH];	/* highlight and and bell */
	char *q;
	int i,count;
	
	if (!in_list(p, L_TD, pp->name)) {
		pprintf(p, _("Only TD programs are allowed to use this command.\n"));
		return COM_OK;
	}
	strcpy(buffer1, ":\0");
	strcpy(buffer2, ":\0");
	strcpy(buffer3, ":\0");
	strcpy(buffer4, ":\0");

	for (q = param[1].val.string, count = 0; *q && count < MAX_STRING_LENGTH-5;) {
		if (q[0] == '\\' && q[1] == 'n') {
			strcat(buffer1, "\n:");
			strcat(buffer2, "\n:");
			strcat(buffer3, "\n:");
			strcat(buffer4, "\n:");
			count += 2;
			q += 2;
		} else if (q[0] == '\\' && q[1] == 'b') {
			strcat(buffer2, "\007");
			strcat(buffer4, "\007");
			count++;
			q += 2;
		} else if (q[0] == '\\' && q[1] == 'H') {
			strcat(buffer3, "\033[7m");
			strcat(buffer4, "\033[7m");
			count += 4;
			q += 2;
		} else if (q[0] == '\\' && q[1] == 'h') {
			strcat(buffer3, "\033[0m");
			strcat(buffer4, "\033[0m");
			count += 4;
			q += 2;
		} else {
			dummy[0] = q[0];
			dummy[1] = '\0';
			strcat(buffer1, dummy);
			strcat(buffer2, dummy);
			strcat(buffer3, dummy);
			strcat(buffer4, dummy);
			count++;
			q++;
		}
	}
	
	if (param[0].type == TYPE_WORD) {
		if ((p1 = player_find_bylogin(param[0].val.word)) < 0) {
			pprintf(p, "*qtell %s 1*\n", param[0].val.word);
			return COM_OK;
		}
		pprintf_prompt(p1, "\n%s\n", (player_globals.parray[p1].highlight && CheckPFlag(p1, PFLAG_BELL)) ? buffer4 :
			(player_globals.parray[p1].highlight && !CheckPFlag(p1, PFLAG_BELL)) ? buffer3 :
			(!player_globals.parray[p1].highlight && CheckPFlag(p1, PFLAG_BELL)) ? buffer2 :
			buffer1);
		pprintf(p, "*qtell %s 0*\n", player_globals.parray[p1].name);
	} else {
		int p1, ch = param[0].val.integer;
		
		if (ch == 0 || ch < 0 || ch >= MAX_CHANNELS) {
			pprintf(p, "*qtell %d 1*\n", param[0].val.integer);
			return COM_OK;
		}
		sprintf (tmp,"%d",param[0].val.integer);
		for (p1 = 0; p1 < player_globals.p_num ; p1++) {
			if (p1 == p || player_censored(p1, p) 
				|| player_globals.parray[p1].status != PLAYER_PROMPT)
				continue;
			if (in_list(p1,L_CHANNEL,tmp))
				pprintf_prompt(p1, "\n%s\n", (player_globals.parray[p1].highlight && CheckPFlag(p1, PFLAG_BELL)) ? buffer4 :
					(player_globals.parray[p1].highlight && !CheckPFlag(p1, PFLAG_BELL)) ? buffer3 :
					(!player_globals.parray[p1].highlight && CheckPFlag(p1, PFLAG_BELL)) ? buffer2 :
					buffer1);
		}
		pprintf(p, "*qtell %d 0*\n", param[0].val.integer);
	}
	return COM_OK;
}

static int on_channel(int ch, int p)
{
	char tmp[10];  /* 9 digits ought to be enough :) */

	sprintf(tmp, "%d", ch);
	return in_list(p, L_CHANNEL, tmp);
	/* since needs ch converted to a string kept hidden from view */
}
