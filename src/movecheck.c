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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "algcheck.h"
#include "chessd_locale.h"
#include "gamedb.h"
#include "globals.h"
#include "movecheck.h"
#include "network.h"
#include "playerdb.h"
#include "utils.h"

/* Simply tests if the input string is a move or not. */
/* If it matches patterns below */
/* Add to this list as you improve the move parser */
/* MS_COMP e2e4 */
/* MS_COMPDASH e2-e4 */
/* MS_CASTLE o-o, o-o-o */
/* Not done yet */
/* MS_ALG e4, Nd5 Ncd5 */

int is_o(char c){
	return c == '0' || c == 'o' || c == 'O';
}

int is_move(const char *m)
{
	int len = strlen(m);
	if (len > 3 && m[len-2] == '=')
		len -= 2;

	if (len == 2 && is_o(m[0]) && is_o(m[1])) /* Test for oo */
		return MS_KCASTLE;

	if (len == 3) {
		/* Test for o-o */
		if(is_o(m[0]) && m[1]=='-' && is_o(m[2]))
			return MS_KCASTLE;
		/* Test for ooo */
		if(is_o(m[0]) && is_o(m[1]) && is_o(m[2]))
			return MS_QCASTLE;
	}

	/* Test for e2e4 */
	if (len == 4 && isfile(m[0]) && isrank(m[1]) && isfile(m[2]) && isrank(m[3]))
		return MS_COMP;

	if (len == 5) {
		/* Test for e2-e4 */
		if(isfile(m[0]) && isrank(m[1]) && m[2]=='-' && isfile(m[3]) && isrank(m[4]))
			return MS_COMPDASH;
		/* Test for o-o-o */
		if(is_o(m[0]) && m[1]=='-' && is_o(m[2]) && m[3]=='-' && is_o(m[4]))
			return MS_QCASTLE;
	}

	return alg_is_move(m);
}

int NextPieceLoop(board_t b, int *f, int *r, int color)
{
	for (;;) {
		++(*r);
		if (*r > 7) {
			*r = 0;
			++(*f);
			if (*f > 7)
				break;
		}
		if (b[*f][*r] != NOPIECE && iscolor(b[*f][*r], color))
			return 1;
	}
	return 0;
}

int InitPieceLoop(board_t b, int *f, int *r, int color)
{
	*f = 0;
	*r = -1;
	return 1;
}

/* All of the routines assume that the obvious problems have been checked */
/* See legal_move() */
static int legal_pawn_move( struct game_state_t *gs, int ff, int fr, int tf, int tr )
{
	if (ff == tf) {
		if (gs->board[tf][tr] != NOPIECE) return 0;
		if (gs->onMove == WHITE) {
			if (tr - fr == 1) return 1;
			if (fr == 1 && tr-fr == 2 && gs->board[ff][2]==NOPIECE) return 1;
		} else {
			if (fr - tr == 1) return 1;
			if (fr == 6 && fr-tr == 2 && gs->board[ff][5]==NOPIECE) return 1;
		}
		return 0;
	}
	if (ff != tf) { /* Capture ? */
		if ((ff - tf != 1) && (tf - ff != 1)) return 0;
		if ((fr - tr != 1) && (tr - fr != 1)) return 0;
		if (gs->onMove == WHITE) {
			if (fr > tr) return 0;
			if (gs->board[tf][tr] != NOPIECE && iscolor(gs->board[tf][tr],BLACK))
				return 1;
			if (gs->ep_possible[0][ff] == 1) {
				if (tf == ff+1 && gs->board[ff+1][fr] == B_PAWN) return 1;
			} else if (gs->ep_possible[0][ff] == -1) {
				if (tf == ff-1 && gs->board[ff-1][fr] == B_PAWN) return 1;
			}
		} else {
			if (tr > fr) return 0;
			if (gs->board[tf][tr] != NOPIECE && iscolor(gs->board[tf][tr],WHITE))
				return 1;
			if (gs->ep_possible[1][ff] == 1) {
				if (tf == ff+1 && gs->board[ff+1][fr] == W_PAWN) return 1;
			} else if (gs->ep_possible[1][ff] == -1) {
				if (tf == ff-1 && gs->board[ff-1][fr] == W_PAWN) return 1;
			}
		}
	}
	return 0;
}

static int legal_knight_move(struct game_state_t * gs, int ff, int fr, int tf, int tr)
{
	int dx = ff - tf, dy = fr - tr;
	return (abs(dx) == 2 && abs(dy) == 1) || (abs(dy) == 2 && abs(dx) == 1);
}

