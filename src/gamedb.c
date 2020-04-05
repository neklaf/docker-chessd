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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __Linux__
#include <linux/stddef.h>
#endif

#include "configuration.h"
#include "board.h"
#include "chessd_locale.h"
#include "common.h"
#include "eco.h"
#include "chessd_main.h"
#include "gamedb.h"
#include "gameproc.h"
#include "globals.h"
#include "malloc.h"
#include "network.h"
#include "utils.h"

#include "playerdb.h"
#include "dbdbi.h"
/*
 * 	gabrielsan: wiping genstruct out
//#include "parser.h"
#include "gamedb_old.h"
*/
#include "config_genstrc.h"

static int check_kings(struct game_state_t *gs);
static int get_empty_slot(void);
//static long OldestHistGame(char *login);
static int game_zero(int g);
static int WriteGame_Database(int g);

const char *TypeStrings[NUM_GAMETYPES] = {"untimed", "blitz", "standard",
					  "nonstandard", "wild", "lightning",
					  "bughouse"};

const char *ratedStyles[NUM_RATEDTYPE] =
	{"standard", "blitz", "wild", "lightning", "bughouse"};

int game_save(int g)
{
	// gabrielsan: 12/01/2004
	//  write data to db instead of file
	return WriteGame_Database(g);
}

void game_write_complete(int g, int isDraw, char *EndSymbol)
{
	// gabrielsan (12/01/2004)
	// 	this function is actually obsolete; the extra information
	// 	it used to generate can be obtained using a query

	// writing the game to the database
	WriteGame_Database(g);
}

/* gabrielsan (12/01/2004)
 *	Function used to insert a game into the database
 *
 */
static int WriteGame_Database(int g)
{
	struct game gg = game_globals.garray[g];
	struct move_t *mov;
	int wpID, bpID, i, gameId;
	dbi_result res;
	char *esc_movestring, *esc_algstring, *esc_fenpos;
	const char sql[]=
		"INSERT INTO Game (gametypeid, gamestatusid, gameresultid, "
						  "WhitePlayerID, BlackPlayerID, WhiteRating, "
						  "BlackRating, Winner, wInitTime, wIncrement, "
						  "bInitTime, bIncrement, timeOfStart, wTime, bTime, "
						  "rated, private, clockStopped, flag_check_time, "
						  "passes, numHalfMoves, FENstartPos, "
						  "WhitePlayerName, BlackPlayerName) "
				 " VALUES (%i, %i, %i, "
				 		  "%i, %i, %i, "
				 		  "%i, %i, %i, %i, "
				 		  "%i, %i, %i, %i, %i, "
				 		  "%i, %i, %i, %i, "
				 		  "%i, %i, %s, "
						  "'%s', '%s')";

	// get the user ids from the database
	wpID = getChessdUserID(gg.white_name);
	bpID = getChessdUserID(gg.black_name);

	/* zero any elements we don't want to save */
	memset(&gg, 0, offsetof(struct game, not_saved_marker));
	gg.game_state.gameNum = 0;

	d_printf("\nplayers= %d %d/%d %d\n", wpID, bpID, gg.white, gg.black);

	if (wpID < 0) {
		// if one of the users is not in the db, the game should not be saved
		d_printf("user [%-30s] not in db.\n", gg.white_name);
		return -1;
	}

	if (bpID < 0) {
		// if one of the users is not in the db, the game should not be saved
		d_printf("user [%-30s] not in db.\n", gg.black_name);
		return -1;
	}

	if(!dbi_conn_quote_string_copy(dbiconn, gg.FENstartPos, &esc_fenpos))
		esc_fenpos = strdup("NULL");

	// before saving the time, revert it to a decent representation
	gg.timeOfStart = untenths(gg.timeOfStart);

	// write the query
	res = dbi_conn_queryf( dbiconn, sql,
		gg.type,         gg.status,       gg.result,          wpID,
		bpID,            gg.white_rating, gg.black_rating,    gg.winner,
		gg.wInitTime,    gg.wIncrement,   gg.bInitTime,       gg.bIncrement,
		gg.timeOfStart,  gg.wTime,        gg.bTime,           gg.rated,
		gg.private,      gg.clockStopped, gg.flag_check_time, gg.passes,
		gg.numHalfMoves, esc_fenpos,      gg.white_name,      gg.black_name );

	if (res) {
		// Get the gameid field from the last INSERTed record
		// The sequence name is PgSQL's implicit SEQUENCE on SERIAL field,
		// see: http://www.postgresql.org/docs/8.0/interactive/datatype.html#DATATYPE-SERIAL
		// Happily, this does what we want for MySQL also, retrieving the
		// last record's AUTO_INCREMENT field.
		gameId = dbi_conn_sequence_last(dbiconn, "Game_gameid_seq");
		d_printf("INSERT INTO Game: dbi_conn_sequence_last() = %d\n",gameId);

		// insert the moves
		for (i=0; i < gg.numHalfMoves; i++) {
			mov = &gg.moveList[i];

			if(!dbi_conn_quote_string_copy(dbiconn, mov->moveString, &esc_movestring))
				esc_movestring = strdup("NULL");
			if(!dbi_conn_quote_string_copy(dbiconn, mov->algString, &esc_algstring))
				esc_algstring = strdup("NULL");
			if(!dbi_conn_quote_string_copy(dbiconn, mov->FENpos, &esc_fenpos))
				esc_fenpos = strdup("NULL");

			dbi_result_free( dbi_conn_queryf( dbiconn,
				"INSERT INTO Move (GameID, fromFile, fromRank, toFile, toRank,"
								  "doublePawn, moveString, algString, FENpos,"
								  "atTime, tookTime, movorder, color)"
						 " VALUES (%i, %i, %i, %i, %i,"
								  "%i, %s, %s, %s, "
								  "%i, %i, %i, %i)",
				gameId, mov->fromFile, mov->fromRank, mov->toFile, mov->toRank,
				mov->doublePawn, esc_movestring, esc_algstring, esc_fenpos,
				mov->atTime, mov->tookTime, i, mov->color ) );

			FREE(esc_movestring);
			FREE(esc_algstring);
			FREE(esc_fenpos);
		}
		dbi_result_free(res);
	}
	return 0;
}

