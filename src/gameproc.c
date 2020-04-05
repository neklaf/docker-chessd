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

#include "config.h"

#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

#include "configuration.h"
#include "setup.h"
#include "gamedb.h"
#include "globals.h"
#include "playerdb.h"
#include "comproc.h"
#include "chessd_locale.h"
#include "utils.h"
#include "ratings.h"
#include "pending.h"
#include "movecheck.h"
#include "malloc.h"
#include "matchproc.h"
#include "obsproc.h"
#include "gameproc.h"
#include "network.h"

/*Let users know that someone is available (format the message)*/
static void getavailmess(int p, char* message)
{
	struct player *pp = &player_globals.parray[p];
	char titles[100];

	titles[0] = '\0';
	AddPlayerLists(p, titles);
	sprintf( message, _("%s%s Blitz (%s), Std (%s), Wild (%s), Light (%s), Bug (%s)\n"
						"  is now available for matches."),
			 pp->name, titles,
			 ratstrii(pp->b_stats.rating, p),
			 ratstrii(pp->s_stats.rating, p),
			 ratstrii(pp->w_stats.rating, p),
			 ratstrii(pp->l_stats.rating, p),
			 ratstrii(pp->bug_stats.rating, p) );
}

void getnotavailmess(int p, char* message)
{
	char titles[100];

	titles[0] = '\0';
	AddPlayerLists(p, titles);
	sprintf(message, _("%s%s is no longer available for matches."),
			player_globals.parray[p].name, titles);
}

void filter_avail(int p, char *msg){
	struct player *pp = &player_globals.parray[p];
	int p1;

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if ( p != p1 && player_globals.parray[p1].status == PLAYER_PROMPT
		  && CheckPFlag(p1, PFLAG_AVAIL) 
		  && CheckPFlag(p1, PFLAG_OPEN)
		  && player_globals.parray[p1].game < 0
		  && ((pp->b_stats.rating <= player_globals.parray[p1].availmax &&
			   pp->b_stats.rating >= player_globals.parray[p1].availmin) 
			 || !player_globals.parray[p1].availmax ) )
			pprintf_prompt(p1,"\n%s\n",msg);
	}
}

void announce_avail(int p)
{
	char avail[200];

	if (player_globals.parray[p].game < 0 && CheckPFlag(p, PFLAG_OPEN)) {
		getavailmess(p, avail);
		filter_avail(p, avail);
	}
}

void announce_notavail(int p)
{
	char avail[200];

	getnotavailmess(p, avail); // BUG? do we need to add checks per above routine?
	filter_avail(p, avail);
}