static int legal_bishop_move(struct game_state_t * gs, int ff, int fr, int tf, int tr)
{
	int dx, dy, x, y, startx, starty, count, incx, incy;

	if (ff > tf) {
		dx = ff - tf;
		incx = -1;
	} else {
		dx = tf - ff;
		incx = 1;
	}
	startx = ff + incx;
	if (fr > tr) {
		dy = fr - tr;
		incy = -1;
	} else {
		dy = tr - fr;
		incy = 1;
	}
	starty = fr + incy;
	if (dx != dy)
		return 0;			/* Not diagonal */
	if (dx == 1)
		return 1;			/* One square, ok */
	count = dx - 1;
	for (x = startx, y = starty; count; x += incx, y += incy, count--) {
		if (gs->board[x][y] != NOPIECE)
			return 0;
	}
	return 1;
}

static int legal_rook_move(struct game_state_t * gs, int ff, int fr, int tf, int tr)
{
	int i, start, stop;

	if (ff == tf) {
		if (abs(fr - tr) == 1)
			return 1;
		if (fr < tr) {
			start = fr + 1;
			stop = tr - 1;
		} else {
			start = tr + 1;
			stop = fr - 1;
		}
		for (i = start; i <= stop; i++) {
			if (gs->board[ff][i] != NOPIECE)
				return 0;
		}
		return 1;
	} else if (fr == tr) {
		if (abs(ff - tf) == 1)
			return 1;
		if (ff < tf) {
			start = ff + 1;
			stop = tf - 1;
		} else {
			start = tf + 1;
			stop = ff - 1;
		}
		for (i = start; i <= stop; i++) {
			if (gs->board[i][fr] != NOPIECE)
				return 0;
		}
		return 1;
	} else
		return 0;
}

static int legal_queen_move(struct game_state_t * gs, int ff, int fr, int tf, int tr)
{
	return legal_rook_move(gs, ff, fr, tf, tr) || legal_bishop_move(gs, ff, fr, tf, tr);
}

/* Check if square (kf,kr) is attacked by enemy piece.
 * Used in castling from/through check testing.
 */

/* new one from soso: */
static int is_square_attacked (struct game_state_t *gs, int kf, int kr)
{
	struct game_state_t fakeMove = *gs;
	fakeMove.board[4][kr] = NOPIECE;
	fakeMove.board[kf][kr] = KING | fakeMove.onMove;
	fakeMove.onMove = CToggle(fakeMove.onMove);
	return in_check(&fakeMove);
}

/* old one:
static int is_square_attacked(struct game_state_t * gs, int kf, int kr)
{
  int f, r;
  gs->onMove = CToggle(gs->onMove);

  for (InitPieceLoop(gs->board, &f, &r, gs->onMove);
       NextPieceLoop(gs->board, &f, &r, gs->onMove);) {
    if (legal_move(gs, f, r, kf, kr)) {
      gs->onMove = CToggle(gs->onMove);
      return 1;
    }
  }
  gs->onMove = CToggle(gs->onMove);
  return 0;
}
*/

static int legal_king_move(struct game_state_t * gs, int ff, int fr, int tf, int tr)
{
	if (gs->onMove == WHITE) {
		/* King side castling */
		if ((fr == 0) && (tr == 0) && (ff == 4) && (tf == 6) && !gs->wkmoved
			&& (!gs->wkrmoved) && (gs->board[5][0] == NOPIECE) &&
			(gs->board[6][0] == NOPIECE) && (gs->board[7][0] == W_ROOK) &&
			(!is_square_attacked(gs, 4, 0)) && (!is_square_attacked(gs, 5, 0)))
		{
			return 1;
		}
		/* Queen side castling */
		if ((fr == 0) && (tr == 0) && (ff == 4) && (tf == 2) && !gs->wkmoved
			&& (!gs->wqrmoved) && (gs->board[3][0] == NOPIECE) &&
			(gs->board[2][0] == NOPIECE) && (gs->board[1][0] == NOPIECE) &&
			(gs->board[0][0] == W_ROOK) &&
			(!is_square_attacked(gs, 4, 0)) && (!is_square_attacked(gs, 3, 0)))
		{
			return 1;
		}
	} else {			/* Black */
		/* King side castling */
		if ((fr == 7) && (tr == 7) && (ff == 4) && (tf == 6) && !gs->bkmoved
			&& (!gs->bkrmoved) && (gs->board[5][7] == NOPIECE) &&
			(gs->board[6][7] == NOPIECE) && (gs->board[7][7] == B_ROOK) &&
			(!is_square_attacked(gs, 4, 7)) && (!is_square_attacked(gs, 5, 7)))
		{
			return 1;
		}
		/* Queen side castling */
		if ((fr == 7) && (tr == 7) && (ff == 4) && (tf == 2) && (!gs->bkmoved)
			&& (!gs->bqrmoved) && (gs->board[3][7] == NOPIECE) &&
			(gs->board[2][7] == NOPIECE) && (gs->board[1][7] == NOPIECE) &&
			(gs->board[0][7] == B_ROOK) &&
			(!is_square_attacked(gs, 4, 7)) && (!is_square_attacked(gs, 3, 7)))
		{
			return 1;
		}
	}
	return abs(ff - tf) <= 1 && abs(fr - tr) <= 1;
}