/* this method is awful! how about allocation as we need it and freeing
    afterwards! */
static int get_empty_slot(void)
{
	int i;

	for (i = 0; i < game_globals.g_num; i++) {
		if (game_globals.garray[i].status == GAME_EMPTY)
			return i;
	}
	game_globals.g_num++;
	game_globals.garray =
		(struct game *)realloc(game_globals.garray,
							   sizeof(struct game) * game_globals.g_num);
	/* yeah great, bet this causes lag!  - DAV*/
	/* I have serious doubt of the truth to the above client- bugg */
	game_globals.garray[game_globals.g_num - 1].status = GAME_EMPTY;
	return game_globals.g_num - 1;
}

int game_new(void)
{
	int new = get_empty_slot();
	game_zero(new);

	return new;
}

static int game_zero(int g)
{
	struct game *gg = &game_globals.garray[g];
	ZERO_STRUCT(game_globals.garray[g]);

	gg->white = -1;
	gg->black = -1;

	gg->status = GAME_NEW;
	gg->link = -1;
	gg->result = END_NOTENDED;
	gg->type = TYPE_UNTIMED;
	gg->game_state.gameNum = g;
	gg->wInitTime = 300;	/* 5 minutes */
	gg->wIncrement = 0;
	gg->bInitTime = 300;	/* 5 minutes */
	gg->bIncrement = 0;
	gg->flag_pending = FLAG_NONE;
	gg->numHalfMoves = 0;

	strcpy(gg->FENstartPos,INITIAL_FEN);

	return 0;
}

int game_free(int g)
{
	struct game *gg = &game_globals.garray[g];
	FREE(gg->moveList);
	FREE(gg->examMoveList);
	gg->moveList = NULL;
	gg->examMoveList = NULL;
	gg->moveListSize = 0;
	gg->examMoveListSize = 0;
	return 0;
}

int game_remove(int g)
{
	struct game *gg = &game_globals.garray[g];
	int white_player, black_player;

	d_printf("CHESSD: started removing game\n");

	if (gg->status == GAME_EMPTY)
		return 0;

	if(gg->status == GAME_ACTIVE)
	  InsertServerEvent( seGAME_COUNT_CHANGE, game_count_only_actives()-1 );

	d_printf("CHESSD: game count OK\n");

	white_player = gg->white;
	black_player = gg->black;

    DeleteActiveGame(player_globals.parray[white_player].login,
	                 player_globals.parray[black_player].login);

	d_printf("CHESSD: active game removed OK\n");

	/* Should remove game from players observation list */
	game_free(g);
	d_printf("CHESSD: game freed OK\n");
	game_zero(g);
	d_printf("CHESSD: game zeroed OK\n");

	gg->status = GAME_EMPTY;
	return 0;
}

