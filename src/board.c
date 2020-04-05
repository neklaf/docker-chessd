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
#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include "gamedb.h"
#include "globals.h"
#include "playerdb.h"
#include "utils.h"
#include "chessd_locale.h"
#include "configuration.h"

const char *wpstring[] = {" ", "P", "N", "B", "R", "Q", "K"};
static const char *bpstring[] = {" ", "p", "n", "b", "r", "q", "k"};

static int pieceValues[7] = {0, 1, 3, 3, 5, 9, 0};

static const int mach_type = (1<<7) | (1<<8) | (1<<9) | (1<<10) | (1<<11);
#define IsMachineStyle(n) (((1<<(n)) & mach_type) != 0)

char bstring[MAX_BOARD_STRING_LEN];

static int board_read_file(char *category, char *gname, struct game_state_t *gs);
static void wild_update(int style);

static int style1(struct game_state_t *b, struct move_t *ml);
static int style2(struct game_state_t *b, struct move_t *ml);
static int style3(struct game_state_t *b, struct move_t *ml);
static int style4(struct game_state_t *b, struct move_t *ml);
static int style5(struct game_state_t *b, struct move_t *ml);
static int style6(struct game_state_t *b, struct move_t *ml);
static int style7(struct game_state_t *b, struct move_t *ml);
static int style8(struct game_state_t *b, struct move_t *ml);
static int style9(struct game_state_t *b, struct move_t *ml);
static int style10(struct game_state_t *b, struct move_t *ml);
static int style11(struct game_state_t *b, struct move_t *ml);
static int style12(struct game_state_t *b, struct move_t *ml);
static int style13(struct game_state_t *b, struct move_t *ml);

static int (*styleFuncs[MAX_STYLES])() = {
	style1,
	style2,
	style3,
	style4,
	style5,
	style6,
	style7,
	style8,
	style9,
	style10,
	style11,
	style12,
	style13
};


static void reset_board_vars(struct game_state_t *gs)
{
	int f,r;
	
	for (f = 0; f < 2; f++) {
		for (r = 0; r < 8; r++)
			gs->ep_possible[f][r] = 0;
		for (r = PAWN; r <= QUEEN; r++)
			gs->holding[f][r-PAWN] = 0;
	}
	gs->wkmoved = gs->wqrmoved = gs->wkrmoved = 0;
	gs->bkmoved = gs->bqrmoved = gs->bkrmoved = 0;
	gs->onMove = WHITE;
	gs->moveNum = 1;
	gs->lastIrreversible = -1;
	gs->gameNum = -1;
}

void board_clear(struct game_state_t *gs)
{
	int f,r;
	
	for (f = 0; f < 8; f++)
		for (r = 0; r < 8; r++)
			gs->board[f][r] = NOPIECE;
	reset_board_vars(gs);
}

void board_standard(struct game_state_t *gs)
{
	int f,r;
	
	board_clear(gs);
	for (f = 0; f < 8; f++){
		gs->board[f][1] = W_PAWN;
		gs->board[f][6] = B_PAWN;
	}
	gs->board[0][0] = W_ROOK;
	gs->board[1][0] = W_KNIGHT;
	gs->board[2][0] = W_BISHOP;
	gs->board[3][0] = W_QUEEN;
	gs->board[4][0] = W_KING;
	gs->board[5][0] = W_BISHOP;
	gs->board[6][0] = W_KNIGHT;
	gs->board[7][0] = W_ROOK;
	gs->board[0][7] = B_ROOK;
	gs->board[1][7] = B_KNIGHT;
	gs->board[2][7] = B_BISHOP;
	gs->board[3][7] = B_QUEEN;
	gs->board[4][7] = B_KING;
	gs->board[5][7] = B_BISHOP;
	gs->board[6][7] = B_KNIGHT;
	gs->board[7][7] = B_ROOK;
}

int board_init(int g,struct game_state_t *b, char *category, char *board)
{
	int retval = 0;
	int wval;

	if (!category || !board || !category[0] || !board[0])
		/* accounts for bughouse too */
		board_standard(b);
	else {
		// WILD games
		if (strcmp(category, "wild") == 0) {
			if (sscanf(board, "%d", &wval) == 1 && wval >= 1 && wval <= 4)
				wild_update(wval);
		}
		retval = board_read_file(category, board, b);
	}
	MakeFENpos(g, game_globals.garray[g].FENstartPos);
	return retval;
}