static void add_pos(int tof, int tor, int *posf, int *posr, int *numpos)
{
	posf[*numpos] = tof;
	posr[*numpos] = tor;
	(*numpos)++;
}

static void possible_pawn_moves(struct game_state_t * gs,
							int onf, int onr, int *posf, int *posr, int *numpos)
{
	if (gs->onMove == WHITE) {
		if (gs->board[onf][onr + 1] == NOPIECE) {
			add_pos(onf, onr + 1, posf, posr, numpos);
			if ((onr == 1) && (gs->board[onf][onr + 2] == NOPIECE))
				add_pos(onf, onr + 2, posf, posr, numpos);
		}
		if ((onf > 0) && (gs->board[onf - 1][onr + 1] != NOPIECE) &&
		(iscolor(gs->board[onf - 1][onr + 1], BLACK)))
			add_pos(onf - 1, onr + 1, posf, posr, numpos);
		if ((onf < 7) && (gs->board[onf + 1][onr + 1] != NOPIECE) &&
		(iscolor(gs->board[onf + 1][onr + 1], BLACK)))
			add_pos(onf + 1, onr + 1, posf, posr, numpos);
		if (gs->ep_possible[0][onf] == -1)
			add_pos(onf - 1, onr + 1, posf, posr, numpos);
		if (gs->ep_possible[0][onf] == 1)
			add_pos(onf + 1, onr + 1, posf, posr, numpos);
	} else {
		if (gs->board[onf][onr - 1] == NOPIECE) {
			add_pos(onf, onr - 1, posf, posr, numpos);
			if ((onr == 6) && (gs->board[onf][onr - 2] == NOPIECE))
				add_pos(onf, onr - 2, posf, posr, numpos);
		}
		if ((onf > 0) && (gs->board[onf - 1][onr - 1] != NOPIECE) &&
		(iscolor(gs->board[onf - 1][onr - 1], WHITE)))
			add_pos(onf - 1, onr - 1, posf, posr, numpos);
/* loon: changed what looks like a typo, here's the original line:
      add_pos(onf - 1, onr + 1, posf, posr, numpos);
*/
		if ((onf < 7) && (gs->board[onf + 1][onr - 1] != NOPIECE) &&
		(iscolor(gs->board[onf + 1][onr - 1], WHITE)))
			add_pos(onf + 1, onr - 1, posf, posr, numpos);
		if (gs->ep_possible[1][onf] == -1)
			add_pos(onf - 1, onr - 1, posf, posr, numpos);
		if (gs->ep_possible[1][onf] == 1)
			add_pos(onf + 1, onr - 1, posf, posr, numpos);
	}
}

static void possible_knight_moves(struct game_state_t * gs,
							int onf, int onr, int *posf, int *posr, int *numpos)
{
	static int knightJumps[8][2] = {{-1,  2}, {1,  2}, { 2, -1}, { 2,  1},
									{-1, -2}, {1, -2}, {-2,  1}, {-2, -1}};
	int f, r, j;

	for (j = 0; j < 8; j++) {
		f = knightJumps[j][0] + onf;
		r = knightJumps[j][1] + onr;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			continue;
		if (gs->board[f][r] == NOPIECE ||
		iscolor(gs->board[f][r], CToggle(gs->onMove)))
			add_pos(f, r, posf, posr, numpos);
	}
}

