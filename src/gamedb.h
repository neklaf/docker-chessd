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

#ifndef _GAMEDB_H
#define _GAMEDB_H

#include "config.h"

#include <stdio.h>
#include <time.h>
#include <dbi/dbi.h>

#include "command.h"
// #include "genparser.h"
#include "board.h"

extern const char *bstr[], *rstr[];
extern const char *ratedStyles[];

#define GAMEFILE_VERSION 5
#define MAX_GLINE_SIZE 1024

#define REL_GAME 0
#define REL_SPOS 1
#define REL_REFRESH 2
#define REL_EXAMINE 3

enum gamestatus {GAME_EMPTY, GAME_NEW, GAME_ACTIVE, GAME_EXAMINE, GAME_SETUP};

/* Do not change the order of these - DAV */
enum gametype {TYPE_UNTIMED, TYPE_BLITZ, TYPE_STAND, TYPE_NONSTANDARD,
               TYPE_WILD, TYPE_LIGHT, TYPE_BUGHOUSE, NUM_GAMETYPES};

/* OK, DAV, I'll try it another way. -- hersco */
enum ratetype {RATE_STAND, RATE_BLITZ, RATE_WILD, RATE_LIGHT, RATE_BUGHOUSE};

#define NUM_RATEDTYPE 5

#define FLAG_CHECKING -1
#define FLAG_NONE 0
#define FLAG_CALLED 1
#define FLAG_ABORT 2

enum gameend {
	END_CHECKMATE = 0,
	END_RESIGN,
	END_FLAG,
	END_AGREEDDRAW,
	END_REPETITION,
	END_50MOVERULE,
	END_ADJOURN,
	END_LOSTCONNECTION,
	END_ABORT,
	END_STALEMATE,
	END_NOTENDED,
	END_COURTESY,
	END_BOTHFLAG,
	END_NOMATERIAL,
	END_FLAGNOMATERIAL,
	END_ADJDRAW,
	END_ADJWIN,
	END_ADJABORT,
	END_COURTESYADJOURN
};

struct game {
	int gameID;

	/* Not saved in game file */
	int revertHalfMove;
	int totalHalfMoves;
	int white;
	int black;
	int link;
	enum gamestatus status;
	int examHalfMoves;
	int examMoveListSize;
	struct move_t *examMoveList; /*_LEN(examMoveListSize)*/    /* extra movelist for examine */

	unsigned startTime;    /* The relative time the game started  */
	unsigned lastMoveTime; /* Last time a move was made */
	unsigned lastDecTime;  /* Last time a players clock was decremented */
	int flag_pending;
	int wTimeWhenReceivedMove;
	int wTimeWhenMoved;
	int bTimeWhenReceivedMove;
	int bTimeWhenMoved;
	int wLastRealTime;
	int wRealTime;
	int bLastRealTime;
	int bRealTime;

	/* this is a dummy variable used to tell which bits are saved in the structure */
	unsigned not_saved_marker;

	/* Saved in the game file */
	enum gameend result;
	int winner;
	int wInitTime, wIncrement;
	int bInitTime, bIncrement;
	unsigned timeOfStart;
	unsigned flag_check_time;
	int wTime;
	int bTime;
	int clockStopped;
	int rated;
	int private;
	enum gametype type;
	int passes; /* For simul's */
	int numHalfMoves;
	int moveListSize; /* Total allocated in *moveList */
	struct move_t *moveList; /* _LEN(moveListSize) */      /* primary movelist */
	char FENstartPos[74]; /*_NULLTERM*/   /* Save the starting position. */
	struct game_state_t game_state;

	char white_name[MAX_LOGIN_NAME]; /*_NULLTERM*/
		/* to hold the playername even after he disconnects */

	char black_name[MAX_LOGIN_NAME]; /*_NULLTERM*/
	int white_rating;
	int black_rating;

	// not yet used attributes
	int whiteID;
	int blackID;
};

extern const char *TypeStrings[NUM_GAMETYPES];
extern const char *TypeChars[NUM_GAMETYPES];

int is_valid_game_handle(int g);
int is_valid_and_active(int g);
int game_new(void);
int game_free(int g);
int game_remove(int g);
int game_finish(int g);
void MakeFENpos (int g, char *FEN);
char *game_name(int wt, int winc, int bt, int binc, char *cat, char *board);
char *game_str(int rated, int wt, int winc, int bt, int binc, char *cat, char *board);
int game_isblitz(int wt, int winc, int bt, int binc, char *cat, char *board);
void send_board_to(int g, int p);
void send_boards(int g);
void game_update_time(int g);
char *EndString(int g, int personal);
const char *EndSym(int g);
char *movesToString(int g, int pgn);
void game_disconnect(int g, int p);
int CharToPiece(char c);
char PieceToChar(int piece);
int game_delete(int wp, int bp);
int game_save(int g);
void RemHist(char *who);
void game_write_complete(int g, int isDraw, char *EndSymbol);
int game_count_only_actives(void);
int game_count(void);
int game_read(int g, int wp, int bp);
int game_delete(int wp, int bp);
int game_delete_by_id(int gameid);
int findGameByID(int g, int gameID);
const char *getGameType(int wt, int winc, int bt, int binc,
						char *cat, char *board);

#endif