void board_calc_strength(struct game_state_t *b, int *ws, int *bs)
{
	int r, f, *p;
	
	*ws = *bs = 0;
	for (f = 0; f < 8; f++) {
		for (r = 0; r < 8; r++) {
			p = colorval(b->board[r][f]) == WHITE ? ws : bs;
			*p += pieceValues[piecetype(b->board[r][f])];
		}
	}
	for (r = PAWN; r <= QUEEN; r++) {
		*ws += b->holding[0][r-1] * pieceValues[r];
		*bs += b->holding[1][r-1] * pieceValues[r];
	}
}

static char *holding_str(int *holding)
{
	static char tmp[32]; /* era 30 */
	int p,i,j;

	i = 0;
	for (p = PAWN; p <= QUEEN; p++)
		for (j = 0; j < holding[p-1] && i < 30; j++)
			tmp[i++] = wpstring[p][0];
	tmp[i] = '\0';
	return tmp;
}

static char *append_holding_machine(char *buf, int g, int c, int p)
{
	struct game_state_t *gs = &game_globals.garray[g].game_state;
	char tmp[50];
	
	sprintf(tmp, U_("<b1> game %d white [%s] black ["), g+1, holding_str(gs->holding[0])); /* xboard */

	strcat(tmp, holding_str(gs->holding[1]));
	strcat(buf, tmp);
	if (p) {
		sprintf(tmp, "] <- %c%s\n", "WB"[c], wpstring[p]);
		strcat(buf, tmp);
	} else
		strcat(buf, "]\n");
	return buf;
}

static char *append_holding_display(char *buf, struct game_state_t *gs, int white)
{
	strcat(buf, white ? "White" : "Black");
	strcat(buf, " holding: [");
	strcat(buf, holding_str(gs->holding[white ? 0 : 1]));
	strcat(buf, "]\n");
	return buf;
}

void update_holding(int g, int pieceCaptured)
{
	int p = piecetype(pieceCaptured);
	int c = colorval(pieceCaptured);
	struct game_state_t *gs = &game_globals.garray[g].game_state;
	int pp, pl;
	char tmp1[80], tmp2[80];
	
	if (c == WHITE) {
		c = 0;
		pp = game_globals.garray[g].white;
	} else {
		c = 1;
		pp = game_globals.garray[g].black;
	}
	gs->holding[c][p-1]++;
	tmp1[0] = '\0';
	append_holding_machine(tmp1, g, c, p);
	sprintf(tmp2, "Game %d %s received: %s -> [%s]\n", g+1,
	player_globals.parray[pp].name, wpstring[p], holding_str(gs->holding[c]));
	for (pl = 0; pl < player_globals.p_num; pl++) {
		if (player_globals.parray[pl].status == PLAYER_EMPTY)
			continue;
		if (player_is_observe(pl, g) || (player_globals.parray[pl].game == g))
			pprintf_prompt(pl, IsMachineStyle(player_globals.parray[pl].style) ? tmp1 : tmp2);
	}
}


/* Globals used for each board */
static int wTime, bTime;
static int orient;
static int myTurn;		/* 1 = my turn, 0 = observe, -1 = other turn */
 /* 2 = examiner, -2 = observing examiner */
 /* -3 = just send position (spos/refresh) */

char *board_to_string(char *wn, char *bn, int wt, int bt,
					  struct game_state_t *b, struct move_t *ml, int style,
					  int orientation, int relation, int p)
{
	struct game *gg = &game_globals.garray[b->gameNum];
	int bh = (b->gameNum >= 0 && gg->link >= 0);
	int nhm;

	orient = orientation;
	myTurn = relation;
	wTime = 0;
	bTime = 0;
	/* when examining we calculate times based on the time left when the
	   move happened, not current time */
	if (gg->status == GAME_EXAMINE) {
		nhm = gg->numHalfMoves;
		if (nhm > 0) {
			wTime = ml[nhm - 1].wTime;
			bTime = ml[nhm - 1].bTime;
		} else {
			wTime = gg->wInitTime;
			bTime = gg->bInitTime;
		}
	}
	
	/* cope with old stored games */
	if (wTime == 0) wTime = wt;
	if (bTime == 0) bTime = bt;
	
	if (style < 0 || style >= MAX_STYLES)
		return NULL;
	
	if (style != 11) {		/* game header */
		sprintf(bstring, _("Game %d (%s vs. %s)\n\n"),
				b->gameNum + 1, gg->white_name, gg->black_name);
	} else
		bstring[0] = '\0';

	if (bh && !IsMachineStyle(style))
		append_holding_display(bstring, b, orientation == BLACK);
	
	if (styleFuncs[style] (b, ml))
		return NULL;
	
	if (bh) {
		if (IsMachineStyle(style))
			append_holding_machine(bstring, b->gameNum, 0, 0);
		else
			append_holding_display(bstring, b, orientation == WHITE);
	}
	return bstring;
}