static void possible_bishop_moves(struct game_state_t * gs,
							int onf, int onr, int *posf, int *posr, int *numpos)
{
	int f, r;

	/* Up Left */
	f = onf;
	r = onr;
	for (;;) {
		f--;
		r++;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Up Right */
	f = onf;
	r = onr;
	for (;;) {
		f++;
		r++;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Down Left */
	f = onf;
	r = onr;
	for (;;) {
		f--;
		r--;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Down Right */
	f = onf;
	r = onr;
	for (;;) {
		f++;
		r--;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
}

static void possible_rook_moves(struct game_state_t * gs,
						  int onf, int onr, int *posf, int *posr, int *numpos)
{
	int f, r;

	/* Left */
	f = onf;
	r = onr;
	for (;;) {
		f--;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Right */
	f = onf;
	r = onr;
	for (;;) {
		f++;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Up */
	f = onf;
	r = onr;
	for (;;) {
		r++;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
	/* Down */
	f = onf;
	r = onr;
	for (;;) {
		r--;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			break;
		if (gs->board[f][r] != NOPIECE && iscolor(gs->board[f][r], gs->onMove))
			break;
		add_pos(f, r, posf, posr, numpos);
		if (gs->board[f][r] != NOPIECE)
			break;
	}
}

static void possible_queen_moves(struct game_state_t * gs,
							int onf, int onr, int *posf, int *posr, int *numpos)
{
	possible_rook_moves(gs, onf, onr, posf, posr, numpos);
	possible_bishop_moves(gs, onf, onr, posf, posr, numpos);
}

static void possible_king_moves(struct game_state_t * gs,
							int onf, int onr, int *posf, int *posr, int *numpos)
{
	static int kingJumps[8][2] = {{-1, -1}, {0, -1}, { 1, -1}, {-1, 1},
								  { 0,  1}, {1,  1}, {-1,  0}, { 1, 0}};
	int f, r, j;

	for (j = 0; j < 8; j++) {
		f = kingJumps[j][0] + onf;
		r = kingJumps[j][1] + onr;
		if (f < 0 || f > 7 || r < 0 || r > 7)
			continue;
		if (gs->board[f][r] == NOPIECE ||
		iscolor(gs->board[f][r], CToggle(gs->onMove)))
			add_pos(f, r, posf, posr, numpos);
	}
}

/* Doesn't check for check */
int legal_move(struct game_state_t * gs, 
			   int fFile, int fRank, int tFile, int tRank)
{
	int move_piece, legal;

	if (fFile == ALG_DROP) {
		move_piece = fRank;
		return ! ( move_piece == KING
			|| gs->holding[gs->onMove != WHITE][move_piece-1] == 0
			|| gs->board[tFile][tRank] != NOPIECE
			|| (move_piece == PAWN && (tRank == 0 || tRank == 7)) );
	} else
		move_piece = piecetype(gs->board[fFile][fRank]);

	if (gs->board[fFile][fRank] == NOPIECE				/* nothing there */
	|| !iscolor(gs->board[fFile][fRank], gs->onMove)	/* Wrong color */
	|| (gs->board[tFile][tRank] != NOPIECE &&
		iscolor(gs->board[tFile][tRank], gs->onMove))	/* Can't capture own */
	|| (fFile == tFile && fRank == tRank))				/* Same square */
		return 0;
	switch (move_piece) {
	case PAWN:   legal = legal_pawn_move(gs, fFile, fRank, tFile, tRank); break;
	case KNIGHT: legal = legal_knight_move(gs, fFile, fRank, tFile, tRank); break;
	case BISHOP: legal = legal_bishop_move(gs, fFile, fRank, tFile, tRank); break;
	case ROOK:   legal = legal_rook_move(gs, fFile, fRank, tFile, tRank); break;
	case QUEEN:  legal = legal_queen_move(gs, fFile, fRank, tFile, tRank); break;
	case KING:   legal = legal_king_move(gs, fFile, fRank, tFile, tRank); break;
	default:
		d_printf("legal_move(): bad piece type %d\n", move_piece);
		return 0;
	}
	return legal;
}

#define DROP_CHAR '@'

/* This fills in the rest of the mt structure once it is determined that
 * the move is legal. Returns MOVE_ILLEGAL if move leaves you in check */
static int move_calculate(struct game_state_t * gs, struct move_t * mt, int promote)
{
	struct game_state_t fakeMove;

	mt->pieceCaptured = gs->board[mt->toFile][mt->toRank];
	mt->enPassant = 0;		/* Don't know yet, let execute move take care
							   of it */
	if (mt->fromFile == ALG_DROP) {
		mt->piecePromotionTo = NOPIECE;
		sprintf(mt->moveString, "%s/%c%c-%c%d",
				wpstring[mt->fromRank],
				DROP_CHAR, DROP_CHAR,
				mt->toFile + 'a', mt->toRank + 1);
	} else {
		if (piecetype(gs->board[mt->fromFile][mt->fromRank]) == PAWN &&
				(mt->toRank == 0 || mt->toRank == 7))
			mt->piecePromotionTo = promote |
				colorval(gs->board[mt->fromFile][mt->fromRank]);
		else
			mt->piecePromotionTo = NOPIECE;

		if (piecetype(gs->board[mt->fromFile][mt->fromRank]) == PAWN &&
				abs(mt->fromRank - mt->toRank) == 2)
			mt->doublePawn = mt->fromFile;
		else
			mt->doublePawn = -1;

		if (piecetype(gs->board[mt->fromFile][mt->fromRank]) == KING &&
				mt->fromFile == 4 && mt->toFile == 2)
			sprintf(mt->moveString, "o-o-o");
		else if (piecetype(gs->board[mt->fromFile][mt->fromRank]) == KING &&
				mt->fromFile == 4 && mt->toFile == 6)
			sprintf(mt->moveString, "o-o");
		else
			sprintf(mt->moveString, "%s/%c%d-%c%d",
					wpstring[piecetype(gs->board[mt->fromFile][mt->fromRank])],
					mt->fromFile + 'a', mt->fromRank + 1,
					mt->toFile + 'a', mt->toRank + 1);
	}
	/* Replace this with an algebraic de-parser */

	sprintf(mt->algString, alg_unparse(gs, mt));
	fakeMove = *gs;
	/* Calculates en-passant also */
	execute_move(&fakeMove, mt, 0);

	/* Does making this move leave ME in check? */
	if (in_check(&fakeMove))
		return MOVE_ILLEGAL;
	/* IanO: bughouse variants: drop cannot be check/checkmate */

	return MOVE_OK;
}

int legal_andcheck_move(struct game_state_t * gs,
			int fFile, int fRank, int tFile, int tRank)
{
	struct move_t mt;

	if (!legal_move(gs, fFile, fRank, tFile, tRank))
		return 0;
	mt.color = gs->onMove;
	mt.fromFile = fFile;
	mt.fromRank = fRank;
	mt.toFile = tFile;
	mt.toRank = tRank;
	/* This should take into account a pawn promoting to another piece */
	return move_calculate(gs, &mt, QUEEN) == MOVE_OK;
}

/* in_check: checks if the side that is NOT about to move is in check
 */
int in_check(struct game_state_t * gs)
{
	int f, r, kf, kr, kpiece;

	/* Find the king */
	kpiece = gs->onMove == WHITE ? B_KING : W_KING;
	for (f = 0; f < 8; f++)
		for (r = 0; r < 8; r++)
			if (gs->board[f][r] == kpiece) {
				kf = f;
				kr = r;
				goto found;
			}
	d_printf( _("CHESSD: in_check() detected game with no king!\n"));
	return 0;

found:
	for (InitPieceLoop(gs->board, &f, &r, gs->onMove);
		 NextPieceLoop(gs->board, &f, &r, gs->onMove);)
	{
		if (legal_move(gs, f, r, kf, kr))	/* In Check? */
			return 1;
	}
	return 0;
}

int has_legal_move(struct game_state_t * gs)
{
	int i, f, r, kf = 0, kr = 0, numpossible = 0;
	int possiblef[500], possibler[500];

	for (InitPieceLoop(gs->board, &f, &r, gs->onMove);
		 NextPieceLoop(gs->board, &f, &r, gs->onMove);) {
		switch (piecetype(gs->board[f][r])) {
		case PAWN:
			possible_pawn_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		case KNIGHT:
			possible_knight_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		case BISHOP:
			possible_bishop_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		case ROOK:
			possible_rook_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		case QUEEN:
			possible_queen_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		case KING:
			kf = f;
			kr = r;
			possible_king_moves(gs, f, r, possiblef, possibler, &numpossible);
			break;
		}
		if (numpossible >= 500)
			d_printf( _("CHESSD: has_legal_move(): Possible move overrun\n"));
	
		for (i = 0; i < numpossible; i++)
			if (legal_andcheck_move(gs, f, r, possiblef[i], possibler[i]))
				return 1;
	}

	/* IanO:  if we got here, then kf and kr must be set */
	if (gs->gameNum >=0 && game_globals.garray[gs->gameNum].link >= 0) {
		/* bughouse: potential drops as check interpositions */
		gs->holding[gs->onMove != WHITE][QUEEN - 1]++;
		for (f = kf-1; f <= kf+1; f++) 
			for (r = kr-1; r <= kr+1; r++) {
				if (f>=0 && f<8 && r>=0 && r<8 && gs->board[f][r] == NOPIECE) {
					/* try a drop next to the king */
					if (legal_andcheck_move(gs, ALG_DROP, QUEEN, f, r)) {
						gs->holding[gs->onMove != WHITE][QUEEN - 1]--;
						return 1;
					}
				}
			}
		gs->holding[gs->onMove != WHITE][QUEEN - 1]--;
	}

	return 0;
}

/* This will end up being a very complicated function */
int parse_move(char *mstr, struct game_state_t *gs, struct move_t *mt, int promote)
{
	int type = is_move(mstr), result;

	mt->color = gs->onMove;
	switch (type) {
	case MS_COMP:
		mt->fromFile = mstr[0] - 'a';
		mt->fromRank = mstr[1] - '1';
		mt->toFile = mstr[2] - 'a';
		mt->toRank = mstr[3] - '1';
		break;
	case MS_COMPDASH:
		mt->fromFile = mstr[0] - 'a';
		mt->fromRank = mstr[1] - '1';
		mt->toFile = mstr[3] - 'a';
		mt->toRank = mstr[4] - '1';
		break;
	case MS_KCASTLE:
		mt->fromFile = 4;
		mt->toFile = 6;
		if (gs->onMove == WHITE) {
			mt->fromRank = 0;
			mt->toRank = 0;
		} else {
			mt->fromRank = 7;
			mt->toRank = 7;
		}
		break;
	case MS_QCASTLE:
		mt->fromFile = 4;
		mt->toFile = 2;
		if (gs->onMove == WHITE) {
			mt->fromRank = 0;
			mt->toRank = 0;
		} else {
			mt->fromRank = 7;
			mt->toRank = 7;
		}
		break;
	case MS_ALG:
		/* Fills in the mt structure */
		if ((result = alg_parse_move(mstr, gs, mt)) != MOVE_OK)
			return result;
		break;
	case MS_NOTMOVE:
	default:
		return MOVE_ILLEGAL;
	}
	if (!legal_move(gs, mt->fromFile, mt->fromRank, mt->toFile, mt->toRank))
		return MOVE_ILLEGAL;
	return move_calculate(gs, mt, promote);
}

/* Returns MOVE_OK, MOVE_NOMATERIAL, MOVE_CHECKMATE, or MOVE_STALEMATE */
/* check_game_status prevents recursion */
int execute_move(struct game_state_t *gs, struct move_t *mt, int check_game_status)
{
	int movedPiece, tookPiece, i, j, strength;

	if (mt->fromFile == ALG_DROP) {
		movedPiece = mt->fromRank;
		tookPiece = NOPIECE;
		gs->holding[gs->onMove != WHITE][movedPiece-1]--;
		gs->board[mt->toFile][mt->toRank] = movedPiece | gs->onMove;
		if (gs->gameNum >= 0)
			gs->lastIrreversible = game_globals.garray[gs->gameNum].numHalfMoves;
	} else {
		movedPiece = gs->board[mt->fromFile][mt->fromRank];
		tookPiece = gs->board[mt->toFile][mt->toRank];
		if (mt->piecePromotionTo == NOPIECE)
			gs->board[mt->toFile][mt->toRank] = gs->board[mt->fromFile][mt->fromRank];
		else
			gs->board[mt->toFile][mt->toRank] = mt->piecePromotionTo | gs->onMove;
		gs->board[mt->fromFile][mt->fromRank] = NOPIECE;
		/* Check if irreversible */
		if ((piecetype(movedPiece) == PAWN) || (tookPiece != NOPIECE)) {
			if (gs->gameNum >= 0)
				gs->lastIrreversible = game_globals.garray[gs->gameNum].numHalfMoves;
		}
		/* Check if this move is en-passant */
		if ((piecetype(movedPiece) == PAWN) && (mt->fromFile != mt->toFile) 
			&& (tookPiece == NOPIECE))
		{
			mt->pieceCaptured = gs->onMove == WHITE ? B_PAWN : W_PAWN;
			mt->enPassant = mt->fromFile > mt->toFile ? -1 : 1;
			gs->board[mt->toFile][mt->fromRank] = NOPIECE;
		}
		/* Check en-passant flags for next moves */
		for (i = 0; i < 8; i++) {
			gs->ep_possible[0][i] = 0;
			gs->ep_possible[1][i] = 0;
		}
	
		if ((piecetype(movedPiece) == PAWN) &&
		((mt->fromRank == mt->toRank + 2) || (mt->fromRank + 2 == mt->toRank))) {
			/* Should turn on en-passant flag if possible */
			if (gs->onMove == WHITE) {
				if ((mt->toFile < 7) && gs->board[mt->toFile + 1][3] == B_PAWN)
					gs->ep_possible[1][mt->toFile + 1] = -1;
				if ((mt->toFile - 1 >= 0) && gs->board[mt->toFile - 1][3] == B_PAWN)
					gs->ep_possible[1][mt->toFile - 1] = 1;
			} else {
				if ((mt->toFile < 7) && gs->board[mt->toFile + 1][4] == W_PAWN)
					gs->ep_possible[0][mt->toFile + 1] = -1;
				if ((mt->toFile - 1 >= 0) && gs->board[mt->toFile - 1][4] == W_PAWN)
					gs->ep_possible[0][mt->toFile - 1] = 1;
			}
		}
		if ((piecetype(movedPiece) == ROOK) && (mt->fromFile == 0)) {
			if ((mt->fromRank == 0) && (gs->onMove == WHITE))
				gs->wqrmoved = 1;
			else if ((mt->fromRank == 7) && (gs->onMove == BLACK))
				gs->bqrmoved = 1;
		}
		if ((piecetype(movedPiece) == ROOK) && (mt->fromFile == 7)) {
			if ((mt->fromRank == 0) && (gs->onMove == WHITE))
				gs->wkrmoved = 1;
			else if ((mt->fromRank == 7) && (gs->onMove == BLACK))
				gs->bkrmoved = 1;
		}
		if (piecetype(movedPiece) == KING) {
			if (gs->onMove == WHITE)
				gs->wkmoved = 1;
			else
				gs->bkmoved = 1;
		}
		if ((piecetype(movedPiece) == KING) &&
		((mt->fromFile == 4) && ((mt->toFile == 6)))) {	/* Check for KS castling */
			gs->board[5][mt->toRank] = gs->board[7][mt->toRank];
			gs->board[7][mt->toRank] = NOPIECE;
		}
		if ((piecetype(movedPiece) == KING) &&
		((mt->fromFile == 4) && ((mt->toFile == 2)))) {	/* Check for QS castling */
			gs->board[3][mt->toRank] = gs->board[0][mt->toRank];
			gs->board[0][mt->toRank] = NOPIECE;
		}
	}
	
	if (gs->onMove == BLACK)
		gs->moveNum++;

	if (check_game_status) {
		/* Does this move result in check? */
		if (in_check(gs)) {
			/* Check for checkmate */
			gs->onMove = CToggle(gs->onMove);
			if (!has_legal_move(gs))
				return MOVE_CHECKMATE;
		} else {
			/* Check for stalemate */
			gs->onMove = CToggle(gs->onMove);
			if (!has_legal_move(gs))
				return MOVE_STALEMATE;

/* loon: check for insufficient mating material, first try
   The rule is discussed further here: http://www.e4ec.org/immr.html
   and here: http://maverickphilosopher.powerblogs.com/posts/1114483599.shtml 
 */
			strength = 0;
			for (i=0; i<8; i++) {
				for (j=0; j<8; j++) {
					switch(piecetype(gs->board[i][j])) {
					case KNIGHT:
					case BISHOP:
						strength++;
						break;
					case KING:
					case NOPIECE:
						break;
					default:
						strength = 2;
						break;
					}
				}
			}
			if (strength < 2)
				return MOVE_NOMATERIAL;
		}
	} else
		gs->onMove = CToggle(gs->onMove);
	return MOVE_OK;
}

int backup_move(int g, int mode)
{
	struct game *gg = &game_globals.garray[g];
	struct game_state_t *gs;
	struct move_t *m, *m1;
	int now, i;

	if (gg->link >= 0)	/*IanO: not implemented for bughouse yet */
		return MOVE_ILLEGAL;
	if (gg->numHalfMoves < 1)
		return MOVE_ILLEGAL;
	gs = &gg->game_state;
	m = (mode==REL_GAME) ? &gg->moveList[gg->numHalfMoves - 1] 
						 : &gg->examMoveList[gg->numHalfMoves - 1];
	if (m->toFile < 0)
		return MOVE_ILLEGAL;
	
	gs->board[m->fromFile][m->fromRank] = gs->board[m->toFile][m->toRank];
	if (m->piecePromotionTo != NOPIECE) {
		gs->board[m->fromFile][m->fromRank] = PAWN |
			colorval(gs->board[m->fromFile][m->fromRank]);
	}
	
	/* When takeback a _first_ move of rook, the ??rmoved variable
	 * must be cleared. To check, if the move is first, we should scan moveList.
	 */
	if (piecetype(gs->board[m->fromFile][m->fromRank]) == ROOK) {
		if (m->color == WHITE) {
			if (m->fromFile == 0 && m->fromRank == 0) {
				for (i = 2; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if (m1->fromFile == 0 && m1->fromRank == 0)
						break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->wqrmoved = 0;
			}
			if (m->fromFile == 7 && m->fromRank == 0) {
				for (i = 2; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if (m1->fromFile == 7 && m1->fromRank == 0)
						break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->wkrmoved = 0;
			}
		} else {
			if (m->fromFile == 0 && m->fromRank == 7) {
				for (i = 3; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if ((m1->fromFile == 0) && (m1->fromRank == 0))
					break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->bqrmoved = 0;
			}
			if (m->fromFile == 7 && m->fromRank == 7) {
				for (i = 3; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if (m1->fromFile == 7 && m1->fromRank == 0)
						break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->bkrmoved = 0;
			}
		}
	}
	
	if (piecetype(gs->board[m->fromFile][m->fromRank]) == KING) {
		gs->board[m->toFile][m->toRank] = m->pieceCaptured;
	
		if (m->toFile - m->fromFile == 2) {
			gs->board[7][m->fromRank] = ROOK |
			colorval(gs->board[m->fromFile][m->fromRank]);
			gs->board[5][m->fromRank] = NOPIECE;
		
			/* If takeback a castling, the appropriates ??moved variables
			 * must be cleared
			 */
			if (m->color == WHITE) {
				gs->wkmoved = 0;
				gs->wkrmoved = 0;
			} else {
				gs->bkmoved = 0;
				gs->bkrmoved = 0;
			}
			goto cleanupMove;
		}
		if (m->fromFile - m->toFile == 2) {
			gs->board[0][m->fromRank] = ROOK | 
				colorval(gs->board[m->fromFile][m->fromRank]);
			gs->board[3][m->fromRank] = NOPIECE;
		
			/* If takeback a castling, the appropriate ??moved variables
			 * must be cleared
			 */
			if (m->color == WHITE) {
				gs->wkmoved = 0;
				gs->wqrmoved = 0;
			} else {
				gs->bkmoved = 0;
				gs->bqrmoved = 0;
			}
			goto cleanupMove;
		}
		/* When takeback a _first_ move of king (not the castling),
		 * the ?kmoved variable must be cleared . To check, if the move is first,
		 * we should scan moveList.
		 */
	
		if (m->color == WHITE) {
			if ((m->fromFile == 4) && (m->fromRank == 0)) {
				for (i = 2; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if ((m1->fromFile == 4) && (m1->fromRank == 0))
						break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->wkmoved = 0;
			}
		} else {
			if ((m->fromFile == 4) && (m->fromRank == 7)) {
				for (i = 3; i < gg->numHalfMoves - 1; i += 2) {
					m1 = mode==REL_GAME ? &gg->moveList[i] : &gg->examMoveList[i];
					if ((m1->fromFile == 4) && (m1->fromRank == 7))
						break;
				}
				if (i == gg->numHalfMoves - 1)
					gs->bkmoved = 0;
			}
		}
	}
	
	if (m->enPassant) {		/* Do enPassant */
		gs->board[m->toFile][m->fromRank] = PAWN |
			(colorval(gs->board[m->fromFile][m->fromRank]) == WHITE ? BLACK : WHITE);
		gs->board[m->toFile][m->toRank] = NOPIECE;
		/* Should set the enpassant array, but I don't care right now */
		goto cleanupMove;
	}
	gs->board[m->toFile][m->toRank] = m->pieceCaptured;

cleanupMove:
	if (gg->status != GAME_EXAMINE)
		game_update_time(g);

	gg->numHalfMoves--;
	if (gg->status != GAME_EXAMINE) {
		if (gg->wInitTime) {	/* Don't update times in untimed games */
			int btimeseal = net_globals.con[player_globals.parray[gg->black].socket]->timeseal,
				wtimeseal = net_globals.con[player_globals.parray[gg->white].socket]->timeseal;
			now = tenth_secs();
		
			if (m->color == WHITE) {
				if (wtimeseal) {  /* white uses timeseal? */
					gg->wRealTime += (m->tookTime * 100);
					gg->wRealTime -= (gg->wIncrement * 100);
					gg->wTime = gg->wRealTime / 100;
					if (btimeseal) /* opp uses timeseal? */
						gg->bTime = gg->bRealTime / 100;
					else    /* opp has no timeseal */
						gg->bTime += (gg->lastDecTime - gg->lastMoveTime);
				} else {  /* white has no timeseal */
					gg->wTime += m->tookTime;
					gg->wTime -= gg->wIncrement;
					if (btimeseal) /* opp uses timeseal? */
						gg->bTime = gg->bRealTime / 100;
					else    /* opp has no timeseal */
						gg->bTime += (gg->lastDecTime - gg->lastMoveTime);
				}
			} else {
				if (btimeseal) {  /* black uses timeseal? */
					gg->bRealTime += (m->tookTime * 100);
					gg->bRealTime -= (gg->wIncrement * 100);
					gg->bTime = gg->bRealTime / 100;
					if (wtimeseal) /* opp uses timeseal? */
						gg->wTime = gg->wRealTime / 100;
					else    /* opp has no timeseal */
						gg->wTime += (gg->lastDecTime - gg->lastMoveTime);
				} else {  /* black has no timeseal */
					gg->bTime += m->tookTime;
					gg->bTime -= gg->bIncrement ? gg->bIncrement : gg->wIncrement;
					if (wtimeseal) /* opp uses timeseal? */
						gg->wTime = gg->wRealTime / 100;
					else    /* opp has no timeseal */
						gg->wTime += (gg->lastDecTime - gg->lastMoveTime);
				}
			}
		
			if (gg->numHalfMoves == 0)
				gg->timeOfStart = now;
				gg->lastMoveTime = gg->lastDecTime = now;
		}
	}
	if (gs->onMove == BLACK)
		gs->onMove = WHITE;
	else {
		gs->onMove = BLACK;
		gs->moveNum--;
	}

	/* Takeback of last move is done already, it's time to update enpassant
	array.  (patch from Soso, added by Sparky 3/17/95) */

	if (gg->numHalfMoves > 0) {
		m1 = mode == REL_GAME ? &gg->moveList[gg->numHalfMoves - 1] 
							  : &gg->examMoveList[gg->numHalfMoves - 1];
		if (piecetype(gs->board[m1->toFile][m1->toRank]) == PAWN) {
			if ((m1->toRank - m1->fromRank) == 2) {
				if ((m1->toFile < 7) && gs->board[m1->toFile + 1][3] == B_PAWN)
					gs->ep_possible[1][m1->toFile + 1] = -1;
				if ((m1->toFile - 1 >= 0) && gs->board[m1->toFile - 1][3] == B_PAWN)
					gs->ep_possible[1][m1->toFile - 1] = 1;
			}
			else if ((m1->toRank - m1->fromRank) == -2) {
				if ((m1->toFile < 7) && gs->board[m1->toFile + 1][4] == W_PAWN)
					gs->ep_possible[0][m1->toFile + 1] = -1;
				if ((m1->toFile - 1 >= 0) && gs->board[m1->toFile - 1][4] == W_PAWN)
					gs->ep_possible[0][m1->toFile - 1] = 1;
			}
		}
	}

	return MOVE_OK;
}
