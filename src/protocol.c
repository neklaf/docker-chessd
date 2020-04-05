/*
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

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "malloc.h"
#include "utils.h"
#include "config.h"
#include "globals.h"

#include "matchproc.h"
#include "playerdb.h"
#include "gamedb.h"
#include "chessd_locale.h"
#include "pending.h"

#include "protocol.h"

/*
 * This module encapsulates the protocol implementation
 */

void  protocol_init_userlist(int p, int num)
{
	pprintf(p, "<userlist> uc=%d\n", num);
}

void protocol_send_user_info(
			int p,
			char* player_name,	// username
			int isRegistered,	// is registered
			int isOpen,			// is open for match requests
			int playingRated, 	// is playing rated games
			int playingGame, 	// playing in which game?

			// stat information
			struct statistics *user_stats, int stat_count,

			char* p1Attrs, 		// special attributes (earned from lists)
			int onlineTime,		// online time
			int idleTime 		// idle time
		)
{
	int i;
	char stat_string[255];
	char temp_stat[255];

	// create the stat string
	strcpy(stat_string, "");
	for (i=0; i < stat_count; i++) {
		sprintf(temp_stat, "stat=%s,%d,%d,%d,%d ",
				user_stats[i].style_name,
				user_stats[i].rating,
				user_stats[i].win,
				user_stats[i].los,
				user_stats[i].dra );
		strcat(stat_string, temp_stat);
	}

	pprintf_prompt(p,
		"<ui> username=%s R=%d OP=%d pR=%d pG=%d OT=%d IT=%d At='%s' "
		"%s "
		"</ui>\n",
		player_name,		// username
		isRegistered?1:0,	// is registered
		isOpen?1:0,			// is open for match requests
		playingRated?1:0,	// is playing rated games
		playingGame+1, 		// playing in which game?
		onlineTime,			// online time
		idleTime, 			// idle time
		p1Attrs, 			// special attributes (earned from lists)

		// stat information
		stat_string
	);

}

void  protocol_end_userlist(int p)
{
	pprintf_prompt(p, "</userlist>\n");
}

void protocol_user_online_status(int p,
		char* username,
		char* userIP,
		int sendIP,
		int registered,
		int inout //0: in 1: out
		)
{
	if (inout == 0) {
		if (sendIP) {
			pprintf_prompt(p, U_("\n[%s (%s: %s) has connected.]\n"),
					username, (CheckPFlag(p, PFLAG_REG) ? "R" : "U"), userIP);
		}
		else
			pprintf_prompt(p, U_("\n[%s has connected.]\n"), username);
	}
	else
		pprintf_prompt(p, U_("\n[%s has disconnected.]\n"), username);
}


/// matches messages
void protocol_challenge(int p,
		int issuing,
		char *category,
		char *opp1Name,
		char *opp1Rating,
		char *opp2Name,
		char *opp2Rating,
		int opponentColor,
		int whiteTime,
		int whiteInc,
		int blackTime,
		int blackInc,
		int deltaWin,
		int deltaLos,
		int deltaDra
		)
{
	char *oppColor;

	switch(opponentColor) {
		case WHITE: oppColor = "White"; break;
		case BLACK: oppColor = "Black"; break;
		default:    oppColor = "auto";
	}

	pprintf_prompt(p, "<ch> I=%d op1=%s,%s op2=%s,%s C=%s wtc=%d,%d btc=%d,%d D=%d,%d,%d</ch>",
			issuing?1:0,
			opp1Name, opp1Rating, opp2Name, opp2Rating, oppColor, whiteTime, whiteInc,
			blackTime, blackInc, deltaWin, deltaLos, deltaDra );
}

void protocol_match_answer(int p,
		char *who,
		int decision) {
	pprintf_prompt(p, "<ma> opponent=%s action=%s </ma>\n",
				   who, decision ? "accept" : "decline");
}

void protocol_pending_status(int p,	// sending to.. (receiver)
		char *whofrom,				//
		char *whoto,				// related user
		int pendency_type,			// what sort of pendency
		int status)					// added/removed
{
	char *statusStr = status ? "added" : "removed";

	if (player_globals.parray[p].ivariables.ext_protocol)
		pprintf_prompt(p, "<pe> from=%s to=%s type=%s status=%s </pe>\n",
					   whofrom, whoto, pendTypes[pendency_type], statusStr);
}

void protocol_game_info(
		int p,
		int game_number,
		const char* category,
		int rated,
		int whiteReg,
		int blackReg,
		int whiteTime,
		int whiteInc,
		int blackTime,
		int blackInc,
		int partnerGame, // for bughouse games
		int whiteRating,
		int blackRating,
		int whiteTimeseal,
		int blackTimeseal )
{
	if (player_globals.parray[p].ivariables.ext_protocol)
		pprintf(p,
		"<g1> %d p=%d t=%s r=%d u=%d,%d it=%d,%d i=%d,%d pt=%d rt=%d,%d ts=%d,%d m=2 n=0\n",
			game_number, 0, category, rated, whiteReg, blackReg,
			whiteTime, whiteInc, blackTime, blackInc, partnerGame,
			whiteRating, blackRating, whiteTimeseal, blackTimeseal );
}

void protocol_challenge_notify(
		int p,
		int issuing,			// is p the challenger?
		int update,				// this is p's update
		char *challenger,		// challenger name
		char *chgerRating,		// challenger rating
		char *challenged,		// challenged name
		char *chgedRating,		// challenged rating
		const char *chgedColor,	// challenged color
		char *category,			// category
		char *board,			// board
		int rated,				// is the game rated?
		int wt,					// white time
		int wi,					// white inc
		int bt,					// black time
		int bi,					// black inc
		const char* adjourned,	// is the game adjourned?
		struct delta_rating_info p_info // information about changes in p's rating
		)
{
	struct player *pp = &player_globals.parray[p];

	if (update) {
		if (issuing) {
			pprintf(p, "\n");
			pprintf(p, _("Updating match request to: "));
		}
		else
			pprintf(p, _("\n%s updates the match request.\n"), challenger);
	}
	pprintf(p, "\n");
	pprintf(p, issuing ? U_("Issuing: ") : U_("Challenge: "));

	pprintf(p, "%s (%s) %s", challenger, chgerRating, chgedColor);
	pprintf_highlight(p, "%s", challenged);

	pprintf(p, " (%s) %s%s.\n", chgedRating,
			game_str(rated, wt * 60, wi, bt * 60, bi, category, board),
			adjourned);

	if (pp->ivariables.ext_protocol) {
		// send the information in the new protocol way
		pprintf(p, "<ch> I=%d from=%s to=%s fr=%s tr=%s C=%s gt=%s r=%d w=%d,%d b=%d,%d a=%d ",
				issuing, challenger, challenged, chgerRating, chgedRating,
				strcmp(chgedColor, "")?chgedColor:"auto",
				getGameType(wt * 60, wi, bt * 60, bi, category, board), rated, wt, wi, bt, bi,
				!strcmp(adjourned, "adjourned") );
		if (rated)
			pprintf(p, "w=%d l=%d d=%d rd=%5.1f ",
					p_info.win, p_info.los, p_info.dra, p_info.sterr );
		pprintf_prompt(p, "</ch>\n");
	}
}