char *move_and_time(struct move_t *m)
{
	static char tmp[80];
	sprintf(tmp, "%-7s (%s)", m->algString, tenth_str(m->tookTime, 0));
	return tmp;
}

/* The following take the game state and whole move list */

static int genstyle(struct game_state_t *b, struct move_t *ml, 
					const char *wp[], const char *bp[], const char *wsqr, const char *bsqr,
					const char *top, const char *mid, const char *start, const char *end,
					const char *label, const char *blabel)
{
	int f, r, count, first, last, inc, ws, bs;
	char tmp[800];
	struct game *gg = &game_globals.garray[b->gameNum];
	
	board_calc_strength(b, &ws, &bs);
	if (orient == WHITE) {
		first = 7;
		last = 0;
		inc = -1;
	} else {
		first = 0;
		last = 7;
		inc = 1;
	}
	strcat(bstring, top);
	for (f = first, count = 7; f != last + inc; f += inc, count--) {
		sprintf(tmp, "     %d  %s", f + 1, start);
		strcat(bstring, tmp);
		for (r = last; r != first - inc; r = r - inc) {
			strcat(bstring, square_color(r, f) == WHITE ? wsqr : bsqr);
			if (piecetype(b->board[r][f]) == NOPIECE) {
				strcat(bstring, square_color(r, f) == WHITE ? bp[0] : wp[0]);
			} else {
				if (colorval(b->board[r][f]) == WHITE)
					strcat(bstring, wp[piecetype(b->board[r][f])]);
				else
					strcat(bstring, bp[piecetype(b->board[r][f])]);
			}
		}
		sprintf(tmp, "%s", end);
		strcat(bstring, tmp);
		switch (count) {
		case 7:
			sprintf(tmp, _("     Move # : %d (%s)"), b->moveNum, CString(b->onMove));
			strcat(bstring, tmp);
			break;
		case 6:
			/*    if ((b->moveNum > 1) || (b->onMove == BLACK)) {  */
			/* The change from the above line to the one below is a kludge by hersco. */
			if (gg->numHalfMoves > 0) {
				/* loon: think this fixes the crashing ascii board on takeback bug */
				sprintf(tmp, _("     %s Moves : '%s'"), CString(CToggle(b->onMove)),
						move_and_time(&ml[gg->numHalfMoves - 1]));
				strcat(bstring, tmp);
			}
			break;
		case 5:
			break;
		case 4:
			sprintf(tmp, _("     Black Clock : %s"), tenth_str((bTime > 0 ? bTime : 0), 1));
			strcat(bstring, tmp);
			break;
		case 3:
			sprintf(tmp, _("     White Clock : %s"), tenth_str((wTime > 0 ? wTime : 0), 1));
			strcat(bstring, tmp);
			break;
		case 2:
			sprintf(tmp, U_("     Black Strength : %d"), bs); /* xboard */
			strcat(bstring, tmp);
			break;
		case 1:
			sprintf(tmp, _("     White Strength : %d"), ws);
			strcat(bstring, tmp);
			break;
		case 0:
			break;
		}
		strcat(bstring, "\n");
		strcat(bstring, count != 0 ? mid : top);
	}
	strcat(bstring, orient == WHITE ? label : blabel);
	return 0;
}

