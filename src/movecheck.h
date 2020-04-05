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

#ifndef _MOVECHECK_H
#define _MOVECHECK_H

#include "board.h"

#define MOVE_OK 0
#define MOVE_ILLEGAL 1
#define MOVE_STALEMATE 2
#define MOVE_CHECKMATE 3
#define MOVE_AMBIGUOUS 4
#define MOVE_NOMATERIAL 5

#define MS_NOTMOVE 0
#define MS_COMP 1
#define MS_COMPDASH 2
#define MS_ALG 3
#define MS_KCASTLE 4
#define MS_QCASTLE 5

#define isrank(c) (((c) <= '8') && ((c) >= '1'))
#define isfile(c) (((c) >= 'a') && ((c) <= 'h'))


int is_move(const char *mstr);
int NextPieceLoop(board_t b, int *f, int *r, int color);
int InitPieceLoop(board_t b, int *f, int *r, int color);
int legal_move(struct game_state_t * gs,
	       int fFile, int fRank,
	       int tFile, int tRank);
int legal_andcheck_move(struct game_state_t * gs,
			int fFile, int fRank,
			int tFile, int tRank);
int in_check(struct game_state_t * gs);
int has_legal_move(struct game_state_t * gs);
int parse_move(char *mstr, struct game_state_t * gs, struct move_t * mt, int promote);
int execute_move(struct game_state_t * gs, struct move_t * mt, int check_game_status);
int backup_move(int g, int mode);

#endif /* _MOVECHECK_H */
