/*
   Copyright (c) 1993 Richard V. Nash.
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


#ifndef _BOARD_H
#define _BOARD_H

// #include "genparser.h" gabrielsan: TODO
#define WHITE 0x00
#define BLACK 0x80
#define CString(c) ( (c) == WHITE ? "White" : "Black" )
#define CToggle(c) ( (c) == BLACK ? WHITE : BLACK )

/* These will go into an array */

#define NOPIECE	0x00
#define PAWN	0x01
#define KNIGHT	0x02
#define BISHOP	0x03
#define ROOK	0x04
#define QUEEN	0x05
#define KING	0x06

#define MAX_BOARD_STRING_LEN 2500
#define MAX_STYLES 13

#define W_PAWN		(WHITE | PAWN)
#define W_KNIGHT	(WHITE | KNIGHT)
#define W_BISHOP	(WHITE | BISHOP)
#define W_ROOK		(WHITE | ROOK)
#define W_QUEEN		(WHITE | QUEEN)
#define W_KING 		(WHITE | KING)

#define B_PAWN		(BLACK | PAWN)
#define B_KNIGHT	(BLACK | KNIGHT)
#define B_BISHOP	(BLACK | BISHOP)
#define B_ROOK		(BLACK | ROOK)
#define B_QUEEN		(BLACK | QUEEN)
#define B_KING		(BLACK | KING)

#define isblack(p) ((p) & BLACK)
#define iswhite(p) (!isblack(p))
#define iscolor(p,color) (((p) & BLACK) == (color))
#define piecetype(p) ((p) & 0x7f)
#define colorval(p) ((p) & 0x80)
#define square_color(r,f) ((((r)+(f)) & 0x01) ? BLACK : WHITE)

/* Treated as [file][rank] */
typedef int board_t[8][8];

/*GENSTRUCT*/ struct game_state_t {
	int board[8][8];
	/* for bughouse */
	int holding[2][5];
	/* For castling */
	unsigned char wkmoved, wqrmoved, wkrmoved;
	unsigned char bkmoved, bqrmoved, bkrmoved;
	/* for ep */
	int ep_possible[2][8];
	/* For draws */
	int lastIrreversible;
	int onMove;
	int moveNum;
	/* Game num not saved, must be restored when read */
	int gameNum;
};

#define ALG_DROP -2

/* bughouse: if a drop move, then fromFile is ALG_DROP and fromRank is piece */

/*GENSTRUCT*/ struct move_t {
	int color;
	int fromFile, fromRank;
	int toFile, toRank;
	int pieceCaptured;
	int piecePromotionTo;
	int enPassant; /* 0 = no, 1=higher -1= lower */
	int doublePawn; /* Only used for board display */
	char moveString[8]; /*_NULLTERM*/
	char algString[8]; /*_NULLTERM*/
	char FENpos[74]; /*_NULLTERM*/
	unsigned atTime;
	unsigned tookTime;

	/* these are used when examining a game */
	int wTime;
	int bTime;
};

#define MoveToHalfMove(gs) (((gs)->moveNum - 1)*2 + ((gs)->onMove != WHITE))

extern const char *wpstring[];

/* the FEN for the default initial position */
#define INITIAL_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w"


void board_clear(struct game_state_t *gs);
void board_standard(struct game_state_t *gs);
int board_init(int g,struct game_state_t *b, char *category, char *board);
void board_calc_strength(struct game_state_t *b, int *ws, int *bs);
void update_holding(int g, int pieceCaptured);
char *board_to_string(char *wn, char *bn, int wt, int bt,
					  struct game_state_t *b, struct move_t *ml, int style,
					  int orientation, int relation, int p);
char *move_and_time(struct move_t *m);
void wild_init(void);

#endif