/* Experimental ANSI board for colour representation */
static int style13(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"   ", "\033[37m\033[1m P ",  "\033[37m\033[1m N ",
							          "\033[37m\033[1m B ",  "\033[37m\033[1m R ",
									  "\033[37m\033[1m Q ",  "\033[37m\033[1m K "};
	static const char *bp[] = {"   ", "\033[21m\033[37m P ", "\033[21m\033[37m N ",
									  "\033[21m\033[37m B ", "\033[21m\033[37m R ",
									  "\033[21m\033[37m Q ", "\033[21m\033[37m K "};
	static const char *wsqr = "\033[40m";
	static const char *bsqr = "\033[45m";
	static const char *top = "\t+------------------------+\n";
	static const char *mid = "";
	static const char *start = "|";
	static const char *end = "\033[0m|";
	static const char *label  = "\t  a  b  c  d  e  f  g  h\n";
	static const char *blabel = "\t  h  g  f  e  d  c  b  a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Standard ICS */
static int style1(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"   |", " P |", " N |", " B |", " R |", " Q |", " K |"};
	static const char *bp[] = {"   |", " *P|", " *N|", " *B|", " *R|", " *Q|", " *K|"};
	static char *wsqr = "";
	static char *bsqr = "";
	static char *top = "\t---------------------------------\n";
	static char *mid = "\t|---+---+---+---+---+---+---+---|\n";
	static char *start = "|";
	static char *end = "";
	static char *label  = "\t  a   b   c   d   e   f   g   h\n";
	static char *blabel = "\t  h   g   f   e   d   c   b   a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* USA-Today Sports Center-style board */
static int style2(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"+  ", "P  ", "N  ", "B  ", "R  ", "Q  ", "K  "};
	static const char *bp[] = {"-  ", "p' ", "n' ", "b' ", "r' ", "q' ", "k' "};
	static char *wsqr = "";
	static char *bsqr = "";
	static char *top = "";
	static char *mid = "";
	static char *start = "";
	static char *end = "";
	static char *label  = "\ta  b  c  d  e  f  g  h\n";
	static char *blabel = "\th  g  f  e  d  c  b  a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Experimental vt-100 ANSI board for dark backgrounds */
static int style3(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"   ", " P ", " N ", " B ", " R ", " Q ", " K "};
	static const char *bp[] = {"   ", " *P", " *N", " *B", " *R", " *Q", " *K"};
	static char *wsqr = "\033[0m";
	static char *bsqr = "\033[7m";
	static char *top = "\t+------------------------+\n";
	static char *mid = "";
	static char *start = "|";
	static char *end = "\033[0m|";
	static char *label  = "\t  a  b  c  d  e  f  g  h\n";
	static char *blabel = "\t  h  g  f  e  d  c  b  a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Experimental vt-100 ANSI board for light backgrounds */
static int style4(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"   ", " P ", " N ", " B ", " R ", " Q ", " K "};
	static const char *bp[] = {"   ", " *P", " *N", " *B", " *R", " *Q", " *K"};
	static char *wsqr = "\033[7m";
	static char *bsqr = "\033[0m";
	static char *top = "\t+------------------------+\n";
	static char *mid = "";
	static char *start = "|";
	static char *end = "\033[0m|";
	static char *label  = "\t  a  b  c  d  e  f  g  h\n";
	static char *blabel = "\t  h  g  f  e  d  c  b  a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Style suggested by ajpierce@med.unc.edu */
static int style5(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"    ", "  o ", " :N:", " <B>", " |R|", " {Q}", " =K="};
	static const char *bp[] = {"    ", "  p ", " :n:", " <b>", " |r|", " {q}", " =k="};
	static char *wsqr = "";
	static char *bsqr = "";
	static char *top = "        .   .   .   .   .   .   .   .   .\n";
	static char *mid = "        .   .   .   .   .   .   .   .   .\n";
	static char *start = "";
	static char *end = "";
	static char *label  = "\t  a   b   c   d   e   f   g   h\n";
	static char *blabel = "\t  h   g   f   e   d   c   b   a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Email Board suggested by Thomas Fought (tlf@rsch.oclc.org) */
static int style6(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"    |", " wp |", " WN |", " WB |", " WR |", " WQ |", " WK |"};
	static const char *bp[] = {"    |", " bp |", " BN |", " BB |", " BR |", " BQ |", " BK |"};
	static char *wsqr = "";
	static char *bsqr = "";
	static char *top = "\t-----------------------------------------\n";
	static char *mid = "\t-----------------------------------------\n";
	static char *start = "|";
	static char *end = "";
	static char *label  = "\t  A    B    C    D    E    F    G    H\n";
	static char *blabel = "\t  H    G    F    E    D    C    B    A\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* Miniature board */