void game_ended(int g, int winner, int why)
{
	struct game *gg = &game_globals.garray[g];
	char outstr[200];
	char avail_black[200]; /* for announcing white/black avail */
	char avail_white[200];
	char avail_bugwhite[200];
	char avail_bugblack[200];
	char tmp[200];
	int p;
	int gl = gg->link;
	int rate_change = 0;
	int isDraw = 0;
	int whiteResult;
	char winSymbol[10];
	char EndSymbol[10];
	char *NameOfWinner, *NameOfLoser;
	int beingplayed = 0;		/* i.e. it wasn't loaded for adjudication */
	int print_avail = 0;

	avail_white[0] = '\0';
	avail_black[0] = '\0';
	avail_bugwhite[0] = '\0';
	avail_bugblack[0] = '\0';

	if (gg->black != -1 && gg->white != -1)
		beingplayed = (player_globals.parray[gg->black].game == g);

	sprintf(outstr, _("\n{Game %d (%s vs. %s) "), g + 1,
			player_globals.parray[gg->white].name,
			player_globals.parray[gg->black].name);
	gg->result = why;
	gg->winner = winner;
	if (winner == WHITE) {
		whiteResult = RESULT_WIN;
		strcpy(winSymbol, "1-0");
		NameOfWinner = player_globals.parray[gg->white].name;
		NameOfLoser = player_globals.parray[gg->black].name;
	} else {
		whiteResult = RESULT_LOSS;
		strcpy(winSymbol, "0-1");
		NameOfWinner = player_globals.parray[gg->black].name;
		NameOfLoser = player_globals.parray[gg->white].name;
	}
	switch (why) {
	case END_CHECKMATE:
		sprintf(tmp, _("%s checkmated} %s"), NameOfLoser, winSymbol);
		strcpy(EndSymbol, "Mat");
		rate_change = 1;
		break;
	case END_RESIGN:
		sprintf(tmp, _("%s resigns} %s"), NameOfLoser, winSymbol);
		strcpy(EndSymbol, "Res");
		rate_change = 1;
		break;
	case END_FLAG:
		sprintf(tmp, _("%s forfeits on time} %s"), NameOfLoser, winSymbol);
		strcpy(EndSymbol, "Fla");
		rate_change = 1;
		break;
	case END_STALEMATE:
		sprintf(tmp, _("Game drawn by stalemate} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "Sta");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_AGREEDDRAW:
		sprintf(tmp, _("Game drawn by mutual agreement} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "Agr");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_BOTHFLAG:
		sprintf(tmp, _("Game drawn because both players ran out of time} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "Fla");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_REPETITION:
		sprintf(tmp, _("Game drawn by repetition} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "Rep");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_50MOVERULE:
		sprintf(tmp, _("Game drawn by the 50 move rule} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "50");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_ADJOURN:
		if (gl >= 0) {
			sprintf(tmp, _("Bughouse game aborted.} *"));
			whiteResult = RESULT_ABORT;
		} else {
			sprintf(tmp, _("Game adjourned by mutual agreement} *"));
			game_save(g);
		}
		break;
	case END_LOSTCONNECTION:
		sprintf(tmp, _("%s lost connection; game "), NameOfWinner);
		if (CheckPFlag(gg->white, PFLAG_REG)
		 && CheckPFlag(gg->black, PFLAG_REG)
		 && gl < 0) {
			sprintf(tmp, _("adjourned} *"));
			game_save(g);
		} else
			sprintf(tmp, _("aborted} *"));
		whiteResult = RESULT_ABORT;
		break;
	case END_ABORT:
		sprintf(tmp, _("Game aborted by mutual agreement} *"));
		whiteResult = RESULT_ABORT;
		// gabrielsan: the game should be removed
		break;
	case END_COURTESY:
		sprintf(tmp, _("Game courtesyaborted by %s} *"), NameOfWinner);
		whiteResult = RESULT_ABORT;
		break;
	case END_COURTESYADJOURN:
		if (gl >= 0) {
			sprintf(tmp, _("Bughouse game courtesyaborted by %s.} *"), NameOfWinner);
			whiteResult = RESULT_ABORT;
		} else {
			sprintf(tmp, _("Game courtesyadjourned by %s} *"), NameOfWinner);
			game_save(g);
		}
		break;
	case END_NOMATERIAL:
		/* Draw by insufficient material (e.g., lone K vs. lone K) */
		sprintf(tmp, _("Neither player has mating material} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "NM ");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_FLAGNOMATERIAL:
		sprintf(tmp,
				_("%s ran out of time and %s has no material to mate} 1/2-1/2"),
				NameOfLoser, NameOfWinner);
		isDraw = 1;
		strcpy(EndSymbol, "TM ");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_ADJWIN:
		sprintf(tmp, _("%s wins by adjudication} %s"), NameOfWinner, winSymbol);
		strcpy(EndSymbol, "Adj");
		rate_change = 1;
		break;
	case END_ADJDRAW:
		sprintf(tmp, _("Game drawn by adjudication} 1/2-1/2"));
		isDraw = 1;
		strcpy(EndSymbol, "Adj");
		rate_change = 1;
		whiteResult = RESULT_DRAW;
		break;
	case END_ADJABORT:
		sprintf(tmp, _("Game aborted by adjudication} *"));
		whiteResult = RESULT_ABORT;
		break;
	default:
		sprintf(tmp, _("Hmm, the game ended and I don't know why} *"));
		d_printf("game_ended(): unknown 'why'\n");
		break;
	}
	strcat(outstr, tmp);

	if (CheckPFlag(gg->white, PFLAG_TOURNEY)
	 && CheckPFlag(gg->black, PFLAG_TOURNEY)) {
	  /* mamer wants more info */
	  sprintf(tmp," [%d %d %d %d %d]",
		  	  gg->wInitTime/(60*10), gg->wIncrement/10, gg->rated,
			  gg->private, gg->type);
	  strcat(outstr, tmp);
	}

	strcat(outstr, "\n");

	if (player_globals.parray[gg->white].chessduserid == -1 ||
		player_globals.parray[gg->black].chessduserid == -1)
	{
		rate_change = 0;
		d_printf( _("Rating won't be changed: ch_white: %d ch_black: %d\n"),
				  player_globals.parray[gg->white].chessduserid,
				  player_globals.parray[gg->black].chessduserid );
	}

	if (gg->rated && rate_change && gg->type != TYPE_BUGHOUSE)
		rating_update(g, -1); /* Adjust ratings; bughouse gets done later. */

	if (beingplayed) {
		int printed = 0, avail_printed = 0;

		pprintf_noformat(gg->white, outstr);
		pprintf_noformat(gg->black, outstr);
		Bell (gg->white);
		Bell (gg->black);

		gg->link = -1;		/*IanO: avoids recursion */
		if (gl >= 0 && game_globals.garray[gl].link >= 0) {
			pprintf_noformat(game_globals.garray[gl].white, outstr);
			pprintf_noformat(game_globals.garray[gl].black, outstr);
			if (CheckPFlag(game_globals.garray[gl].white, PFLAG_OPEN)) {
				getavailmess (game_globals.garray[gl].white, avail_bugwhite);
				print_avail = 1;
			}
			if (CheckPFlag(game_globals.garray[gl].black, PFLAG_OPEN)) {
				getavailmess (game_globals.garray[gl].black, avail_bugblack);
				print_avail = 1;
			}
			if (gg->rated && rate_change) /* Adjust ratings */
				rating_update(g, gl);
			game_ended(gl, CToggle(winner), why);
		}

		if (player_num_active_boards(gg->white) <= 1      /* not a simul or */
				&& CheckPFlag(gg->white, PFLAG_OPEN))
		{   /* simul is over? */
			getavailmess (gg->white,avail_white);
			print_avail = 1;
		} else {    /* Part of an ongoing simul!  Let's shrink the array. */
			; // BUG?
		}

		if (CheckPFlag(gg->black, PFLAG_OPEN)) {
			getavailmess (gg->black,avail_black);
			print_avail = 1;
		}

		// inform observers
		for (p = 0; p < player_globals.p_num; p++) {
			struct player *pp = &player_globals.parray[p];
			if (p == gg->white || p == gg->black || pp->status != PLAYER_PROMPT)
				continue;

			if (CheckPFlag(p, PFLAG_GIN) || player_is_observe(p, g)) {
				pprintf_noformat(p, outstr);
				printed = 1;
			}

			if (CheckPFlag(p, PFLAG_AVAIL) && CheckPFlag(p, PFLAG_OPEN) 
				&& pp->game < 0 && print_avail)
			{
				if ((player_globals.parray[gg->white].b_stats.rating <= pp->availmax &&
					 player_globals.parray[gg->white].b_stats.rating >= pp->availmin) 
					|| !pp->availmax )
				{
					pprintf (p,"\n%s",avail_white);
					avail_printed = 1;
				}
				if ((player_globals.parray[gg->black].b_stats.rating <= pp->availmax &&
					 player_globals.parray[gg->black].b_stats.rating >= pp->availmin) 
					|| !pp->availmax )
				{
					pprintf (p,"\n%s",avail_black);
					avail_printed = 1;
				}
				if (gl != -1) {  /* bughouse ? */
					if ((player_globals.parray[game_globals.garray[gl].white].b_stats.rating <= pp->availmax 
					  && player_globals.parray[game_globals.garray[gl].white].b_stats.rating >= pp->availmin) 
					  || !pp->availmax)
					{
						pprintf (p,"\n%s",avail_bugwhite);
						avail_printed = 1;
					}
					if ((player_globals.parray[game_globals.garray[gl].black].b_stats.rating <= pp->availmax 
					  && player_globals.parray[game_globals.garray[gl].black].b_stats.rating >= pp->availmin) 
					  || !pp->availmax)
					{
						pprintf (p,"\n%s",avail_bugblack);
						avail_printed = 1;
					}
				}
				if (avail_printed) {
					avail_printed = 0;
					printed = 1;
					pprintf(p, "\n");
				}
			}

			if (printed) {
				send_prompt(p);
				printed = 0;
			}
		}

		if (!(gg->rated && rate_change)) {
			pprintf(gg->white, _("No ratings adjustment done.\n"));
			pprintf(gg->black, _("No ratings adjustment done.\n"));
		}
	}

	if (rate_change && gl < 0)
		game_write_complete(g, isDraw, EndSymbol);

	if (!(player_globals.parray[gg->white].simul_info != NULL &&
		  player_globals.parray[gg->white].simul_info->numBoards))
	{
		player_globals.parray[gg->white].num_white++;
		PFlagOFF(gg->white, PFLAG_LASTBLACK);
		player_globals.parray[gg->black].num_black++;
		PFlagON(gg->black, PFLAG_LASTBLACK);
	}

	FREE(player_globals.parray[gg->white].last_opponent);
	FREE(player_globals.parray[gg->black].last_opponent);

	player_globals.parray[gg->white].last_opponent = strdup(gg->black_name);
	player_globals.parray[gg->black].last_opponent = strdup(gg->white_name);

	if (beingplayed) {
		player_globals.parray[gg->white].game = -1;
		player_globals.parray[gg->black].game = -1;
		player_globals.parray[gg->white].opponent = -1;
		player_globals.parray[gg->black].opponent = -1;
		if (gg->white != command_globals.commanding_player)
			send_prompt(gg->white);
		if (gg->black != command_globals.commanding_player)
			send_prompt(gg->black);
		if ((player_globals.parray[gg->white].simul_info != NULL) &&
			(player_globals.parray[gg->white].simul_info->numBoards))
			player_simul_over(gg->white, g, whiteResult);
	}

	if (rate_change) {
		// now that the users had their ratings changed, tell
		// those interested in keeping this info up to date
		player_status_update(gg->white); // inform about white changes
		player_status_update(gg->black); // inform about black changes
	}

	/**
	 * Take this opportunity to tell everyone about player updates
	 * (that is, the users have finished playing and their stats may
	 *  have changed)
	 */
	for (p = 0; p < player_globals.p_num; p++) {
		struct player *pp = &player_globals.parray[p];
		if (pp->status == PLAYER_PROMPT 
			&& CheckPFlag(p, PFLAG_GIN) && pp->ivariables.ext_protocol)
		{
			// only users listening to game notifications will be informed
			sendUserInfo(gg->white, p);
			sendUserInfo(gg->black, p);
		}
	}

	// DEBUG: this has been added to track a potential problem after
	// removing games
	d_printf("Game removal began: game_ended\n");

	game_finish(g);
}

static int was_promoted(struct game *g, int f, int r)
{
#define BUGHOUSE_PAWN_REVERT 1
#ifdef BUGHOUSE_PAWN_REVERT
	int i;

	for (i = g->numHalfMoves-2; i > 0; i -= 2) {
		if (g->moveList[i].toFile == f && g->moveList[i].toRank == r) {
			if (g->moveList[i].piecePromotionTo)
				return 1;
			if (g->moveList[i].fromFile == ALG_DROP)
				return 0;
			f = g->moveList[i].fromFile;
			r = g->moveList[i].fromRank;
		}
	}
#endif
	return 0;
}

int pIsPlaying (int p)
{
	struct player *pp = &player_globals.parray[p];
	int g = pp->game;
	int p1 = pp->opponent;
	struct game *gg = &game_globals.garray[g];

	if (g < 0 || gg->status != GAME_ACTIVE) {
		pprintf (p, _("You are not playing a game.\n"));
		return 0;
	}

	if (gg->white != p && gg->black != p) {
		/* oh oh; big bad game bug. */
		d_printf(_("BUG:  Player %s playing game %d according to player_globals.parray,"
				 "\n      but not according to game_globals.garray.\n"), pp->name, g+1);
		pprintf (p, _("Disconnecting you from game number %d.\n"), g+1);
		pp->game = -1;
		if (p1 >= 0 && player_globals.parray[p1].game == g
		    && gg->white != p1 && gg->black != p1) {
			pprintf (p1, _("Disconnecting you from game number %d.\n"), g+1);
			player_globals.parray[p1].game = -1;
		}
		return 0;
	}
	return 1;
}

/* add clock increments */
static void game_add_increment(struct player *pp, struct game *gg)
{
	/* no update on first move */
	if (gg->game_state.moveNum == 1) 
		return;

	if (net_globals.con[pp->socket]->timeseal) {	/* does he use timeseal? */
		if (pp->side == WHITE) {
			gg->wRealTime += gg->wIncrement * 100;
			gg->wTime = gg->wRealTime / 100; /* remember to conv to tenth secs */
		} else if (pp->side == BLACK) {
			gg->bRealTime += gg->bIncrement * 100;	/* conv to ms */
			gg->bTime = gg->bRealTime / 100; /* remember to conv to tenth secs */
		}
	} else {
		if (gg->game_state.onMove == BLACK)
			gg->bTime += gg->bIncrement;
		else if (gg->game_state.onMove == WHITE)
			gg->wTime += gg->wIncrement;
	}
}

/* updates clocks for a game with timeseal */
void timeseal_update_clocks(struct player *pp, struct game *gg)
{
	/* no update on first move */
	if (gg->game_state.moveNum == 1)
		return;

	if (pp->side == WHITE) {
		gg->wLastRealTime = gg->wRealTime;
		gg->wTimeWhenMoved = net_globals.con[pp->socket]->time;
		if( gg->wTimeWhenMoved < gg->wTimeWhenReceivedMove ||
		    gg->wTimeWhenReceivedMove == 0 )
		{
			/* might seem weird - but could be caused by a person moving BEFORE
			   he receives the board pos (this is possible due to lag) but it's
			   safe to say he moved in 0 secs :-) */
			gg->wTimeWhenReceivedMove = gg->wTimeWhenMoved;
		} else {
			gg->wRealTime -= gg->wTimeWhenMoved - gg->wTimeWhenReceivedMove;
			// just make sure it won't turn negative
            if (gg->wRealTime < 0)
                gg->wRealTime = 0;
		}
	} else if (pp->side == BLACK) {
		gg->bLastRealTime = gg->bRealTime;
		gg->bTimeWhenMoved = net_globals.con[pp->socket]->time;
		if( gg->bTimeWhenMoved < gg->bTimeWhenReceivedMove ||
		    gg->bTimeWhenReceivedMove == 0 )
		{
			gg->bTimeWhenReceivedMove = gg->bTimeWhenMoved;
		} else {
			gg->bRealTime -= gg->bTimeWhenMoved - gg->bTimeWhenReceivedMove;
			// just make sure it won't turn negative
            if (gg->bRealTime < 0)
                gg->bRealTime = 0;
		}
	}
}

void process_move(int p, char *command)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int g, result, len, i;
	struct move_t move;
	unsigned now = 0;

	if (pp->game < 0) {
		pprintf(p, _("You are not playing or examining a game.\n"));
		return;
	}
	decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);

	g = pp->game;
	gg = &game_globals.garray[g];

	if (gg->status == GAME_SETUP) {
		if (!attempt_drop(p,g,command)) {
			pprintf(p, _("You are still setting up the position.\n"));
			pprintf(p, _("Type: 'setup done' when you are finished editing.\n"));
		} else
			send_board_to(g, p);
		return;
	}

	if (gg->status != GAME_EXAMINE) {
		if (!pIsPlaying(p)) 
			return;

		if (pp->side != gg->game_state.onMove) {
			pprintf(p, U_("It is not your move.\n")); /* xboard */
			return;
		}
		if (gg->clockStopped) {
			pprintf(p, _("Game clock is paused, use \"unpause\" to resume.\n"));
			return;
		}
	}
	if ((len = strlen(command)) > 1) {
		if (command[len - 2] == '=') {
			switch (tolower(command[strlen(command) - 1])) {
			case 'n': pp->promote = KNIGHT; break;
			case 'b': pp->promote = BISHOP; break;
			case 'r': pp->promote = ROOK; break;
			case 'q': pp->promote = QUEEN; break;
			default:
				pprintf(p, _("Don't understand that move.\n"));
				return;
			}
		}
	}
	switch (parse_move(command, &gg->game_state, &move, pp->promote)) {
	case MOVE_ILLEGAL:
		if (pp->ivariables.ext_protocol)
			pprintf(p, U_("Illegal move (%s).\n"), command); /* jin*/
		else
			pprintf(p, U_("Illegal move.\n")); /* xboard */
		return;
	case MOVE_AMBIGUOUS:
		pprintf(p, _("Ambiguous move.\n"));
		return;
	}

	move.doublePawn = 0;
	if (gg->status == GAME_EXAMINE) {
		gg->numHalfMoves++;
		if (gg->numHalfMoves > gg->examMoveListSize) {
			gg->examMoveListSize += 20;	/* Allocate 20 moves at a time */
			gg->examMoveList = (struct move_t *)
				realloc(gg->examMoveList,
						sizeof(struct move_t) * gg->examMoveListSize);
		}
		result = execute_move(&gg->game_state, &move, 1);
		move.atTime = now;
		move.wTime = now;
		move.bTime = now;
		move.tookTime = 0;
		MakeFENpos(g, move.FENpos);
		gg->examMoveList[gg->numHalfMoves - 1] = move;
		/* roll back time */
		if (gg->game_state.onMove == WHITE)
			gg->wTime += gg->lastDecTime - gg->lastMoveTime;
		else
			gg->bTime += gg->lastDecTime - gg->lastMoveTime;

		now = tenth_secs();
		if (gg->numHalfMoves == 0)
			gg->timeOfStart = now;
		gg->lastMoveTime = gg->lastDecTime = now;

	} else {			/* real game */
		i = pp->opponent;
		if ( player_globals.parray[i].simul_info &&
			(player_globals.parray[i].simul_info->numBoards &&
			(player_globals.parray[i].simul_info->boards[player_globals.parray[i].simul_info->onBoard] != g)) )
		{
			pprintf(p, U_("It isn't your turn: wait until the simul giver is at your board.\n")); /* xboard */
			return;
		}
		if (net_globals.con[pp->socket]->timeseal)	/* does he use timeseal? */
			timeseal_update_clocks(pp, &game_globals.garray[g]);

		/* We need to reset the opp's time for receiving the board since the
		   timeseal decoder only alters the time if it's 0. Otherwise the time
		   would be changed if the player did a refresh, which would screw up
		   the timings. */
		if (pp->side == WHITE)
			gg->bTimeWhenReceivedMove = 0;
		else
			gg->wTimeWhenReceivedMove = 0;

		game_update_time(g);
		game_add_increment(pp, gg);

		/* Do the move */
		gg->numHalfMoves++;
		if (gg->numHalfMoves > gg->moveListSize) {
			gg->moveListSize += 20;	/* Allocate 20 moves at a time */
			gg->moveList = realloc(gg->moveList, 
								   sizeof(struct move_t) * gg->moveListSize);
		}
		result = execute_move(&gg->game_state, &move, 1);
		if (result == MOVE_OK && gg->link >= 0 && move.pieceCaptured != NOPIECE) {
			/* transfer captured piece to partner */
			/* check if piece reverts to a pawn */
			if (was_promoted(&game_globals.garray[g], move.toFile, move.toRank))
				update_holding(gg->link, colorval(move.pieceCaptured) | PAWN);
			else
				update_holding(gg->link, move.pieceCaptured);
		}
		now = tenth_secs();
		move.atTime = now;
		if (gg->numHalfMoves > 1) {
			move.tookTime = move.atTime - gg->lastMoveTime;
			d_printf("DEBUG: atTime %d, lastMoveTime %d \n", move.atTime, gg->lastMoveTime);
		} else {
			move.tookTime = move.atTime - gg->startTime;
			d_printf("DEBUG: atTime %d, startTime %d \n", move.atTime, gg->startTime);
		}

		gg->lastMoveTime = now;
		gg->lastDecTime = now;
		move.wTime = gg->wTime;
		move.bTime = gg->bTime;
		d_printf("DEBUG: TookTime[no timeseal]: %d\n", move.tookTime);

		if (net_globals.con[pp->socket]->timeseal) {
			/* does he use timeseal? */
			if (pp->side == WHITE) {
				move.tookTime = (game_globals.garray[pp->game].wTimeWhenMoved -
					game_globals.garray[pp->game].wTimeWhenReceivedMove) / 100;

				d_printf("DEBUG: [white] whenmoved %d, whenreceived %d \n",
						game_globals.garray[pp->game].wTimeWhenMoved,
						game_globals.garray[pp->game].wTimeWhenReceivedMove );
			} else {
				move.tookTime = (game_globals.garray[pp->game].bTimeWhenMoved -
					game_globals.garray[pp->game].bTimeWhenReceivedMove) / 100;

				d_printf("DEBUG: [black] whenmoved %d, whenreceived %d \n",
						game_globals.garray[pp->game].bTimeWhenMoved,
						game_globals.garray[pp->game].bTimeWhenReceivedMove );
			}
		}
		d_printf("DEBUG: TookTime[after timeseal]: %d\n", move.tookTime);

		if ((int)move.tookTime < 0)
			move.tookTime = 0;

		MakeFENpos(g, move.FENpos);
		gg->moveList[gg->numHalfMoves - 1] = move;
	}

	send_boards(g);

	if (result == MOVE_ILLEGAL) {
		pprintf(p, _("Internal error, illegal move accepted!\n"));
		d_printf("process_move(): illegal move accepted!\n");
	}
	if (result == MOVE_OK && gg->status == GAME_EXAMINE) {
		int p1;

		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if ( player_globals.parray[p1].status == PLAYER_PROMPT
			 && (player_is_observe(p1, g) || player_globals.parray[p1].game == g) )
			{
				pprintf(p1, _("%s moves: %s\n"), pp->name, move.algString);
			}
		}
	}
	if (result == MOVE_CHECKMATE) {
		if (gg->status == GAME_EXAMINE) {
			int p1;

			for (p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status != PLAYER_PROMPT)
					continue;
				if (player_is_observe(p1, g) ||
					(player_globals.parray[p1].game == g))
				{
					pprintf(p1, _("%s has been checkmated.\n"),
							(CToggle(gg->game_state.onMove) == BLACK) ?
							_("White") : _("Black"));
				}
			}
		} else
			game_ended(g, CToggle(gg->game_state.onMove), END_CHECKMATE);
	}
	if (result == MOVE_STALEMATE) {
		if (gg->status == GAME_EXAMINE) {
			int p1;

			for (p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status != PLAYER_PROMPT)
					continue;
				if (player_is_observe(p1, g) ||
					(player_globals.parray[p1].game == g)) {
					pprintf(p1, _("Stalemate.\n"));
				}
			}
		} else
			game_ended(g, CToggle(gg->game_state.onMove), END_STALEMATE);
	}
	if (result == MOVE_NOMATERIAL) {
		if (gg->status == GAME_EXAMINE) {
			int p1;

			for (p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status != PLAYER_PROMPT)
					continue;
				if (player_is_observe(p1, g) ||
					(player_globals.parray[p1].game == g)) {
					pprintf(p1, _("No mating material.\n"));
				}
			}
		} else
			game_ended(g, CToggle(gg->game_state.onMove), END_NOMATERIAL);
	}
}

int com_resign(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, o, oconnected, pID, oID;

	if (param[0].type == TYPE_NULL) {
		g = pp->game;
		if (!pIsPlaying(p))
			return COM_OK;
		else {
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
			game_ended(g, game_globals.garray[g].white == p ? BLACK : WHITE,
					   END_RESIGN);
		}
	} else
		if (FindPlayer(p, param[0].val.word, &o, &oconnected)) {
			g = game_new();

			// gabrielsan:
			// 	recovering users IDs...
			pID = pp->chessduserid;
			oID = player_globals.parray[o].chessduserid;

			if (game_read(g, p, o) < 0) {
				if (game_read(g, o, p) < 0) {
					pprintf(p, _("You have no stored game with %s\n"),
							player_globals.parray[o].name);
					if (!oconnected)
						player_remove(o);
					return COM_OK;
				} else {
					game_globals.garray[g].white = o;
					game_globals.garray[g].black = p;
				}
			} else {
				game_globals.garray[g].white = p;
				game_globals.garray[g].black = o;
			}

			pprintf(p, _("You resign your stored game with %s\n"),
					player_globals.parray[o].name);
			pcommand(p, _("message %s I have resigned our stored game \"%s vs. %s.\""),
					player_globals.parray[o].name,
					player_globals.parray[game_globals.garray[g].white].name,
					player_globals.parray[game_globals.garray[g].black].name);

			/*	gabrielsan:
			 *		remove based on IDs now
			 */
			game_delete(pID, oID);
			game_ended(g, game_globals.garray[g].white == p ? BLACK : WHITE,
					   END_RESIGN);
			if (!oconnected)
				player_remove(o);
		}
	return COM_OK;
}

static int Check50MoveRule (int p, int g)
{
	struct game *gg = &game_globals.garray[g];
	int num_reversible = gg->numHalfMoves;

	if (gg->game_state.lastIrreversible >= 0)
		num_reversible -= gg->game_state.lastIrreversible;

	if (num_reversible > 99) {
		game_ended(g, gg->white == p ? BLACK : WHITE, END_50MOVERULE);
		return 1;
	}
	return 0;
}

static char *GetFENpos (int g, int half_move)
{
	struct game *gg = &game_globals.garray[g];
	int ls = gg->moveListSize-1;
	if (half_move < 0)
		return gg->FENstartPos;
	else if (half_move > ls )
		return gg->moveList[ls].FENpos;
	else
		return gg->moveList[half_move].FENpos;
}

static int CheckRepetition (int p, int g)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* pend;
	struct game *gg = &game_globals.garray[g];
	int move_num, flag1 = 1, flag2 = 1;
	char *pos1, *pos2, *pos;

	if (gg->numHalfMoves < 8)
		return 0; /* can't have three repeats any quicker. */

	// don't even index it before trying the numHalfMoves
	pos1 = GetFENpos(g, gg->numHalfMoves - 1);
	pos2 = GetFENpos(g, gg->numHalfMoves);

	// check if both variables are valid
	if (!pos1 || !pos2)
		return 0;

	for (move_num = gg->game_state.lastIrreversible;
		 move_num < (gg->numHalfMoves - 1); move_num++)
	{
		pos = GetFENpos(g, move_num);
		if (!strcmp(pos1, pos))
			flag1++;
		if (!strcmp(pos2, pos))
			flag2++;
	}
	if (flag1 >= 3 || flag2 >= 3) {
		if ( (pend = find_pend(pp->opponent, p, PEND_DRAW)) ) {
			delete_pending(pend);
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
		}
		game_ended(g, (gg->white == p) ? BLACK : WHITE, END_REPETITION);
		return 1;
	}
	return 0;
}

int com_draw(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* pend;
	int p1, g = pp->game;

	// better check whether the game is valid
	if (g>=0 && g < game_globals.g_num 
		&& game_globals.garray[g].status == GAME_ACTIVE)
	{
		if (!pIsPlaying(p) || Check50MoveRule(p, g) || CheckRepetition(p, g))
			return COM_OK;

		p1 = pp->opponent;

		if ( player_globals.parray[p1].simul_info &&
			(player_globals.parray[p1].simul_info->numBoards &&
			 player_globals.parray[p1].simul_info->boards[player_globals.parray[p1].simul_info->onBoard] != g))
		{
			pprintf(p, _("You can only make requests when the simul player is at your board.\n"));
			return COM_OK;
		}

		if ( (pend = (find_pend(pp->opponent, p, PEND_DRAW))) ) {
			delete_pending(pend);
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
			game_ended(g, game_globals.garray[g].white == p ? BLACK : WHITE,
					   END_AGREEDDRAW);
		} else {
			pprintf(pp->opponent, "\n");
			pprintf_highlight(pp->opponent, "%s", pp->name);
			pprintf_prompt(pp->opponent, U_(" offers you a draw.\n")); /* xboard */
			pprintf(p, _("Draw request sent.\n"));
			add_request(p, pp->opponent, PEND_DRAW);
		}
	}
	return COM_OK;
}

int com_pause(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending *pend;
	struct game *gg;
	int g, now;

	if (!pIsPlaying(p))
		return COM_OK;

	g = pp->game;
	gg = &game_globals.garray[g];
	if (gg->wTime == 0) {
		pprintf(p, _("You can't pause untimed games.\n"));
		return COM_OK;
	}
	if (gg->clockStopped) {
		pprintf(p, _("Game is already paused, use \"unpause\" to resume.\n"));
		return COM_OK;
	}
	if ( (pend = find_pend(pp->opponent, p, PEND_PAUSE)) ) {
		delete_pending(pend);
		gg->clockStopped = 1;
		/* Roll back the time */
		if (gg->game_state.onMove == WHITE)
			gg->wTime += gg->lastDecTime - gg->lastMoveTime;
		else
			gg->bTime += gg->lastDecTime - gg->lastMoveTime;
	
		now = tenth_secs();
		if (gg->numHalfMoves == 0)
			gg->timeOfStart = now;
		gg->lastMoveTime = gg->lastDecTime = now;
		send_boards(g);
		pprintf_prompt(pp->opponent, U_("\n%s accepted pause. Game clock paused.\n"), /* xboard */
					   pp->name);
		pprintf(p, U_("Game clock paused.\n")); /* xboard */
	} else {
		pprintf(pp->opponent, "\n");
		pprintf_highlight(pp->opponent, "%s", pp->name);
		pprintf_prompt(pp->opponent, U_(" requests to pause the game.\n")); /* xboard */
		pprintf(p, _("Pause request sent.\n"));
		add_request(p, pp->opponent, PEND_PAUSE);
	}
	return COM_OK;
}

int com_unpause(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending *pend;
	int g, now;
	struct game *gg;

	if (!pIsPlaying(p))
		return COM_OK;

	g = pp->game;
	gg = &game_globals.garray[g];
	if (!gg->clockStopped) {
		pprintf(p, _("Game is not paused.\n"));
		return COM_OK;
	}
	if ( (pend = find_pend(pp->opponent, p, PEND_UNPAUSE)) ) {
		delete_pending(pend);
		gg->clockStopped = 0;
		now = tenth_secs();
		if (gg->numHalfMoves == 0)
			gg->timeOfStart = now;
		gg->lastMoveTime = gg->lastDecTime = now;
		send_boards(g);
		pprintf(p, U_("Game clock resumed.\n")); /* xboard */
		pprintf_prompt(pp->opponent, U_("\nGame clock resumed.\n")); /* xboard */
	} else {
		pprintf(pp->opponent, "\n");
		pprintf_highlight(pp->opponent, "%s", pp->name);
		pprintf_prompt(pp->opponent, U_(" requests to unpause the game.\n")); /* xboard */
		pprintf(p, _("Unpause request sent.\n"));
		add_request(p, pp->opponent, PEND_UNPAUSE);
	}
	return COM_OK;
}

int com_abort(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending *pend;
	struct game *gg;
	int p1, g, myColor, yourColor, myGTime, yourGTime;
	int courtesyOK = 1;

	g = pp->game;
	gg = &game_globals.garray[g];
	if (!is_valid_game_handle(g))
		return COM_FAILED;

	if (!pIsPlaying(p))
		return COM_OK;

	p1 = pp->opponent;
	if (p == gg->white) {
		myColor = WHITE;
		yourColor = BLACK;
		myGTime = gg->wTime;
		yourGTime = gg->bTime;
	} else {
		myColor = BLACK;
		yourColor = WHITE;
		myGTime = gg->bTime;
		yourGTime = gg->wTime;
	}
	if ( player_globals.parray[p1].simul_info &&
		(player_globals.parray[p1].simul_info->numBoards &&
		 player_globals.parray[p1].simul_info->boards[player_globals.parray[p1].simul_info->onBoard] != g))
	{
		pprintf(p, _("You can only make requests when the simul player is at your board.\n"));
		return COM_OK;
	}
	if ( (pend = find_pend(p1, p, PEND_ABORT)) ) {
		delete_pending(pend);
		decline_withdraw_offers(p, -1, -1, DO_DECLINE);
		game_ended(g, yourColor, END_ABORT);
	} else {
		game_update_time(g);

		if (net_globals.con[pp->socket]->timeseal
			&& gg->game_state.onMove == myColor
			&& gg->flag_pending == FLAG_ABORT)
		{
			/* It's my move, opponent has asked for abort; I lagged out,
			   my timeseal prevented courtesyabort, and I am sending an abort
			   request before acknowledging (and processing) my opponent's
			   courtesyabort.  OK, let's abort already :-). */
			decline_withdraw_offers(p, -1, -1,DO_DECLINE);
			game_ended(g, yourColor, END_ABORT);
		}

		if (net_globals.con[player_globals.parray[p1].socket]->timeseal) {
			/* opp uses timeseal? */
			int yourRealTime = myColor == WHITE ? gg->bRealTime : gg->wRealTime;

			if (myGTime > 0 && yourGTime <= 0 && yourRealTime > 0) {
				/* Override courtesyabort; opponent still has time.
				 * Check for lag. */
				courtesyOK = 0;

				if (gg->game_state.onMove != myColor
					&& gg->flag_pending != FLAG_CHECKING)
				{
					/* Opponent may be lagging; let's ask. */
					gg->flag_pending = FLAG_ABORT;
					gg->flag_check_time = time(0);
					pprintf(p, _("Opponent has timeseal; trying to courtesyabort.\n"));
					pprintf(p1, "\n[G]\n");
					return COM_OK;
				}
			}
		}

		if (myGTime > 0 && yourGTime <= 0 && courtesyOK) {
			/* player wants to abort + opponent is out of time = courtesyabort */
			pprintf(p, _("Since you have time, and your opponent has none, the game has been aborted."));
			pprintf(p1, _("Your opponent has aborted the game rather than calling your flag."));
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
			game_ended(g, myColor, END_COURTESY);
		} else {
			pprintf(p1, "\n");
			pprintf_highlight(p1, "%s", pp->name);
			pprintf(p1, U_(" would like to abort the game; ")); /* xboard */
			pprintf_prompt(p1, _("type \"abort\" to accept.\n"));
			pprintf(p, _("Abort request sent.\n"));
			add_request(p, p1, PEND_ABORT);
		}
	}
	return COM_OK;
}

static int player_has_mating_material(struct game_state_t *gs, int color)
{
	int i, j, piece, minor_pieces = 0;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++) {
			piece = gs->board[i][j];
			switch (piecetype(piece)) {
			case BISHOP:
			case KNIGHT:
				if (iscolor(piece, color))
					minor_pieces++;
				break;
			case KING:
			case NOPIECE:
				break;
			default:
				if (iscolor(piece, color))
					return 1;
			}
		}
	return minor_pieces > 1;
}

int com_flag(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int g;
	int myColor;

	if (!pIsPlaying(p))
		return COM_OK;

	g = pp->game;
	gg = &game_globals.garray[g];

	myColor = p == gg->white ? WHITE : BLACK;
	if (gg->type == TYPE_UNTIMED) {
		pprintf(p, _("You can't flag an untimed game.\n"));
		return COM_OK;
	}
	if (gg->numHalfMoves < 2) {
		pprintf(p, _("You cannot flag before both players have moved.\n"
					 "Use abort instead.\n"));
		return COM_OK;
	}
	game_update_time(g);

	{
		int myTime, yourTime, opp = pp->opponent, serverTime;

		/* does caller use timeseal? */
		if (net_globals.con[pp->socket]->timeseal) 
			myTime = myColor==WHITE ? gg->wRealTime : gg->bRealTime;
		else
			myTime = myColor == WHITE ? gg->wTime : gg->bTime;

		serverTime = myColor == WHITE ? gg->bTime : gg->wTime;

		/* opp uses timeseal? */
		if (net_globals.con[player_globals.parray[opp].socket]->timeseal)
			yourTime = myColor == WHITE ? gg->bRealTime : gg->wRealTime;
		else
			yourTime = serverTime;

		/* the clocks to compare are now in myTime and yourTime */
		if (myTime <= 0 && yourTime <= 0) {
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
			game_ended(g, myColor, END_BOTHFLAG);
			return COM_OK;
		}

		if (yourTime > 0) {
			/* Opponent still has time, but if that's only because s/he
			 * may be lagging, we should ask for an acknowledgement and then
			 * try to call the flag. */

			if (serverTime <= 0 && gg->game_state.onMove != myColor
			    && gg->flag_pending != FLAG_CHECKING)
			{
				/* server time thinks opponent is down, but RealTime disagrees.
				 * ask client to acknowledge it's alive. */
				gg->flag_pending = FLAG_CALLED;
				gg->flag_check_time = time(0);
				pprintf(p, U_("Opponent has timeseal; checking if (s)he's lagging.\n")); /* xboard */
				pprintf(opp, "\n[G]\n");
				return COM_OK;
			}

			/* if we're here, it means one of:
			 * 1. the server agrees opponent has time, whether lagging or not.
			 * 2. opp. has timeseal (if yourTime != serverTime), had time left
			 *    after the last move (yourTime > 0), and it's still your move.
			 * 3. we're currently checking a flag call after having receiving
			 *    acknowledgement from the other timeseal (and would have reset
			 *    yourTime if the flag were down). */

			pprintf(p, U_("Your opponent is not out of time!\n")); /* xboard */
			return COM_OK;
		}
	}

	decline_withdraw_offers(p, -1, -1, DO_DECLINE);
	if (player_has_mating_material(&gg->game_state, myColor))
		game_ended(g, myColor, END_FLAG);
	else
		game_ended(g, myColor, END_FLAGNOMATERIAL);
	return COM_OK;
}

int com_adjourn(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* pend;
	struct game *gg;
	int p1, g, myColor, yourColor;

	if (!pIsPlaying(p))
		return COM_OK;

	p1 = pp->opponent;
	g = pp->game;
	gg = &game_globals.garray[g];
	/*
	 * Check if the game is valid
	 */
	if (!is_valid_and_active(g)) {
		// problems!
		pprintf(p, _("This game is invalid! Tell an admin.\n"));
		d_printf(_("com_adjourn(): game is invalid!\n"));

		if (!is_valid_game_handle(g))
			d_printf( "CHESSD_ERR: Invalid game handle(%d) when adjourning. "
					  "Match between %s x %s", g,
					  pp->name, player_globals.parray[p1].name );
		else
			d_printf( "CHESSD_ERR: Attempt to adjourn inactive game. "
					  "Match between %s x %s",
					  pp->name, player_globals.parray[p1].name );
		return COM_FAILED;
	}

	if (!CheckPFlag(p, PFLAG_REG) || !CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Both players must be registered to adjourn a game.  Use \"abort\".\n"));
		return COM_OK;
	}
	if (gg->link >= 0) {
		pprintf(p, _("Bughouse games cannot be adjourned.\n"));
		return COM_OK;
	}
	myColor = p == gg->white ? WHITE : BLACK;
	yourColor = myColor == WHITE ? BLACK : WHITE;

	if ( (pend = find_pend(p1, p, PEND_ADJOURN)) ) {
		delete_pending(pend);
		decline_withdraw_offers(p, -1, -1, DO_DECLINE);
		game_ended(pp->game, yourColor, END_ADJOURN);
	} else {
		game_update_time(g);

		if ((myColor == WHITE && gg->wTime > 0 && gg->bTime <= 0)
		 || (myColor == BLACK && gg->bTime > 0 && gg->wTime <= 0))
		{
			/* player wants to adjourn + opponent is out of time = courtesyadjourn */
			pprintf(p,  _("Since you have time, and your opponent has none, the game has been adjourned."));
			pprintf(p1, _("Your opponent has adjourned the game rather than calling your flag."));
			decline_withdraw_offers(p, -1, -1, DO_DECLINE);
			game_ended(g, myColor, END_COURTESYADJOURN);
		} else {
			pprintf(p1, "\n");
			pprintf_highlight(p1, "%s", pp->name);
			pprintf(p1, U_(" would like to adjourn the game; ")); /* xboard */
			pprintf_prompt(p1, _("type \"adjourn\" to accept.\n"));
			pprintf(p, _("Adjourn request sent.\n"));
			add_request(p, p1, PEND_ADJOURN);
		}
	}
	return COM_OK;
}

int com_takeback(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int nHalfMoves = 1, g, i, p1, pend_half_moves;
	struct pending* from;
	struct game *gg;

	if (!pIsPlaying(p))
		return COM_OK;

	p1 = pp->opponent;
	if ( player_globals.parray[p1].simul_info &&
		(player_globals.parray[p1].simul_info->numBoards &&
		 player_globals.parray[p1].simul_info->boards[player_globals.parray[p1].simul_info->onBoard] != pp->game))
	{
		pprintf(p, _("You can only make requests when the simul player is at your board.\n"));
		return COM_OK;
	}

	g = pp->game;
	gg = &game_globals.garray[g];
	if (gg->link >= 0) {
		pprintf(p, _("Takeback not implemented for bughouse games yet.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_INT) {
		nHalfMoves = param[0].val.integer;
		if (nHalfMoves <= 0) {
			pprintf (p,_("You can't takeback less than 1 move.\n"));
			return COM_OK;
		}
	}
	if ( (from = find_pend(pp->opponent, p, PEND_TAKEBACK)) ) {
		pend_half_moves = from->wtime;
		delete_pending(from);
		if (pend_half_moves == nHalfMoves) {
			/* Doing the takeback */
			decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
			for (i = 0; i < nHalfMoves; i++) {
				if (backup_move(g, REL_GAME) != MOVE_OK) {
					pprintf(gg->white, _("Can only backup %d moves\n"), i);
					pprintf(gg->black, _("Can only backup %d moves\n"), i);
					break;
				}
			}
		
			gg->wTimeWhenReceivedMove = gg->bTimeWhenReceivedMove = 0;
		
			send_boards(g);
		} else {
			if (!gg->numHalfMoves) {
				pprintf(p, _("There are no moves in your game.\n"));
				pprintf_prompt(pp->opponent, _("\n%s has declined the takeback request.\n"),
							   pp->name);
				return COM_OK;
			}
	
			if (gg->numHalfMoves < nHalfMoves) {
				pprintf(p, _("There are only %d half moves in your game.\n"), gg->numHalfMoves);
				pprintf_prompt(pp->opponent, _("\n%s has declined the takeback request.\n"),
							   pp->name);
				return COM_OK;
			}
			pprintf(p, _("You disagree on the number of half-moves to takeback.\n"));
			pprintf(p, _("Alternate takeback request sent.\n"));
			pprintf_prompt(pp->opponent, _("\n%s proposes a different number (%d) of half-move(s).\n"), 
						   pp->name, nHalfMoves);
			from = add_request(p, pp->opponent, PEND_TAKEBACK);
			from->wtime = nHalfMoves;
		}
	} else {
		if (!gg->numHalfMoves) {
			pprintf(p, _("There are no moves in your game.\n"));
			return COM_OK;
		}
		if (gg->numHalfMoves < nHalfMoves) {
			pprintf(p, _("There are only %d half moves in your game.\n"), 
					gg->numHalfMoves);
			return COM_OK;
		}
		pprintf(pp->opponent, "\n");
		pprintf_highlight(pp->opponent, "%s", pp->name);
		pprintf_prompt(pp->opponent, U_(" would like to take back %d half move(s).\n"), /* xboard */
					   nHalfMoves);
		pprintf(p, _("Takeback request sent.\n"));
		from = add_request(p, pp->opponent, PEND_TAKEBACK);
		from->wtime = nHalfMoves;
	}
	return COM_OK;
}

int com_switch(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* pend;
	int g = pp->game, tmp, now, p1;
	char *strTmp;
	struct game *gg = &game_globals.garray[g];

	if (!pIsPlaying(p))
		return COM_OK;

	p1 = pp->opponent;
	if ( player_globals.parray[p1].simul_info &&
		(player_globals.parray[p1].simul_info->numBoards &&
		 player_globals.parray[p1].simul_info->boards[player_globals.parray[p1].simul_info->onBoard] != g))
	{
		pprintf(p, _("You can only make requests when the simul player is at your board.\n"));
		return COM_OK;
	}

	if (gg->link >= 0) {
		pprintf(p, _("Switch not implemented for bughouse games.\n"));
		return COM_OK;
	}
	if ( (pend = find_pend(pp->opponent, p, PEND_SWITCH)) ) {
		delete_pending(pend);
		/* Doing the switch */
		decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
	
		tmp = gg->white;
		gg->white = gg->black;
		gg->black = tmp;
		pp->side = pp->side == WHITE ? BLACK : WHITE;
		strTmp = strdup(gg->white_name);
		strcpy(gg->white_name, gg->black_name);
		strcpy(gg->black_name, strTmp);
		free(strTmp);
	
		player_globals.parray[pp->opponent].side =
			player_globals.parray[pp->opponent].side == WHITE ? BLACK : WHITE;
		/* Roll back the time */
		if (gg->game_state.onMove == WHITE)
			gg->wTime += gg->lastDecTime - gg->lastMoveTime;
		else
			gg->bTime += gg->lastDecTime - gg->lastMoveTime;
	
		now = tenth_secs();
		if (gg->numHalfMoves == 0)
			gg->timeOfStart = now;
		gg->lastMoveTime = gg->lastDecTime = now;
		send_boards(g);
		return COM_OK;
	}
	if (gg->rated && gg->numHalfMoves > 0) {
		pprintf(p, _("You cannot switch sides once a rated game is underway.\n"));
		return COM_OK;
	}
	pprintf(pp->opponent, "\n");
	pprintf_highlight(pp->opponent, "%s", pp->name);
	pprintf_prompt(pp->opponent, U_(" would like to switch sides.\n")); /* xboard */
	pprintf_prompt(pp->opponent, _("Type \"accept\" to switch sides, or \"decline\" to refuse.\n"));
	pprintf(p, _("Switch request sent.\n"));
	add_request(p, pp->opponent, PEND_SWITCH);
	return COM_OK;
}

int com_time(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int p1, g;

	if (param[0].type == TYPE_NULL) {
		g = pp->game;
		if (!pIsPlaying(p))
			return COM_OK;
	} else {
		g = GameNumFromParam(p, &p1, &param[0]);
		if (g < 0)
			return COM_OK;
	}
	gg = &game_globals.garray[g];
	if (g < 0 || g >= game_globals.g_num || gg->status != GAME_ACTIVE) {
		pprintf(p, _("There is no such game.\n"));
		return COM_OK;
	}
	game_update_time(g);

	pprintf(p, _("White (%s) : %d mins, %d secs\n"),
			player_globals.parray[gg->white].name,
			gg->wTime / 600, (gg->wTime - ((gg->wTime / 600) * 600)) / 10);
	pprintf(p, _("Black (%s) : %d mins, %d secs\n"),
			player_globals.parray[gg->black].name,
			gg->bTime / 600, (gg->bTime - ((gg->bTime / 600) * 600)) / 10);
	return COM_OK;
}

int com_ptime(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int retval, part = pp->partner;

	if (part < 0) {
		pprintf(p, _("You do not have a partner.\n"));
		return COM_OK;
	}
	retval = pcommand(p, "time %s", player_globals.parray[part].name);
	return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_boards(int p, param_list param)
{
	char *category = NULL;
	char dname[MAX_FILENAME_SIZE];

	if (param[0].type == TYPE_WORD)
	category = param[0].val.word;
	if (category) {
		pprintf(p, _("Boards Available For Category %s:\n"), category);
		sprintf(dname, "%s/%s", BOARD_DIR, category);
	} else {
		pprintf(p, _("Categories Available:\n"));
		sprintf(dname, "%s", BOARD_DIR);
	}
	if (!plsdir(p, dname))
		pprintf(p, "No such category %s, try \"boards\".\n", category);
	return COM_OK;
}

int com_simmatch(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, g, adjourned, num;
	char tmp[100];
	struct pending* pend;
	char* board = NULL;
	char* category = NULL;
	char fname[MAX_FILENAME_SIZE];
	struct game *gg;

	if (in_list(p, L_NAME_ABUSER, pp->login)) {
		pprintf(p, "You can't play here because your name is abusing.");
		return COM_OK;
	}

	if (pp->game >=0) {
		if (game_globals.garray[pp->game].status == GAME_EXAMINE) {
			pprintf(p, _("You are still examining a game.\n"));
			return COM_OK;
		}
		else if (game_globals.garray[pp->game].status == GAME_SETUP) {
			pprintf(p, _("You are still setting up a position.\n"));
			return COM_OK;
		}
	}
	p1 = player_find_part_login(param[0].val.word);
	if (p1 < 0) {
		pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
		return COM_OK;
	}
	if (p == p1) {
		pprintf(p, _("You can't simmatch yourself!\n"));
		return COM_OK;
	}

	if ( (pend = find_pend(p1, p, PEND_SIMUL)) ) {
		/* Accepting Simul ! */
		if (pp->simul_info && pp->simul_info->numBoards >= MAX_SIMUL) {
			pprintf(p, _("You are already playing the maximum of %d boards.\n"), MAX_SIMUL);
			pprintf(p1, _("Simul request removed, boards filled.\n"));
			delete_pending(pend);
			return COM_OK;
		}
		unobserveAll(p);		/* stop observing when match starts */
		unobserveAll(p1);

		g = game_new();
		gg = &game_globals.garray[g];
		adjourned = 0;
		
		/*
		 * gabrielsan: adjourned games have been restored
		 */
		if (game_read(g, p, p1) >= 0) {
			adjourned = 1;
			delete_pending(pend);
		}

		if (!adjourned) {		/* no adjourned game, so begin a new game */

			if ( pend->category && pend->board_type ) {
				board = strdup(pend->category);
				category = strdup(pend->board_type);
			}

			delete_pending(pend);

			if (create_new_match(g, p, p1, 0, 0, 0, 0, 0, (board ? board : ""),
								 (category ? category : ""), 1, 1) == COM_FAILED)
			{
				pprintf(p, _("There was a problem creating the new match.\n"));
				pprintf_prompt(p1, _("There was a problem creating the new match.\n"));
				game_remove(g);

				if (board != NULL) {
					free(board);
					free(category);
				}
				return COM_OK;
			}

			if (board != NULL) {
				free (board);
				free (category);
			}

		} else {			/* resume adjourned game */
			/* gabrielsan:
			 * 	TODO: the game_delete here should remove the old 'adjourned'
			 * 	game. For now, nothing happens.
			 */
			game_delete(p, p1);

			sprintf(tmp, U_("{Game %d (%s vs. %s) Continuing %s %s simul.}\n"),
					g + 1, pp->name, player_globals.parray[p1].name,
					rstr[gg->rated], bstr[gg->type]); /* xboard */
			pprintf(p, tmp);
			pprintf(p1, tmp);

			gg->white = p;
			gg->black = p1;
			gg->status = GAME_ACTIVE;
			gg->startTime = tenth_secs();
			gg->lastMoveTime = gg->lastDecTime = gg->startTime;
			pp->game = g;
			pp->opponent = p1;
			pp->side = WHITE;
			player_globals.parray[p1].game = g;
			player_globals.parray[p1].opponent = p;
			player_globals.parray[p1].side = BLACK;
			send_boards(g);
		}

		if (pp->simul_info == NULL) {
			pp->simul_info = malloc(sizeof(struct simul_info_t));
			pp->simul_info->numBoards = 0;
			pp->simul_info->onBoard = 0;
			pp->simul_info->num_wins = 
			pp->simul_info->num_draws = 
			pp->simul_info->num_losses = 0;
		}
		num = pp->simul_info->numBoards;
		/*    pp->simul_info->results[num] = -1; */
		pp->simul_info->boards[num] = pp->game;
		pp->simul_info->numBoards++;
		if (pp->simul_info->numBoards > 1 && pp->simul_info->onBoard >= 0)
			player_goto_board(p, pp->simul_info->onBoard);
		else
			pp->simul_info->onBoard = 0;
		return COM_OK;
	}
	if ( find_pend(-1, p, PEND_SIMUL) ) {
		pprintf(p, _("You cannot be the simul giver and request to join another simul.\nThat would just be too confusing for me and you.\n"));
		return COM_OK;
	}
	if (pp->simul_info && pp->simul_info->numBoards) {
		pprintf(p, _("You cannot be the simul giver and request to join another simul.\nThat would just be too confusing for me and you.\n"));
		return COM_OK;
	}
	if (pp->game >= 0) {
		pprintf(p, _("You are already playing a game.\n"));
		return COM_OK;
	}
	if (!CheckPFlag(p1, PFLAG_SIMOPEN)) {
		pprintf_highlight(p, "%s", player_globals.parray[p1].name);
		pprintf(p, _(" is not open to receiving simul requests.\n"));
		return COM_OK;
	}
	if ( player_globals.parray[p1].simul_info &&
		 player_globals.parray[p1].simul_info->numBoards >= MAX_SIMUL )
	{
		pprintf_highlight(p, "%s", player_globals.parray[p1].name);
		pprintf(p, _(" is already playing the maximum of %d boards.\n"), MAX_SIMUL);
		return COM_OK;
	}

	/* loon: checking for some crazy situations we can't allow :) */

	if (player_globals.parray[p1].simul_info
	 && player_globals.parray[p1].game >=0 
	 && player_globals.parray[p1].simul_info->numBoards == 0)
	{
		pprintf_highlight(p, "%s", player_globals.parray[p1].name);

		if (player_globals.parray[game_globals.garray[player_globals.parray[p1].game].white].simul_info->numBoards) {
			pprintf(p, _(" is playing in "));
			pprintf_highlight(p, "%s",
				player_globals.parray[player_globals.parray[p1].opponent].name);
			pprintf(p, _("'s simul, and can't accept.\n"));
		} else
			pprintf(p, _(" can't begin a simul while playing a non-simul game.\n"));
		return COM_OK;
	}

	g = game_new();		/* Check if an adjourned untimed game */
	gg = &game_globals.garray[g];
	/*
	 * gabrielsan: adjourned restored
	 */
	adjourned = !(game_read(g, p, p1) < 0 && game_read(g, p1, p) < 0);
	if (adjourned && gg->type != TYPE_UNTIMED)
		adjourned = 0;
	game_remove(g);

	pend = add_request(p, p1, PEND_SIMUL);

	if (param[1].type == TYPE_WORD && param[2].type == TYPE_WORD) {
		sprintf(fname, "%s/%s/%s", BOARD_DIR, param[1].val.word, param[2].val.word);
		if (!file_exists(fname)) {
			pprintf(p, _("No such category/board: %s/%s\n"),
					param[1].val.word, param[2].val.word);
			return COM_OK;
		}
		pend->category = strdup(param[1].val.word);
		pend->board_type = strdup(param[2].val.word);
	} else {
		pend->category = NULL;
		pend->board_type = NULL;
	}

	pprintf(p1, "\n");
	pprintf_highlight(p1, "%s", pp->name);
	if (adjourned) {
		pprintf_prompt(p1, U_(" requests to continue an adjourned simul game.\n")); /* xboard */
		pprintf(p, _("Request to resume simul sent. Adjourned game found.\n"));
	} else {
		if (pend->category == NULL)
			pprintf_prompt(p1, U_(" requests to join a simul match with you.\n")); /* xboard */
		else
			pprintf_prompt(p1, U_(" requests to join a %s %s simul match with you.\n"), /* xboard */
					pend->category,pend->board_type);
		pprintf(p, _("Simul match request sent.\n"));
	}
	return COM_OK;
}

int com_goboard(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int on, g, p1, gamenum;

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (param[0].type == TYPE_WORD) {
		p1 = player_find_part_login(param[0].val.word);
		if (p1 < 0) {
			pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
			return COM_OK;
		}
		if (p == p1) {
			pprintf(p, _("You can't goboard yourself!\n"));
			return COM_OK;
		}
	
		gamenum = player_globals.parray[p1].game;
		if (gamenum < 0) {
			pprintf(p, _("%s is not playing a game.\n"), 
					   player_globals.parray[p1].login);
			return COM_OK;
		}
	} else {
		gamenum = param[0].val.integer - 1;
		if (gamenum < 0)
			gamenum = 0;
	}

	on = pp->simul_info->onBoard;
	g = pp->simul_info->boards[on];
	if (gamenum == g) {
		pprintf(p, _("You are already at that board!\n"));
		return COM_OK;
	}
	if (pp->simul_info->numBoards > 1) {
		decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
		if (player_goto_simulgame_bynum(p, gamenum) != -1 && g >= 0){
			pprintf(game_globals.garray[g].black, "\n");
			pprintf_highlight(game_globals.garray[g].black, "%s", pp->name);
			pprintf_prompt(game_globals.garray[g].black, 
						   _(" has moved away from your board.\n"));
		} else
			pprintf(p, _("You are not playing that game/person.\n"));
	} else
		pprintf(p, _("You are only playing one board!\n"));
	return COM_OK;
}

int com_simnext(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int on, g;

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (pp->simul_info->numBoards > 1) {
		decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
		on = pp->simul_info->onBoard;
		g = pp->simul_info->boards[on];
		if (g >= 0) {
			pprintf(game_globals.garray[g].black, "\n");
			pprintf_highlight(game_globals.garray[g].black, "%s", pp->name);
			pprintf_prompt(game_globals.garray[g].black, 
						   _(" is moving away from your board.\n"));
			player_goto_next_board(p);
		}
	} else
		pprintf(p, _("You are only playing one board!\n"));
	return COM_OK;
}

int com_simprev(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int on, g;

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}
	if (pp->simul_info->numBoards > 1) {
		decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
		on = pp->simul_info->onBoard;
		g = pp->simul_info->boards[on];
		if (g >= 0) {
			pprintf(game_globals.garray[g].black, "\n");
			pprintf_highlight(game_globals.garray[g].black, "%s", pp->name);
			pprintf_prompt(game_globals.garray[g].black, 
						   _(" is moving back to the previous board.\n"));
		}
		player_goto_prev_board(p);
	} else
		pprintf(p, _("You are only playing one board!\n"));
	return COM_OK;
}

int com_simgames(int p, param_list param)
{
	int p1 = p;

	if (param[0].type == TYPE_WORD 
		&& (p1 = player_find_part_login(param[0].val.word)) < 0)
	{
		pprintf(p, _("No player named %s is logged in.\n"), param[0].val.word);
		return COM_OK;
	}
	if (p1 == p)
		pprintf(p, _("You are playing %d simultaneous games.\n"),
	    		   player_num_active_boards(p1));
	else
		pprintf(p, _("%s is playing %d simultaneous games.\n"), player_globals.parray[p1].name,
	    		   player_num_active_boards(p1));
	return COM_OK;
}

int com_simpass(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, p1, on;
	struct game *gg;

	if (!pIsPlaying(p))
		return COM_OK;

	g = pp->game;
	gg = &game_globals.garray[g];
	p1 = gg->white;

	if (player_globals.parray[p1].simul_info == NULL) {
		pprintf(p, _("You are not participating in a simul.\n"));
		return COM_OK;
	}

	if (!player_globals.parray[p1].simul_info->numBoards) {
		pprintf(p, _("You are not participating in a simul.\n"));
		return COM_OK;
	}
	if (p == p1) {
		pprintf(p, _("You are the simul holder and cannot pass!\n"));
		return COM_OK;
	}
	if (player_num_active_boards(p1) == 1) {
		pprintf(p, _("This is the only game, so passing is futile.\n"));
		return COM_OK;
	}
	on = player_globals.parray[p1].simul_info->onBoard;
	if (player_globals.parray[p1].simul_info->boards[on] != g) {
		pprintf(p, _("You cannot pass until the simul holder arrives!\n"));
		return COM_OK;
	}
	if (gg->passes >= MAX_SIMPASS) {
		Bell (p);
		pprintf(p, _("You have reached your maximum of %d pass(es).\n"), MAX_SIMPASS);
		pprintf(p, _("Please move IMMEDIATELY!\n"));
		pprintf_highlight(p1, "%s", pp->name);
		pprintf_prompt(p1, _(" tried to pass, but is out of passes.\n"));
		return COM_OK;
	}
	decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);

	gg->passes++;
	pprintf(p, _("You have passed and have %d pass(es) left.\n"),
			   (MAX_SIMPASS - gg->passes));
	pprintf_highlight(p1, "%s", pp->name);
	pprintf_prompt(p1, _(" has decided to pass and has %d pass(es) left.\n"),
					   (MAX_SIMPASS - gg->passes));
	player_goto_next_board(p1);
	return COM_OK;
}

int com_simabort(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}
	decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
	game_ended(pp->simul_info->boards[pp->simul_info->onBoard],
			   WHITE, END_ABORT);
	return COM_OK;
}

int com_simallabort(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int i;

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
	for (i = 0; pp->simul_info != NULL && i < pp->simul_info->numBoards; i++)
		if (pp->simul_info->boards[i] >= 0)
			game_ended(pp->simul_info->boards[i], WHITE, END_ABORT);

	return COM_OK;
}

int com_simadjourn(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}
	decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE);
	game_ended(pp->simul_info->boards[pp->simul_info->onBoard], WHITE, END_ADJOURN);
	return COM_OK;
}