/* old moves not stored now - uses smoves */
int game_finish(int g)
{
	player_game_ended(g);		/* Alert playerdb that game ended */
	game_remove(g);
	return 0;
}

void MakeFENpos (int g, char *FEN)
{
	strcpy(FEN, boardToFEN(g));
}

static char *game_time_str(int wt, int winc, int bt, int binc)
{
	static char tstr[50];

	if (!wt && !winc) {			/* Untimed */
		strcpy(tstr, "");
		return tstr;
	}
	if (wt == bt && winc == binc)
		sprintf(tstr, " %d %d", wt, winc);
	else
		sprintf(tstr, " %d %d : %d %d", wt, winc, bt, binc);
	return tstr;
}

const char *bstr[] = {"untimed", "blitz", "standard",
	"non-standard", "wild", "lightning", "Bughouse"};
const char *rstr[] = {"unrated", "rated"};

char *game_name(int wt, int winc, int bt, int binc, char* cat, char* board)
{
	static char tstr[200];

	sprintf(tstr, "%s",
			bstr[game_isblitz(wt / 60, winc, bt / 60, binc, cat, board)]);
	return tstr;
}

const char *getGameType(int wt, int winc, int bt, int binc,
						char *cat, char *board)
{
	return bstr[game_isblitz(wt / 60, winc, bt / 60, binc, cat, board)];
}

char *game_str(int rated, int wt, int winc, int bt, int binc,
		       char *cat, char *board)
{
	static char tstr[200];

	if (cat && cat[0] && board && board[0] &&
	(strcmp(cat, "standard") || strcmp(board, "standard")))
		sprintf(tstr, _("%s %s%s Loaded from %s/%s"),
				rstr[rated],
				bstr[game_isblitz(wt / 60, winc, bt / 60, binc, cat, board)],
				game_time_str(wt / 60, winc, bt / 60, binc),
				cat, board);
	else
		sprintf(tstr, "%s %s%s",
				rstr[rated],
				bstr[game_isblitz(wt / 60, winc, bt / 60, binc, cat, board)],
				game_time_str(wt / 60, winc, bt / 60, binc));
	return tstr;
}

int game_isblitz(int wt, int winc, int bt, int binc, char *cat, char *board)
{
	int total;

	if(cat && cat[0]) {
		if (!strcmp(cat, "bughouse"))
			return TYPE_BUGHOUSE;
		if (board && board[0]) {
			if (!strcmp(cat, "wild"))
				return TYPE_WILD;
			if (strcmp(cat, "standard") || strcmp(board, "standard"))
				return TYPE_NONSTANDARD;
		}
	}

	if (wt == 0 || bt == 0) /* nonsense if one is timed and one is not */
		return TYPE_UNTIMED;

	if (wt != bt || winc != binc)
		return TYPE_NONSTANDARD;
	total = wt * 60 + winc * 40;
	if (total < 180)		/* 3 minute */
		return TYPE_LIGHT;
	if (total >= 900)		/* 15 minutes */
		return TYPE_STAND;
	else
		return TYPE_BLITZ;
}

void send_board_to(int g, int p)
{
	struct game *gg = &game_globals.garray[g];
	struct player *pp = &player_globals.parray[p];
	char *b;
	int side;
	int relation;

/* since we know g and p, figure out our relationship to this game */

	side = WHITE;
	if (gg->status == GAME_EXAMINE || gg->status == GAME_SETUP)
		relation = pp->game == g ? 2 : -2;
	else {
		if (pp->game == g) {
			side = pp->side;
			relation = side == gg->game_state.onMove ? 1 : -1;
		} else
			relation = 0;
	}

	if (CheckPFlag(p, PFLAG_FLIP))  /* flip board? */
		side = side == WHITE ? BLACK : WHITE;

	// don't bother updating time of a non-active game
	if (gg->status == GAME_ACTIVE)
		game_update_time(g);

	b = board_to_string(gg->white_name,
						gg->black_name,
						gg->wTime,
						gg->bTime,
						&gg->game_state,
						gg->status == GAME_EXAMINE || gg->status == GAME_SETUP
							? gg->examMoveList : gg->moveList,
						pp->style,
						side, relation, p);
	Bell(p);

	if (pp->game == g && net_globals.con[pp->socket]->timeseal)
		pprintf_noformat(p, "\n%s\n[G]\n", b);
	else
		pprintf_noformat(p, "\n%s", b);

	if (p != command_globals.commanding_player)
		send_prompt(p);
}