static int style7(struct game_state_t *b, struct move_t *ml)
{
	static const char *wp[] = {"  ", " P", " N", " B", " R", " Q", " K"};
	static const char *bp[] = {" -", " p", " n", " b", " r", " q", " k"};
	static char *wsqr = "";
	static char *bsqr = "";
	static char *top = "\t:::::::::::::::::::::\n";
	static char *mid = "";
	static char *start = "..";
	static char *end = " ..";
	static char *label  = "\t   a b c d e f g h\n";
	static char *blabel = "\t   h g f e d c b a\n";
	
	return genstyle(b, ml, wp, bp, wsqr, bsqr, top, mid, start, end, label, blabel);
}

/* ICS interface maker board-- raw data dump */
static int style8(struct game_state_t *b, struct move_t *ml)
{
	char tmp[800];
	int f, r, ws, bs;
	struct game *gg = &game_globals.garray[b->gameNum];

	board_calc_strength(b, &ws, &bs);
	sprintf(tmp, "#@#%03d%-16s%s%-16s%s", b->gameNum + 1,
			gg->white_name, orient == WHITE ? "*" : ":",
			gg->black_name, orient == WHITE ? ":" : "*");
	strcat(bstring, tmp);
	for (r = 0; r < 8; r++) {
		for (f = 0; f < 8; f++) {
			if (b->board[f][r] == NOPIECE) {
				strcat(bstring, " ");
			} else {
				if (colorval(b->board[f][r]) == WHITE)
					strcat(bstring, wpstring[piecetype(b->board[f][r])]);
				else
					strcat(bstring, bpstring[piecetype(b->board[f][r])]);
			}
		}
	}
	sprintf(tmp, "%03d%s%02d%02d%05d%05d%-7s(%s)@#@\n",
			gg->numHalfMoves / 2 + 1,
			(b->onMove == WHITE) ? "W" : "B",
			ws,
			bs,
			(wTime + 5) / 10,
			(bTime + 5) / 10,
			gg->numHalfMoves ? ml[gg->numHalfMoves - 1].algString : "none",
			gg->numHalfMoves ? tenth_str(ml[gg->numHalfMoves - 1].tookTime, 0) : "0:00");
	strcat(bstring, tmp);
	return 0;
}

/* last 2 moves only (previous non-verbose mode) */
static int style9(struct game_state_t *b, struct move_t *ml)
{
	int i, count, startmove;
	char tmp[800];
	struct game *gg = &game_globals.garray[b->gameNum];
	
	sprintf(tmp, U_("\nMove     %-23s%s\n"), /* xboard */
			gg->white_name, gg->black_name);
	strcat(bstring, tmp);
	sprintf(tmp, "----     --------------         --------------\n");
	strcat(bstring, tmp);
	startmove = ((gg->numHalfMoves - 3) / 2) * 2;
	if (startmove < 0)
		startmove = 0;
	for (i = startmove, count = 0; i < gg->numHalfMoves && count < 4; i++, count++) {
		if (!(i & 1)) {
			sprintf(tmp, "  %2d     ", i/2 + 1);
			strcat(bstring, tmp);
		}
		sprintf(tmp, "%-23s", move_and_time(&ml[i]));
		strcat(bstring, tmp);
		if (i & 1)
			strcat(bstring, "\n");
	}
	if (i & 1)
		strcat(bstring, "\n");
	return 0;
}