int com_simalladjourn(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int i;

	if (pp->simul_info == NULL) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}

	if (!pp->simul_info->numBoards) {
		pprintf(p, _("You are not giving a simul.\n"));
		return COM_OK;
	}
	/* decline_withdraw_offers(p, -1, -PEND_SIMUL, DO_DECLINE); */
	for (i = 0; pp->simul_info && i < pp->simul_info->numBoards; i++)
		if (pp->simul_info->boards[i] >= 0)
			game_ended(pp->simul_info->boards[i], WHITE, END_ADJOURN);

	return COM_OK;
}

int com_moretime(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int g, increment;

	if (pp->game >=0 && (game_globals.garray[pp->game].status == GAME_EXAMINE ||
						 game_globals.garray[pp->game].status == GAME_SETUP))
	{
		pprintf(p, _("You cannot use moretime in an examined game.\n"));
		return COM_OK;
	}
	increment = param[0].val.integer;
	if (increment <= 0) {
		pprintf(p, _("Moretime requires an integer value greater than zero.\n"));
		return COM_OK;
	}
	if (!pIsPlaying(p))
		return COM_OK;

	if (increment > 600) {
		pprintf(p, _("Moretime has a maximum limit of 600 seconds.\n"));
		increment = 600;
	}
	g = pp->game;
	gg = &game_globals.garray[g];
	if (gg->white == p) {
		gg->bTime += increment * 10;
		gg->bRealTime += increment * 10 * 100;
		pprintf(p, U_("%d seconds were added to your opponent's clock\n"), /* xboard */
				increment);
		pprintf_prompt(pp->opponent,
					   U_("\nYour opponent has added %d seconds to your clock.\n"), /* xboard */
					   increment);
	}
	if (gg->black == p) {
		gg->wTime += increment * 10;
		gg->wRealTime += increment * 10 * 100;
		pprintf(p, U_("%d seconds were added to your opponent's clock\n"), /* xboard */
				increment);
		pprintf_prompt(pp->opponent,
					   U_("\nYour opponent has added %d seconds to your clock.\n"), /* xboard */
					   increment);
	}
	return COM_OK;
}