void send_boards(int g)
{
	struct game *gg = &game_globals.garray[g];
	int p,p1;
	struct simul_info_t *simInfo =
		player_globals.parray[gg->white].simul_info;
	int which_board = -1;

	/* Begin code added 3/28/96 - Figbert */

	if (gg->status != GAME_SETUP && check_kings(&gg->game_state)) {
		d_printf( _("Game has invalid number of kings.  Aborting...\n"));
		game_finish(g);

		for (p = 0; p < player_globals.p_num; p++) {
			struct player *pp = &player_globals.parray[p];
			if (pp->status == PLAYER_EMPTY)
				continue;
			if (pp->game == g) {

				p1 = pp->opponent;

				if (p1 >= 0 && player_globals.parray[p1].game == g) {
					pprintf (p1, _("Disconnecting you from game number %d.\n"), g+1);
					player_globals.parray[p1].game = -1;
				}

				pprintf (p, _("Disconnecting you from game number %d.\n"), g+1);
				pp->game = -1;
			}
		}
		return;
	}

	/* End code added 3/28/96 - Figbert */

	if (simInfo != NULL)
		which_board = simInfo->boards[simInfo->onBoard];

	if ((simInfo == NULL) || (which_board == g)) {
		for (p = 0; p < player_globals.p_num; p++) {
			struct player *pp = &player_globals.parray[p];
			if (pp->status == PLAYER_EMPTY)
				continue;
			if (player_is_observe(p, g) || (pp->game == g))
				send_board_to(g, p);
		}
	}
}

int is_valid_game_handle(int g)
{
	return g >= 0 && g <= game_globals.g_num;
}

int is_valid_and_active(int g) {
	return is_valid_game_handle(g) &&
		   game_globals.garray[g].status == GAME_ACTIVE;
}

void game_update_time(int g)
{
	struct game *gg = &game_globals.garray[g];
	unsigned now, timesince;

	// just make sure this is indeed a valid game number
	if (is_valid_game_handle(g)
	&& game_globals.garray[g].status == GAME_ACTIVE) {
		/* no update on first move */
		if (gg->game_state.moveNum != 1 && !gg->clockStopped
			&& gg->type != TYPE_UNTIMED)
		{
			now = tenth_secs();
			timesince = now - gg->lastDecTime;
			if (gg->game_state.onMove == WHITE)
				gg->wTime -= timesince;
			else
				gg->bTime -= timesince;
			gg->lastDecTime = now;
		}
	}
	else {
		d_printf("CHESSD: Attempt to update the time of an invalid game\n");
		if (!is_valid_game_handle(g))
			d_printf("CHESSD_ERROR: Invalid game handle (%d)\n",g);
		else if (game_globals.garray[g].status != GAME_ACTIVE)
			d_printf("CHESSD_ERROR: Reason: game (%d) not active\n", g);
	}
}

char *EndString(int g, int personal)
{
	struct game *gg = &game_globals.garray[g];
	static char endstr[200];
	char *blackguy, *whiteguy, *winner, *loser;

	if(personal){
		blackguy = gg->black_name;
		whiteguy = gg->white_name;
	}else{
		blackguy = _("Black");
		whiteguy = _("White");
	}
	if(gg->winner == WHITE){
		winner = whiteguy;
		loser  = blackguy;
	}else{
		winner = blackguy;
		loser  = whiteguy;
	}

	switch (gg->result) {
	case END_CHECKMATE:
		sprintf(endstr, _("%s checkmated"), loser);
		break;
	case END_RESIGN:
		sprintf(endstr, _("%s resigned"), loser);
		break;
	case END_FLAG:
		sprintf(endstr, _("%s ran out of time"), loser);
		break;
	case END_AGREEDDRAW:
		return _("Game drawn by mutual agreement");
	case END_BOTHFLAG:
		return _("Game drawn because both players ran out of time");
	case END_REPETITION:
		return _("Game drawn by repetition");
	case END_50MOVERULE:
		return _("Draw by the 50 move rule");
	case END_ADJOURN:
		return _("Game adjourned by mutual agreement");
	case END_LOSTCONNECTION:
		sprintf(endstr, _("%s lost connection, game adjourned"), winner);
		break;
	case END_ABORT:
		return _("Game aborted by mutual agreement");
	case END_STALEMATE:
		return _("Stalemate.");
	case END_NOTENDED:
		return _("Still in progress");
	case END_COURTESY:
		sprintf(endstr, _("Game courtesyaborted by %s"), winner);
		break;
	case END_COURTESYADJOURN:
		sprintf(endstr, _("Game courtesyadjourned by %s"), winner);
		break;
	case END_NOMATERIAL:
		return _("Game drawn because neither player has mating material");
	case END_FLAGNOMATERIAL:
		sprintf(endstr, _("%s ran out of time and %s has no material to mate"),
				loser, winner);
		break;
	case END_ADJDRAW:
		return _("Game drawn by adjudication");
	case END_ADJWIN:
		sprintf(endstr, _("%s wins by adjudication"), winner);
		break;
	case END_ADJABORT:
		return _("Game aborted by adjudication");
	default:
		return "???????";
	}
	return endstr;
}