/* Sleator's 'new and improved' raw dump format... */
static int style10(struct game_state_t *b, struct move_t *ml)
{
	int f, r, ws, bs;
	char tmp[800];
	struct game *gg = &game_globals.garray[b->gameNum];
	
	board_calc_strength(b, &ws, &bs);
	sprintf(tmp, "<10>\n");
	strcat(bstring, tmp);
	for (r = 7; r >= 0; r--) {
		strcat(bstring, "|");
		for (f = 0; f < 8; f++) {
			if (b->board[f][r] == NOPIECE) {
				strcat(bstring, " ");
			} else {
				if (colorval(b->board[f][r]) == WHITE)
					strcat(bstring, wpstring[piecetype(b->board[f][r])]);
				else
					strcat(bstring, bpstring[piecetype(b->board[f][r])]);
			}
		}
		strcat(bstring, "|\n");
	}
	strcat(bstring, (b->onMove == WHITE) ? "W " : "B ");
	if (gg->numHalfMoves)
		sprintf(tmp, "%d ", ml[gg->numHalfMoves - 1].doublePawn);
	else
		strcpy(tmp, "-1 ");
	strcat(bstring, tmp);
	sprintf(tmp, "%d %d %d %d %d\n",
			!(b->wkmoved || b->wkrmoved), !(b->wkmoved || b->wqrmoved),
			!(b->bkmoved || b->bkrmoved), !(b->bkmoved || b->bqrmoved),
			(gg->numHalfMoves - (b->lastIrreversible == -1 ? 0 : b->lastIrreversible)));
	strcat(bstring, tmp);
	sprintf(tmp, "%d %s %s %d %d %d %d %d %d %d %d %s (%s) %s %d\n",
			b->gameNum,
			gg->white_name,
			gg->black_name,
			myTurn,
			gg->wInitTime / 600,
			gg->wIncrement / 10,
			ws,
			bs,
			(wTime + 5) / 10,
			(bTime + 5) / 10,
			gg->numHalfMoves / 2 + 1,
			gg->numHalfMoves ? ml[gg->numHalfMoves - 1].moveString : "none",
			gg->numHalfMoves ? tenth_str(ml[gg->numHalfMoves - 1].tookTime, 0) : "0:00",
			gg->numHalfMoves ? ml[gg->numHalfMoves - 1].algString : "none",
			orient != WHITE);
	strcat(bstring, tmp);
	sprintf(tmp, ">10<\n");
	strcat(bstring, tmp);
	return 0;
}

/* Same as 8, but with verbose moves ("P/e3-e4", instead of "e4") */
static int style11(struct game_state_t *b, struct move_t *ml)
{
	char tmp[800];
	int f, r, ws, bs;
	struct game *gg = &game_globals.garray[b->gameNum];
	
	board_calc_strength(b, &ws, &bs);
	sprintf(tmp, "#@#%03d%-16s%s%-16s%s", b->gameNum,
			gg->white_name, orient == WHITE ? "*" : ":",
			gg->black_name, orient == WHITE ? ":" : "*");
	strcat(bstring, tmp);
	for (r = 0; r < 8; r++) {
		for (f = 0; f < 8; f++) {
			if (b->board[f][r] == NOPIECE) {
				strcat(bstring, " ");
			} else {
				if (colorval(b->board[f][r]) == WHITE)
					strcat(bstring, wpstring[piecetype(b->board[f][r])]);
				else
					strcat(bstring, bpstring[piecetype(b->board[f][r])]);
			}
		}
	}
	sprintf(tmp, "%03d%s%02d%02d%05d%05d%-7s(%s)@#@\n",
		    gg->numHalfMoves / 2 + 1,
		    (b->onMove == WHITE) ? "W" : "B",
		    ws,
		    bs,
		    (wTime + 5) / 10,
		    (bTime + 5) / 10,
		    gg->numHalfMoves ? ml[gg->numHalfMoves - 1].moveString : "none",
		    gg->numHalfMoves ? tenth_str(ml[gg->numHalfMoves - 1].tookTime, 0) : "0:00");
	strcat(bstring, tmp);
	return 0;
}

