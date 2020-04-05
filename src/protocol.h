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

#ifndef PROTOCOL_MODULE_H
#define PROTOCOL_MODULE_H

#include "matchproc.h"


/*
 * Functions to send user list
 *
 */
void  protocol_init_userlist(int p, int num);

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
		);

void  protocol_end_userlist(int p);

void protocol_user_online_status(int p,
		char* username,
		char* userIP,
		int sendIP,
		int registered,
		int inout //0: in 1: out
		);

// match messages
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
		);


void protocol_match_answer(int p,
		char *who,
		int decision);

void protocol_pending_status(int p,	// sending to.. (receiver)
		char *whofrom,				//
		char *whoto,				// related user
		int pendency_type,			// what sort of pendency
		int status); 				// added/removed

/*
 * Send game information
 *
 */
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
		int partnerGame,// for bughouse games
		int whiteRating,
		int blackRating,
		int whiteTimeseal,
		int blackTimeseal
		);

/*
 * send challenge notification
 *
 */
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
		);

#endif