const char *EndSym(int g)
{
	static const char *symbols[] = {"1-0", "0-1"};

	switch (game_globals.garray[g].result) {
	case END_CHECKMATE:
	case END_RESIGN:
	case END_FLAG:
	case END_ADJWIN:
		return symbols[game_globals.garray[g].winner != WHITE];
	case END_AGREEDDRAW:
	case END_BOTHFLAG:
	case END_REPETITION:
	case END_50MOVERULE:
	case END_STALEMATE:
	case END_NOMATERIAL:
	case END_FLAGNOMATERIAL:
	case END_ADJDRAW:
		return "1/2-1/2";
	default:
		break; // avoids compiler warning about unhandled cases
	}
	return "*";
}

/* This should be enough to hold any game up to at least 8000 moves
 * If we overwrite this, the server will crash :-).
 */
/* 8000? who you trying to kid? this is awful - enough for 600 halfs :) -DAV*/
#define GAME_STRING_LEN 19000
static char gameString[GAME_STRING_LEN];
char *movesToString(int g, int pgn)
{
	static char truncwarning[] = N_(" ... (too long, truncated!)");
	struct game *gg = &game_globals.garray[g];
	struct move_t *moves;
	char tmp[160];
	int wr, br, i, col, len, n;
	time_t curTime;

	if (gg->status == GAME_EXAMINE || gg->status == GAME_SETUP)
		moves = gg->examMoveList;
	else
		moves = gg->moveList;

	wr = gg->white_rating;
	br = gg->black_rating;

	curTime = untenths(gg->timeOfStart);

	if (pgn) {
		sprintf(gameString,
			"\n[Event \"%s %s %s game\"]\n"
			"[Site \"%s, %s\"]\n",
			config.strs[server_name],
			rstr[gg->rated], bstr[gg->type],
			config.strs[server_name],
			config.strs[server_location]);

		strftime(tmp, sizeof(tmp),
			"[Date \"%Y.%m.%d\"]\n"
			"[Time \"%H:%M:%S\"]\n",
			localtime((time_t *) &curTime));
		strcat(gameString, tmp);
		sprintf(tmp,
			"[Round \"-\"]\n"
			"[White \"%s\"]\n"
			"[Black \"%s\"]\n"
			"[WhiteElo \"%d\"]\n"
			"[BlackElo \"%d\"]\n",
			gg->white_name, gg->black_name, wr, br);
		strcat(gameString, tmp);
		sprintf(tmp,
			"[TimeControl \"%d+%d\"]\n"
			"[Mode \"ICS\"]\n"
			"[Result \"%s\"]\n\n",
			gg->wInitTime / 10, gg->wIncrement / 10, EndSym(g));
		strcat(gameString, tmp);

		col = 0;
		for (i = 0; i < gg->numHalfMoves; i++) {
			if (moves[i].color == WHITE) {
				if ((col += sprintf(tmp, "%d. ", (i+1)/2 + 1)) > 70) {
					strcat(gameString, "\n");
					col = 0;
				}
				strcat(gameString, tmp);
			} else if (i==0) {
				strcat(tmp, "1. ... ");
				col += 7;
			}
			if ((col += sprintf(tmp, "%s ", moves[i].algString)) > 70) {
				strcat(gameString, "\n");
				col = 0;
			}
			strcat(gameString, tmp);
		}
		strcat(gameString, "\n");

	} else {

		sprintf(gameString, "\n%s ", gg->white_name);
		if (wr > 0)
			sprintf(tmp, "(%d) ", wr);
		else
			strcpy(tmp, _("(UNR) "));

		strcat(gameString, tmp);
		sprintf(tmp, "vs. %s ", gg->black_name);
		strcat(gameString, tmp);
		if (br > 0)
			sprintf(tmp, "(%d) ", br);
		else
			strcpy(tmp, _("(UNR) "));

		strcat(gameString, tmp);
		strcat(gameString, "--- ");
		strcat(gameString, strltime(&curTime));

		strcat(gameString, gg->rated ? _("\nRated ") : _("\nUnrated "));
		strcat(gameString, TypeStrings[gg->type]);
		strcat(gameString, _(" match, initial time: "));
		if (gg->bInitTime != gg->wInitTime || gg->wIncrement != gg->bIncrement)
		{ /* different starting times */
			sprintf(tmp,
				_("%d minutes, increment: %d seconds AND %d minutes, increment: %d seconds.\n\n"),
				gg->wInitTime / 600, gg->wIncrement / 10,
				gg->bInitTime / 600, gg->bIncrement / 10);
		} else {
			sprintf(tmp, _("%d minutes, increment: %d seconds.\n\n"),
				gg->wInitTime / 600, gg->wIncrement / 10);
		}

		strcat(gameString, tmp);
		sprintf(tmp, U_("Move  %-19s%-19s\n"), gg->white_name, gg->black_name); /* xboard */
		strcat(gameString, tmp);
		strcat(gameString, "----  ----------------   ----------------\n");

		len = strlen(gameString) + strlen(_(truncwarning)); // leave room for warning at end
		for (i = 0; i < gg->numHalfMoves; i += 2) {
			if (i == 0 && moves[i].color == BLACK) {
				sprintf(tmp, "%3d.  %-16s   %-16s\n",
						(i+1)/2 + 1, "...", move_and_time(&moves[i]));
				i--;
			} else if (i + 1 < gg->numHalfMoves) {
				sprintf(tmp, "%3d.  %-16s   ",
						(i+1)/2 + 1, move_and_time(&moves[i]));
				strcat(gameString, tmp);
				sprintf(tmp, "%-16s\n", move_and_time(&moves[i+1]));
			} else
				sprintf(tmp, "%3d.  %-16s\n", (i+1)/2 + 1,
						move_and_time(&moves[i]));

			n = strlen(tmp);
			if(len+n >= GAME_STRING_LEN){	/* don't overflow string */
				strcat(gameString, _(truncwarning));
				return gameString;
			}
			strcat(gameString, tmp);
			len += n;
		}
		strcat(gameString, "      ");
	}

	sprintf(tmp, "{%s} %s\n", EndString(g, 0), EndSym(g));
	strcat(gameString, tmp);

	return gameString;
}