/* Similar to style 10.  See the "style12" help file for information */
static int style12(struct game_state_t *b, struct move_t *ml)
{
	int f, r, ws, bs;
	char tmp[800];
	struct game *gg = &game_globals.garray[b->gameNum];
	
	board_calc_strength(b, &ws, &bs);
	sprintf(bstring, "<12> ");
	for (r = 7; r >= 0; r--) {
		for (f = 0; f < 8; f++) {
			if (b->board[f][r] == NOPIECE) {
				strcat(bstring, "-");
			} else {
				if (colorval(b->board[f][r]) == WHITE)
					strcat(bstring, wpstring[piecetype(b->board[f][r])]);
				else
					strcat(bstring, bpstring[piecetype(b->board[f][r])]);
			}
		}
		strcat(bstring, " ");
	}
	strcat(bstring, b->onMove == WHITE ? "W " : "B ");
	if (gg->numHalfMoves)
		sprintf(tmp, "%d ", ml[gg->numHalfMoves - 1].doublePawn);
	else
		strcpy(tmp, "-1 ");
	strcat(bstring, tmp);
	sprintf(tmp, "%d %d %d %d %d ",
		  !(b->wkmoved || b->wkrmoved), !(b->wkmoved || b->wqrmoved),
		  !(b->bkmoved || b->bkrmoved), !(b->bkmoved || b->bqrmoved),
		  (gg->numHalfMoves - ((b->lastIrreversible == -1) ? 0 : b->lastIrreversible)));
	strcat(bstring, tmp);
	sprintf(tmp, "%d %s %s %d %d %d %d %d %d %d %d %s (%s) %s %d %d 0\n",
		  b->gameNum + 1,
		  gg->white_name,
		  gg->black_name,
		  myTurn,
		  gg->wInitTime / 600,
		  gg->wIncrement / 10,
		  ws,
		  bs,
		  (wTime / 10),
		  (bTime / 10),
		  gg->numHalfMoves / 2 + 1,
		  gg->numHalfMoves ? ml[gg->numHalfMoves - 1].moveString : "none",
		  gg->numHalfMoves ? tenth_str(ml[gg->numHalfMoves - 1].tookTime, 0) : "0:00",
		  gg->numHalfMoves ? ml[gg->numHalfMoves - 1].algString : "none",
		  (orient == WHITE) ? 0 : 1,
		  b->moveNum > 1 ? 1 : 0); /* ticking */
	strcat(bstring, tmp);
	return 0;
}

static int board_read_file(char *category, char *gname, struct game_state_t *gs)
{
	FILE *fp;
	int c;
	int onNewLine = 1;
	int onColor = -1;
	int onPiece = -1;
	int onFile = -1;
	int onRank = -1;
	
	fp = fopen_p("%s/%s/%s", "r", BOARD_DIR, category, gname);
	if (!fp)
		return 1;
	
	board_clear(gs);
	while (!feof(fp)) {
		c = fgetc(fp);
		if (onNewLine) {
			if (c == 'W') {
				onColor = WHITE;
				if (gs->onMove < 0)
					gs->onMove = WHITE;
			} else if (c == 'B') {
				onColor = BLACK;
				if (gs->onMove < 0)
					gs->onMove = BLACK;
			} else /* if (c == '#') */ {  /* Skip any line we don't understand */
				while (!feof(fp) && c != '\n')
					c = fgetc(fp);
				continue;
			}
			onNewLine = 0;
		} else {
			switch (c) {
			case 'P':
				onPiece = PAWN;
				break;
			case 'R':
				onPiece = ROOK;
				break;
			case 'N':
				onPiece = KNIGHT;
				break;
			case 'B':
				onPiece = BISHOP;
				break;
			case 'Q':
				onPiece = QUEEN;
				break;
			case 'K':
				onPiece = KING;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'g':
			case 'h':
				onFile = c - 'a';
				onRank = -1;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
				onRank = c - '1';
				if (onFile >= 0 && onColor >= 0 && onPiece >= 0)
					gs->board[onFile][onRank] = onPiece | onColor;
				break;
			case '#':
				while (!feof(fp) && c != '\n')
					c = fgetc(fp);	/* Comment line */
			case '\n':
				onNewLine = 1;
				onColor = onPiece = onFile = onRank = -1;
				break;
			default:
				break;
			}
		}
	}
	fclose(fp);
	return 0;
}

#define WHITE_SQUARE 1
#define BLACK_SQUARE 0
#define ANY_SQUARE -1
#define SquareColor(f, r) ((f ^ r) & 1)

static void place_piece(board_t b, int piece, int squareColor)
{
	int r, f;
	
	r = iscolor(piece, BLACK) ? 7 : 0;
	
	for(;;) {
		if (squareColor == ANY_SQUARE) {
			f = random() % 8;
		} else {
			f = (random() % 4) * 2;
			if (SquareColor(f, r) != squareColor)
				f++;
		}
		if ((b)[f][r] == NOPIECE) {
			(b)[f][r] = piece;
			break;
		}
	}
}

static void wild_update(int style)
{
	int f, r, i, onPiece;
	board_t b;
	FILE *fp;

	for (f = 0; f < 8; f++)
		for (r = 0; r < 8; r++)
			b[f][r] = NOPIECE;
	for (f = 0; f < 8; f++) {
		b[f][1] = W_PAWN;
		b[f][6] = B_PAWN;
	}
	switch (style) {
	case 1:
		if (random() & 1) {
			b[4][0] = W_KING;
			b[3][0] = W_QUEEN;
		} else {
			b[3][0] = W_KING;
			b[4][0] = W_QUEEN;
		}
		if (random() & 1) {
			b[4][7] = B_KING;
			b[3][7] = B_QUEEN;
		} else {
			b[3][7] = B_KING;
			b[4][7] = B_QUEEN;
		}
		b[0][0] = b[7][0] = W_ROOK;
		b[0][7] = b[7][7] = B_ROOK;
		/* Must do bishops before knights to be sure
		   opposite colored squares are available. */
		place_piece(b, W_BISHOP, WHITE_SQUARE);
		place_piece(b, W_BISHOP, BLACK_SQUARE);
		place_piece(b, W_KNIGHT, ANY_SQUARE);
		place_piece(b, W_KNIGHT, ANY_SQUARE);
		place_piece(b, B_BISHOP, WHITE_SQUARE);
		place_piece(b, B_BISHOP, BLACK_SQUARE);
		place_piece(b, B_KNIGHT, ANY_SQUARE);
		place_piece(b, B_KNIGHT, ANY_SQUARE);
		break;
	case 2:
		place_piece(b, W_KING, ANY_SQUARE);
		place_piece(b, W_QUEEN, ANY_SQUARE);
		place_piece(b, W_ROOK, ANY_SQUARE);
		place_piece(b, W_ROOK, ANY_SQUARE);
		place_piece(b, W_BISHOP, ANY_SQUARE);
		place_piece(b, W_BISHOP, ANY_SQUARE);
		place_piece(b, W_KNIGHT, ANY_SQUARE);
		place_piece(b, W_KNIGHT, ANY_SQUARE);
		/* Black mirrors White */
		for (i = 0; i < 8; i++)
			b[i][7] = b[i][0] | BLACK;
		break;
	case 3:
		/* Generate White king on random square plus random set of pieces */
		place_piece(b, W_KING, ANY_SQUARE);
		for (i = 0; i < 8; i++)
			if (b[i][0] != W_KING)
				b[i][0] = (random() % 4) + 2;

		/* Black mirrors White */
		for (i = 0; i < 8; i++)
			b[i][7] = b[i][0] | BLACK;
		break;
	case 4:
		/* Generate White king on random square plus random set of pieces */
		place_piece(b, W_KING, ANY_SQUARE);
		for (i = 0; i < 8; i++)
			if (b[i][0] != W_KING)
				b[i][0] = (random() % 4) + 2;

		/* Black has same set of pieces, but randomly permuted, except that Black
		   must have the same number of bishops on white squares as White has on
		   black squares, and vice versa.  So we must place Black's bishops first
		   to be sure there are enough squares left of the correct color. */

		for (i = 0; i < 8; i++)
			if (b[i][0] == W_BISHOP)
				place_piece(b, B_BISHOP, !SquareColor(i, 0));

		for (i = 0; i < 8; i++)
			if (b[i][0] != W_BISHOP)
				place_piece(b, b[i][0] | BLACK, ANY_SQUARE);
		break;

	default:
		return;
	}

	fp = fopen_p("%s/wild/%d", "w", BOARD_DIR, style);
	if (!fp) {
		d_printf( _("CHESSD: Can't write wild style %d\n"), style);
		return;
	}
	fprintf(fp, "W:");
	onPiece = -1;
	for (r = 1; r >= 0; r--) {
		for (f = 0; f < 8; f++) {
			if (onPiece < 0 || b[f][r] != onPiece) {
				onPiece = b[f][r];
				fprintf(fp, " %s", wpstring[piecetype(b[f][r])]);
			}
			fprintf(fp, " %c%c", f + 'a', r + '1');
		}
	}
	fprintf(fp, "\nB:");
	onPiece = -1;
	for (r = 6; r < 8; r++) {
		for (f = 0; f < 8; f++) {
			if (onPiece < 0 || b[f][r] != onPiece) {
				onPiece = b[f][r];
				fprintf(fp, " %s", wpstring[piecetype(b[f][r])]);
			}
			fprintf(fp, " %c%c", f + 'a', r + '1');
		}
	}
	fprintf(fp, "\n");
	fclose(fp);
}

void wild_init(void)
{
	wild_update(1);
	wild_update(2);
	wild_update(3);
	wild_update(4);
}