void game_disconnect(int g, int p)
{
	game_ended(g, (game_globals.garray[g].white == p) ? WHITE : BLACK,
			   END_LOSTCONNECTION);
}

int CharToPiece(char c)
{
	switch (c) {
	case 'P': return W_PAWN;
	case 'p': return B_PAWN;
	case 'N': return W_KNIGHT;
	case 'n': return B_KNIGHT;
	case 'B': return W_BISHOP;
	case 'b': return B_BISHOP;
	case 'R': return W_ROOK;
	case 'r': return B_ROOK;
	case 'Q': return W_QUEEN;
	case 'q': return B_QUEEN;
	case 'K': return W_KING;
	case 'k': return B_KING;
	default:  return NOPIECE;
	}
}

char PieceToChar(int piece)
{
	int t = piecetype(piece);
	return t < NOPIECE || t > KING ? '?'
		: ( (isblack(piece) ? " pnbrqk" : " PNBRQK")[t] );
}

int game_count_only_actives(void)
{
	int g, count = 0;

	for (g = 0; g < game_globals.g_num; g++) {
	  if (game_globals.garray[g].status == GAME_ACTIVE)
	    count++;
	}
	return count;
}

// get the number of active games
int game_count(void)
{
	int g, count = 0;

	for (g = 0; g < game_globals.g_num; g++) {
		if (game_globals.garray[g].status == GAME_ACTIVE
		 || game_globals.garray[g].status == GAME_EXAMINE
		 || game_globals.garray[g].status == GAME_SETUP)
			count++;
	}
	if (count > command_globals.game_high)
		command_globals.game_high = count;
	return count;
}

static int check_kings(struct game_state_t *gs)
{
	/* Function written 3/28/96 by Figbert to check for 1 king each side only! */
	int blackking = 0, whiteking = 0;
	int f, r;

	for (f = 0; f < 8; f++) {
		for (r = 0; r < 8; r++) {
			if (gs->board[f][r] == B_KING)
				blackking++;
			else if (gs->board[f][r] == W_KING)
				whiteking++;
		}
	}

	return blackking == 1 && whiteking == 1 ? 0 /*Perfect!*/ : -1;
}

int ReadGameFromDB(int g, char* where)
{
	dbi_result res;
	int nr, gtype, gresult;
	char *gFENstartPos, *gwhite_name, *gblack_name,
		 *mmovestring, *malgstring, *mfenpos;
	struct game *gg = &game_globals.garray[g];
	struct game g1;
	struct move_t *m;

	res = dbi_conn_queryf(dbiconn, "SELECT * FROM Game WHERE %s", where);
	if (!res || !dbi_result_next_row(res) ) {
		// no games have been found
		if(res) dbi_result_free(res);
		return -1;
	}

	g1 = *gg;

    memset(&gg->not_saved_marker, 0,
           sizeof(struct game) - offsetof(struct game, not_saved_marker));
    gg->game_state.gameNum = g;

	// read game attributes
	dbi_result_get_fields( res,
		"gameID.%i GameTypeID.%i GameResultID.%i "
		"WhitePlayerID.%i BlackPlayerID.%i WhiteRating.%i BlackRating.%i "
		"Winner.%i wInitTime.%i wIncrement.%i bInitTime.%i "
		"bIncrement.%i timeOfStart.%i wTime.%i bTime.%i "
		"rated.%i private.%i clockStopped.%i flag_check_time.%i "
		"passes.%i numHalfMoves.%i "
		"FENstartPos.%s WhitePlayerName.%s BlackPlayerName.%s",
		&gg->gameID, &gtype, &gresult,
		&gg->whiteID, &gg->blackID, &gg->white_rating, &gg->black_rating,
		&gg->winner, &gg->wInitTime, &gg->wIncrement, &gg->bInitTime,
		&gg->bIncrement, &gg->timeOfStart, &gg->wTime, &gg->bTime,
		&gg->rated, &gg->private, &gg->clockStopped, &gg->flag_check_time,
		&gg->passes, &gg->numHalfMoves,
		&gFENstartPos, &gwhite_name, &gblack_name );
    gg->type = gtype;     // it's an enum, might not be int-sized
    gg->result = gresult; // ditto
	gg->timeOfStart = tenths(gg->timeOfStart);
    strncpy(gg->FENstartPos, gFENstartPos, sizeof(gg->FENstartPos));
    strncpy(gg->white_name, gwhite_name, sizeof(gg->white_name));
    strncpy(gg->black_name, gblack_name, sizeof(gg->black_name));

	dbi_result_free(res);

	// read the moves
	res = dbi_conn_queryf(dbiconn,
		"SELECT * FROM Move WHERE GameID=%i ORDER BY movOrder", gg->gameID);
	nr = res ? dbi_result_get_numrows(res) : 0;

	// read the query, inserting the moves in the game structure
	if (nr > 0) {
		gg->moveListSize = nr;
		gg->moveList = malloc(sizeof(struct move_t) * gg->moveListSize);
		for( m = gg->moveList ; dbi_result_next_row(res) ; ++m ){
			dbi_result_get_fields(res,
				"fromFile.%i fromRank.%i toFile.%i toRank.%i "
				"doublePawn.%i moveString.%s algString.%s FENpos.%s "
				"atTime.%i tookTime.%i",
				&m->fromFile, &m->fromRank, &m->toFile, &m->toRank,
				&m->doublePawn, &mmovestring, &malgstring, &mfenpos,
				&m->atTime, &m->tookTime);
			strncpy(m->moveString, mmovestring, sizeof(m->moveString));
			strncpy(m->algString, malgstring, sizeof(m->algString));
			strncpy(m->FENpos, mfenpos, sizeof(m->FENpos));

			d_printf("myFEN= %s\n", m->FENpos);
		}
		// restore the game pieces to their place
		// bugfix: no setup is done if no movement was recovered
		FEN_to_board(gg->moveList[nr-1].FENpos, &gg->game_state);

		// the following was the one responsible for some recent
		// problems
		d_printf("myFEN= %s\n",gg->moveList[nr-1].FENpos);
	}
	else {
		// at least get some space for the movements
		gg->moveListSize = 50;
		gg->numHalfMoves = 0;
		gg->moveList = malloc(sizeof(struct move_t) * gg->moveListSize);
		d_printf(_("CHESSD: game with 0 movements\n"));
	}
	if(res) dbi_result_free(res);

	/* when examining we are not supposed to restore the game
	   state, so put it back here */
	if (g1.status == GAME_EXAMINE || g1.status == GAME_SETUP)
		gg->game_state = g1.game_state;
	gg->status = g1.status;

	/* cope with continuing a game with timeseal that was started without it */
	gg->wRealTime = gg->wTime * 100;
	gg->bRealTime = gg->bTime * 100;

	return 0;
}

int game_read(int g, int wp, int bp)
{
	/*
	 * Parameters:
	 * 		'g' is a game id slot
	 * 		wp: white player index
	 * 		bp: black player index
	 *
	 * Description:
	 * 		Recover an adjourned game
	 */
	struct game *gg = &game_globals.garray[g];
	char where[1024];
	int piece;
	char* sql = "( WhitePlayerID=%d AND BlackPlayerID=%d "
				"AND GameResultID IN(%d,%d,%d) )";

	gg->white = wp;
	gg->black = bp;
	gg->moveListSize = 0;
	gg->game_state.gameNum = g;

	for (piece=PAWN; piece <= QUEEN; piece++)
		gg->game_state.holding[0][piece-PAWN]
			= gg->game_state.holding[1][piece-PAWN] = 0;

	sprintf(where, sql, player_globals.parray[wp].chessduserid,
						player_globals.parray[bp].chessduserid,
						END_ADJOURN, END_COURTESYADJOURN, END_LOSTCONNECTION);

	if (ReadGameFromDB(g, where) < 0)
		return -1;

	gg->result = END_NOTENDED;
	// if the game did not ended, update the players rating
	if (gg->type == TYPE_BLITZ) {
		gg->white_rating = player_globals.parray[wp].b_stats.rating;
		gg->black_rating = player_globals.parray[bp].b_stats.rating;
	} else if (gg->type == TYPE_WILD) {
		gg->white_rating = player_globals.parray[wp].w_stats.rating;
		gg->black_rating = player_globals.parray[bp].w_stats.rating;
	} else if (gg->type == TYPE_LIGHT) {
		gg->white_rating = player_globals.parray[wp].l_stats.rating;
		gg->black_rating = player_globals.parray[bp].l_stats.rating;
	} else if (gg->type == TYPE_BUGHOUSE) {
		gg->white_rating = player_globals.parray[wp].bug_stats.rating;
		gg->black_rating = player_globals.parray[bp].bug_stats.rating;
	} else {
		gg->white_rating = player_globals.parray[wp].s_stats.rating;
		gg->black_rating = player_globals.parray[bp].s_stats.rating;
	}

	gg->status = GAME_ACTIVE;
	gg->startTime = tenth_secs();
	gg->lastMoveTime = gg->lastDecTime = gg->startTime;

	/* cope with continuing a game with timeseal that was started without it */
	gg->wRealTime = gg->wTime * 100;
	gg->bRealTime = gg->bTime * 100;

	return 0;
}

int findGameByID(int g, int gameID)
{
	char where[32];

	sprintf(where, " gameid=%d ", gameID);
	return ReadGameFromDB(g, where);
}


int game_delete(int wp, int bp)
{
	dbi_result res;
	int wpID, bpID, gameID;
	char *sql = "SELECT gameid FROM Game "
				"WHERE gameresultid IN(%i,%i,%i)"
					 " AND ( (whiteplayerid = %i AND blackplayerid = %i)"
						" OR (whiteplayerid = %i AND blackplayerid = %i) )";

    wpID = getChessdUserID(player_globals.parray[wp].login);
    bpID = getChessdUserID(player_globals.parray[bp].login);

	// 1. get the game id
	res = dbi_conn_queryf(dbiconn, sql,
                              END_ADJOURN, END_COURTESYADJOURN, END_LOSTCONNECTION,
                              wpID, bpID, bpID, wpID );

	// if any found... delete first only (BUG?)
	if ( res && dbi_result_next_row(res) ) {
		gameID = dbi_result_get_int_idx(res,1); // dbi field indices are 1-based
		game_delete_by_id(gameID);
	}
	else
		d_printf("game_delete(): no suitable stored game (%d/%d)\n", wpID, bpID);

	if(res) dbi_result_free(res);

	return 0;
}

int game_delete_by_id(int gameid)
{
	// first, remove the moves
	dbi_result_free( dbi_conn_queryf( dbiconn,
		"DELETE FROM Move WHERE gameid = %i", gameid) );
	// .. and the game itself
	dbi_result_free( dbi_conn_queryf( dbiconn,
		"DELETE FROM Game WHERE gameid = %i", gameid) );
	return 0;
}
