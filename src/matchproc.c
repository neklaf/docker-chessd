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

/**
  matchproc.c
  Feb 9 1996 - rewritten - DAV

*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "chessd_locale.h"
#include "configuration.h"
#include "follow.h"
#include "formula.h"
#include "gamedb.h"
#include "gameproc.h"
#include "globals.h"
#include "lists.h"
#include "malloc.h"
#include "obsproc.h"
#include "pending.h"
#include "playerdb.h"
#include "ratings.h"
#include "seekproc.h"
#include "utils.h"
#include "matchproc.h"

#include "protocol.h"

const char *colorstr[] = {"", N_("[black] "), N_("[white] ")};
static const char *adjustr[] = {"", N_(" (adjourned)")};

static void challenge_notification(
					int p1, int p2, char *category, char *board,
					int rated, int type,
					const char* color,
					int wt, int wi, int bt, int bi, int adjourned,
					int update);

static void prepare_match(int g, int wt, int bt, int winc, int binc,
						  int wp, int bp, int rated)
{
	struct game *gg = &game_globals.garray[g];
  wt = wt * 60;                 /* To Seconds */
  bt = bt * 60;
  gg->white = wp;
  gg->black = bp;

  strcpy(gg->white_name, player_globals.parray[wp].name);
  strcpy(gg->black_name, player_globals.parray[bp].name);
  gg->status = GAME_ACTIVE;
  if (gg->type == TYPE_UNTIMED || gg->type == TYPE_NONSTANDARD)
    gg->rated = 0;
  else
    gg->rated = rated;
  gg->private = BoolCheckPFlag(wp, PFLAG_PRIVATE)
  			 || BoolCheckPFlag(bp, PFLAG_PRIVATE);
  gg->white = wp;
  switch(gg->type){
  case TYPE_BLITZ:
    gg->white_rating = player_globals.parray[wp].b_stats.rating;
    gg->black_rating = player_globals.parray[bp].b_stats.rating;
    break;
  case TYPE_WILD:
    gg->white_rating = player_globals.parray[wp].w_stats.rating;
    gg->black_rating = player_globals.parray[bp].w_stats.rating;
    break;
  case TYPE_LIGHT:
    gg->white_rating = player_globals.parray[wp].l_stats.rating;
    gg->black_rating = player_globals.parray[bp].l_stats.rating;
    break;
  case TYPE_BUGHOUSE:
    gg->white_rating = player_globals.parray[wp].bug_stats.rating;
    gg->black_rating = player_globals.parray[bp].bug_stats.rating;
    break;
  default:
    gg->white_rating = player_globals.parray[wp].s_stats.rating;
    gg->black_rating = player_globals.parray[bp].s_stats.rating;
  }

  gg->game_state.gameNum = g;

  gg->wTime = gg->wInitTime = wt * 10;
  gg->wIncrement = winc * 10;
  gg->bTime = bt * 10;

  if (gg->type != TYPE_UNTIMED) {
    if (wt == 0)
        gg->wTime = 100;
    if (bt == 0)
        gg->bTime = 100;
  } /* 0 x games start with 10 seconds */

  gg->wRealTime = gg->wTime * 100;
  gg->bRealTime = gg->bTime * 100;
  gg->wTimeWhenReceivedMove = gg->bTimeWhenReceivedMove = 0;

  gg->bInitTime = bt * 10;
  gg->bIncrement = binc * 10;

  if (gg->game_state.onMove == BLACK) {   /* Start with black */
    gg->numHalfMoves = 1;
    gg->moveListSize = 1;
    gg->moveList = malloc(sizeof(struct move_t));
    gg->moveList[0].fromFile = -1;
    gg->moveList[0].fromRank = -1;
    gg->moveList[0].toFile = -1;
    gg->moveList[0].toRank = -1;
    gg->moveList[0].color = WHITE;
    strcpy(gg->moveList[0].moveString, "NONE");
    strcpy(gg->moveList[0].algString, "NONE");
  } else {
    gg->numHalfMoves = 0;
    gg->moveListSize = 0;
    gg->moveList = NULL;
  }

  gg->timeOfStart = tenth_secs();
  gg->startTime = tenth_secs();
  gg->lastMoveTime = gg->startTime;
  gg->lastDecTime = gg->startTime;
  gg->clockStopped = 0;

}

static void pick_colors(int* wp, int* bp, int white, int wt, int bt, 
						int winc, int binc)
{
 int reverse = 0;

  if (white == 0)
    reverse = 1; /* did challenger ask for black? */
  else if (white == -1) { /* unknown */
    if (wt == bt && winc == binc) { /* if diff times challenger is white */
      if (CheckPFlag(*wp, PFLAG_LASTBLACK) == CheckPFlag(*bp, PFLAG_LASTBLACK)) {
        if ((player_globals.parray[*wp].num_white - player_globals.parray[*wp].num_black) >
          (player_globals.parray[*bp].num_white - player_globals.parray[*bp].num_black))
          reverse = 1; /* whose played the most extra whites gets black */
      } else if (!CheckPFlag(*wp, PFLAG_LASTBLACK))
        reverse = 1;
    } else
      reverse = 1;
  }
  if (reverse) {
    int tmp = *wp;
    *wp = *bp;
    *bp = tmp;
  }
}

static void output_match_messages(int wp,int bp,int g, char* mess)
{
	struct game *gg = &game_globals.garray[g];
	char outStr[1024], notavail_white[200], notavail_black[200];
	int avail_printed = 0, printed, p, wt, winc, bt, binc;
	int wrating, brating, w_timeseal, b_timeseal, rated;

	notavail_white[0] = '\0';
	notavail_black[0] = '\0';

	sprintf(outStr, U_("\n{Game %d (%s vs. %s) %s %s %s match.}\n"), /* xboard */
			g + 1, player_globals.parray[wp].name, player_globals.parray[bp].name,
			mess, rstr[gg->rated], bstr[gg->type]);
	pprintf(wp, "%s", outStr);
	pprintf(bp, "%s", outStr);

	wt   = gg->wInitTime;
	bt   = gg->bInitTime;
	winc = gg->wIncrement;
	binc = gg->bIncrement;
	wrating = gg->white_rating;
	brating = gg->black_rating;
	w_timeseal = net_globals.con[player_globals.parray[wp].socket]->timeseal != 0;
	b_timeseal = net_globals.con[player_globals.parray[bp].socket]->timeseal != 0;
	rated = gg->rated;

	// send the game information using the new protocol
	protocol_game_info(wp, g+1, bstr[gg->type], rated,
				CheckPFlag(wp, PFLAG_REG), CheckPFlag(bp, PFLAG_REG),
				wt * 60, winc, bt * 60, binc, /*bh*/0,
				wrating, brating, w_timeseal, b_timeseal );

	protocol_game_info(bp, g+1, bstr[gg->type], rated,
				CheckPFlag(wp, PFLAG_REG), CheckPFlag(bp, PFLAG_REG),
				wt * 60, winc, bt * 60, binc, /*bh*/0,
				wrating, brating, w_timeseal, b_timeseal );

	if (!player_num_active_boards(wp) && CheckPFlag(wp, PFLAG_OPEN))
		/* open may be 0 if a simul */
		getnotavailmess(wp,notavail_white);

	if (CheckPFlag(bp, PFLAG_OPEN))
		getnotavailmess(bp,notavail_black);

	for (p = 0; p < player_globals.p_num; p++) {
		struct player *pp = &player_globals.parray[p];
		int gnw, gnb;
		printed = 0;

		if (p == wp || p == bp || pp->status != PLAYER_PROMPT)
			continue;
		if (CheckPFlag(p, PFLAG_GIN)) {
			pprintf(p, "%s", outStr);
			printed = 1;
		}
		gnw = in_list(p, L_GNOTIFY, player_globals.parray[wp].login);
		gnb = in_list(p, L_GNOTIFY, player_globals.parray[bp].login);
		if (gnw || gnb) {
			pprintf(p, _("\nGame notification: "));

			if (gnw)
				pprintf_highlight(p, player_globals.parray[wp].name);
			else
				pprintf(p, player_globals.parray[wp].name);

			pprintf(p, " (%s) vs. ",
					ratstr(GetRating(&player_globals.parray[wp], gg->type)));

			if (gnb)
				pprintf_highlight(p, player_globals.parray[bp].name);
			else
				pprintf(p, player_globals.parray[bp].name);
			pprintf(p, _(" (%s) %s %s %d %d: Game %d\n"),
					ratstr(GetRating(&player_globals.parray[bp], gg->type)),
					rstr[gg->rated], bstr[gg->type],
					gg->wInitTime/600, gg->wIncrement/10, g+1);
			printed = 1;
		}

		if (CheckPFlag(p, PFLAG_AVAIL) && CheckPFlag(p, PFLAG_OPEN) && (pp->game < 0) &&
				(CheckPFlag(wp, PFLAG_OPEN) || CheckPFlag(bp, PFLAG_OPEN))) {

			if (((player_globals.parray[wp].b_stats.rating <= pp->availmax) &&
						(player_globals.parray[wp].b_stats.rating >= pp->availmin)) ||
					(!pp->availmax)) {
				pprintf (p,"\n%s",notavail_white);
				avail_printed = 1;
			}

			if (((player_globals.parray[bp].b_stats.rating <= pp->availmax) &&
						(player_globals.parray[bp].b_stats.rating >= pp->availmin)) ||
					(!pp->availmax)) {
				pprintf (p,"\n%s",notavail_black);
				avail_printed = 1;
			}

			if (avail_printed) {
				printed = 1;
				avail_printed = 0;
				pprintf (p,"\n");
			} /* was black or white open originally (for simuls) */
		}

		if (printed)
			send_prompt (p);
	}
}

int create_new_match(int g, int white_player, int black_player,
                     int wt, int winc, int bt, int binc,
                     int rated, char *category, char *board,
                     int white, int simul)
{
	remove_request(white_player, black_player, -PEND_PARTNER);

	pick_colors(&white_player,&black_player,white,wt,bt,winc,binc);

	decline_withdraw_offers(white_player,-1, PEND_MATCH,DO_WITHDRAW | DO_DECLINE);
	decline_withdraw_offers(black_player,-1, PEND_MATCH,DO_WITHDRAW | DO_DECLINE);
	decline_withdraw_offers(black_player,-1, PEND_SIMUL,DO_WITHDRAW | DO_DECLINE);
	if (simul)
		decline_withdraw_offers(white_player, -1, PEND_SIMUL, DO_WITHDRAW);
	else
		decline_withdraw_offers(white_player, -1, PEND_SIMUL,
								DO_WITHDRAW | DO_DECLINE);

	if (board_init(g,&game_globals.garray[g].game_state, category, board)) {
		pprintf(white_player, _("PROBLEM LOADING BOARD. Game Aborted.\n"));
		pprintf(black_player, _("PROBLEM LOADING BOARD. Game Aborted.\n"));
		d_printf( _("CHESSD: PROBLEM LOADING BOARD %s %s. Game Aborted.\n"),
				category, board);
		game_remove(g);
		return COM_FAILED;
	}

	if (simul)
		game_globals.garray[g].type = TYPE_UNTIMED;
	else
		game_globals.garray[g].type =
			game_isblitz(wt, winc, bt, binc, category, board);

	prepare_match(g,wt,bt,winc,binc,white_player,black_player,rated);

	/*****************************/

	player_globals.parray[white_player].game = g;
	player_globals.parray[white_player].opponent = black_player;
	player_globals.parray[white_player].side = WHITE;
	player_globals.parray[white_player].promote = QUEEN;
	player_globals.parray[black_player].game = g;
	player_globals.parray[black_player].opponent = white_player;
	player_globals.parray[black_player].side = BLACK;
	player_globals.parray[black_player].promote = QUEEN;
	gics_globals.userstat.games++;

	output_match_messages(white_player,black_player, g, _("Creating"));
	send_boards(g);

	/* obey any 'follow' lists */
	follow_start(white_player,black_player);

	if(game_globals.garray[g].status == GAME_ACTIVE)
	{
		InsertServerEvent( seGAME_COUNT_CHANGE, game_count() );

		InsertActiveGame(game_globals.garray[g].type,
				player_globals.parray[white_player].chessduserid,
				player_globals.parray[black_player].chessduserid,
				player_globals.parray[white_player].login,
				player_globals.parray[black_player].login);
	}

	return COM_OK;
}

int accept_match(struct pending *pend, int p, int p1)
{
	struct player *pp = &player_globals.parray[p];
	int wt, winc, bt, binc, rated, white;
	char category[50], board[50];
	char tmp[100];
	int bh = 0, partner = 0, pp1 = 0, g1, g2;

	unobserveAll(p);              /* stop observing when match starts */
	unobserveAll(p1);

	wt = pend->wtime;
	winc = pend->winc;
	bt = pend->btime;
	binc = pend->binc;
	rated = pend->rated;
	strcpy (category, pend->category);
	strcpy (board, pend->board_type);
	white = (pend->seek_color == -1) ? -1 : 1 - pend->seek_color;

	pprintf(p, _("You accept the challenge of %s.\n"),
			player_globals.parray[p1].name);
	pprintf_prompt(p1, _("\n%s accepts your challenge.\n"), pp->name);

	if(!pend->status)
		delete_pending(pend);

	//  withdraw_seeks(p);
	//  withdraw_seeks(p1);
	seekremoveall(p);
	seekremoveall(p1);

	pend_join_match (p,p1);

	if (game_isblitz(wt, winc, bt, binc, category, board) == TYPE_BUGHOUSE) {
		bh = 1;

		if ((partner = pp->partner) >= 0 &&
				(pp1 = player_globals.parray[p1].partner) >= 0) {
			unobserveAll(partner);         /* stop observing when match starts */
			unobserveAll(pp1);

			pprintf(partner, _("\nYour partner accepts the challenge of %s.\n"),
					player_globals.parray[p1].name);
			pprintf(pp1, _("\n%s accepts your partner's challenge.\n"), pp->name);

			pend_join_match (partner,pp1);
		} else {
			return COM_OK;
		}
	}

	// this game should be inserted in the DB
	g1 = game_new(); /* create a game structure */

	if (g1 < 0) {
		sprintf(tmp, _("There was a problem creating the new match.\n"));
		pprintf(p, tmp);
		pprintf_prompt(p1, tmp);
	}

	if (game_read(g1, p1, p) >= 0) {
		int swap;
		swap = p;
		p = p1;
		p1 = swap;
		pp = &player_globals.parray[p];

	} else
	if (game_read(g1, p, p1) < 0) {
		/* so no adjourned game */

		if (create_new_match(g1,p, p1, wt, winc, bt, binc, rated,
					         category, board, white,0) == COM_FAILED)
			return COM_OK;

		/* create first game */

		if (bh) { /* do bughouse creation */

			white = (pp->side == WHITE ? 0 : 1);
			g2 = game_new();
			if ((g2 < 0) ||
				(create_new_match(g2,partner, pp1, wt, winc, bt,
				  binc, rated, category, board,white,0) == COM_FAILED)) {
				sprintf(tmp,
					_("There was a problem creating the new match.\n"));
				pprintf_prompt(partner, tmp);
				pprintf_prompt(pp1, tmp);
				sprintf(tmp,
					_("There was a problem creating your partner's match.\n"));
				game_remove(g1); /* abort first game */
				return COM_OK;
			}

			game_globals.garray[g1].link = g2; /* link the games */
			game_globals.garray[g2].link = g1;

			sprintf(tmp, _("\nYour partner is playing game %d (%s vs. %s).\n"),
					g2 + 1, game_globals.garray[g2].white_name,
					game_globals.garray[g2].black_name);
			pprintf(p, tmp);
			pprintf_prompt(p1, tmp);

			sprintf(tmp, _("\nYour partner is playing game %d (%s vs. %s).\n"),
					g1 + 1, game_globals.garray[g1].white_name,
					game_globals.garray[g1].black_name);
			pprintf_prompt(partner, tmp);
			pprintf_prompt(pp1, tmp);
		}
		return COM_OK;
	}

	/* resume adjourned game */
	game_delete(p, p1); /* remove the game from disk */

	game_globals.garray[g1].white = p;
	game_globals.garray[g1].black = p1;
	game_globals.garray[g1].status = GAME_ACTIVE;
	game_globals.garray[g1].result = END_NOTENDED;
	game_globals.garray[g1].startTime = tenth_secs();
	game_globals.garray[g1].lastMoveTime = game_globals.garray[g1].startTime;
	game_globals.garray[g1].lastDecTime = game_globals.garray[g1].startTime;
	pp->game = g1;
	pp->opponent = p1;
	pp->side = WHITE;
	player_globals.parray[p1].game = g1;
	player_globals.parray[p1].opponent = p;
	player_globals.parray[p1].side = BLACK;

	/* xboard */
	sprintf(tmp, U_("{Game %d (%s vs. %s) Continuing %s %s match.}\n"),
			g1+1, pp->name, player_globals.parray[p1].name,
			rstr[game_globals.garray[g1].rated],
			bstr[game_globals.garray[g1].type]);

	pprintf(p, tmp);
	pprintf(p1, tmp);
	output_match_messages(p, p1, g1, _("Continuing"));


	//	FEN_to_board(game_globals.garray[g1].FENstartPos ,  &(game_globals.garray[g1].game_state));

	send_boards(g1);

	/* obey any 'follow' lists */
	follow_start(p,p1);

	/* pedrorib: not tested yet 23/03/04 */
	InsertServerEvent( seGAME_COUNT_CHANGE, game_count() );

	InsertActiveGame(game_globals.garray[g1].type,
			player_globals.parray[p].chessduserid,
			player_globals.parray[p1].chessduserid,
			player_globals.parray[p].login,
			player_globals.parray[p1].login);

	return COM_OK;
}

/* board and category are initially empty strings */
static int parse_match_string(int p, int* wt,int* bt,int* winc,int* binc,
                                int* white,int* rated,char* category,
			        char* board, char* mstring)
{
  char parsebuf[100];
  int numba, confused = 0, bughouse = 0;

  while (!confused && (sscanf(mstring, " %99s", parsebuf) == 1)) {
    mstring = eatword(eatwhite(mstring));

    if ((category[0] != '\0') && (board[0] == '\0') && !bughouse)
      strcpy(board, parsebuf);
    else if (isdigit(*parsebuf)) {
      if ((numba = atoi(parsebuf)) < 0) {
        pprintf(p, _("You can't specify negative time controls.\n"));
        return 0;
      } else if (numba > 999) {
        pprintf(p, _("You can't specify time or inc above 999.\n"));
        return 0;
      } else if (*wt == -1)
        *wt = numba;
      else if (*winc == -1)
        *winc = numba;
      else if (*bt == -1)
        *bt = numba;
      else if (*binc == -1)
        *binc = numba;
      else
        confused = 1;

    } else if (!strcmp("rated", parsebuf) || !strcmp("r",parsebuf)) {
      if (*rated == -1)
        *rated = 1;
      else
        confused = 1;
    } else if (!strcmp("unrated", parsebuf) || !strcmp("u",parsebuf)) {
      if (*rated == -1)
        *rated = 0;
      else
        confused = 1;
    } else if (!strcmp("white", parsebuf) || !strcmp ("w", parsebuf)) {
      if (*white == -1)
        *white = 1;
      else
        confused = 1;
    } else if (!strcmp("black", parsebuf) || !strcmp ("b", parsebuf)) {
      if (*white == -1)
        *white = 0;
      else
        confused = 1;

    } else if (category[0] == '\0') {
      if ((parsebuf[0] == 'w') && (isdigit(parsebuf[1]))) {
        strcpy(category, "wild");
        strcpy(board, parsebuf+1);
      } else {
        strcpy(category,parsebuf);
        if (!strncmp("bughouse",category, strlen(category))
            && strlen(category) >= 3) {
          strcpy(category, "bughouse");
          bughouse = 1;
        }
      }
    } else
      confused = 1;
  }

  if (confused) {
    pprintf(p, _("Can't interpret %s in match command.\n"), parsebuf);
    return 0;
  }
  return 1;
}

static void print_match_rating_info(int p,int p1,int type,int rated)
{
  if (rated) {
    int win, draw, loss;
    double newsterr;

    rating_sterr_delta(p1, p, type, time(0), RESULT_WIN, &win, &newsterr);
    rating_sterr_delta(p1, p, type, time(0), RESULT_DRAW, &draw, &newsterr);
    rating_sterr_delta(p1, p, type, time(0), RESULT_LOSS, &loss, &newsterr);
    pprintf(p1, _("Your %s rating will change:  Win: %s%d,  Draw: %s%d,  Loss: %s%d\n"),
            bstr[type],
            win >= 0 ? "+" : "", win,
            draw >= 0 ? "+" : "", draw,
            loss >= 0 ? "+" : "", loss);
    pprintf(p1, _("Your new RD will be %5.1f\n"), newsterr);

    rating_sterr_delta(p, p1, type, time(0), RESULT_WIN, &win, &newsterr);
    rating_sterr_delta(p, p1, type, time(0), RESULT_DRAW, &draw, &newsterr);
    rating_sterr_delta(p, p1, type, time(0), RESULT_LOSS, &loss, &newsterr);
    pprintf(p, _("Your %s rating will change:  Win: %s%d,  Draw: %s%d,  Loss: %s%d\n"),
            bstr[type],
            win >= 0 ? "+" : "", win,
            draw >= 0 ? "+" : "", draw,
            loss >= 0 ? "+" : "", loss);
    pprintf(p, _("Your new RD will be %5.1f\n"), newsterr);
  }
}

static void check_lists_match(int p,int p1)
{
  struct player *pp = &player_globals.parray[p];
  if (in_list(p, L_COMPUTER, pp->name)) {
    pprintf(p1, "--** %s is a ", pp->name);
    pprintf_highlight(p1, _("computer"));
    pprintf(p1, " **--\n");
  }
  if (in_list(p, L_COMPUTER, player_globals.parray[p1].name)) {
    pprintf(p, "--** %s is a ", player_globals.parray[p1].name);
    pprintf_highlight(p, _("computer"));
    pprintf(p, " **--\n");
  }
  if (in_list(p, L_ABUSER, pp->name)) {
    pprintf(p1, _("--** %s is in the "), pp->name);
    pprintf_highlight(p1, _("abuser"));
    pprintf(p1, _(" list **--\n"));
  }
  if (in_list(p, L_ABUSER, player_globals.parray[p1].name)) {
    pprintf(p, _("--** %s is in the "), player_globals.parray[p1].name);
    pprintf_highlight(p, _("abuser"));
    pprintf(p, _(" list **--\n"));
  }
}

int com_match(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct player *pp_opp;
	int adjourned;                /* adjourned game? */
	int g;                        /* more adjourned game junk */
	int p1;
	int bh = 0, partner = 0, pp1 = 0;
	struct pending* pendfrom;
	struct pending* pendto;
	struct pending* pend;
	int wt = -1;                  /* white start time */
	int winc = -1;                /* white increment */
	int bt = -1;                  /* black start time */
	int binc = -1;                /* black increment */
	int rated = -1;               /* 1 = rated, 0 = unrated */
	int white = -1;               /* 1 = want white, 0 = want black */
	char category[100], board[100];
	textlist *clauses = NULL;
	int type = 0, p1ID, p2ID;

	// check here if the user is a 'name abuser'. name abusers can't play in
	// the server

	if (in_list(p, L_NAME_ABUSER, pp->login)) {
		pprintf_prompt(p, "You can't play here because your name violates the server rules.\n");
		return COM_OK;
	}

	category[0] ='\0';
	board[0] ='\0';

	if (pp->game >= 0 && (game_globals.garray[pp->game].status == GAME_EXAMINE
					   || game_globals.garray[pp->game].status == GAME_SETUP) )
	{
		pprintf(p, _("You can't challenge while you are examining a game.\n"));
		return COM_OK;
	}

	if (pp->game >= 0) {
		pprintf(p, _("You can't challenge while you are playing a game.\n"));
		return COM_OK;
	}

	stolower(param[0].val.word);
	p1 = player_find_part_login(param[0].val.word);
	if (p1 < 0) {
		pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
		return COM_OK;
	}

	// check whether the other player can play
	pp_opp = &player_globals.parray[p1];

	if (in_list(p, L_NAME_ABUSER, pp_opp->login)) {
		pprintf_prompt(p, _("You can't play against this player "
			"because his name violates this server's rules.\n"));
		return COM_OK;
	}

	// gabrielsan: obtaining users' IDs
	p1ID = pp->chessduserid;
	p2ID = player_globals.parray[p1].chessduserid;

	if (p1 == p) {  /* Allowing to match yourself to enter analysis mode */
		ExamineScratch (p, param, 0);
		return COM_OK;
	}

	if (!CheckPFlag(p1, PFLAG_OPEN)) {
		pprintf(p, _("Player \"%s\" is not open to match requests.\n"),
				player_globals.parray[p1].name);
		return COM_OK;
	}

	if (player_globals.parray[p1].game >= 0) {
		pprintf(p, _("Player \"%s\" is involved in another game.\n"),
				player_globals.parray[p1].name);
		return COM_OK;
	}

	if (CheckPFlag(p, PFLAG_TOURNEY) && !CheckPFlag(p1, PFLAG_TOURNEY)) {
		pprintf(p, _("You may only match players with their tournament variable set.\n"));
		return COM_OK;
	}

	if (!CheckPFlag(p, PFLAG_TOURNEY) && CheckPFlag(p1, PFLAG_TOURNEY)) {
		pprintf(p, _("%s is in a tournament, and cannot accept other challenges.\n"),
				player_globals.parray[p1].name);
		return COM_OK;
	}

	if (!CheckPFlag(p, PFLAG_OPEN)) {
		PFlagON(p, PFLAG_OPEN);
		pprintf(p, _("Setting you open for matches.\n"));
	}

	/* look for an adjourned game between p and p1 */
	g = game_new();

	// the game read should fill this game's ID, so we can 'update' it later
	adjourned = game_read(g, p, p1) >= 0 || game_read(g, p1, p) >= 0;

	if (adjourned) {
		type = game_globals.garray[g].type;
		wt = game_globals.garray[g].wInitTime / 600;
		bt = game_globals.garray[g].bInitTime / 600;
		winc = game_globals.garray[g].wIncrement / 10;
		binc = game_globals.garray[g].bIncrement / 10;
		rated = game_globals.garray[g].rated;
	}

	d_printf("Removing game started in com_match: removing previous game\n");
	game_remove(g);

	pendto = find_pend(p, p1, PEND_MATCH);
	pendfrom = find_pend(p1, p, PEND_MATCH);

	if (!adjourned) {
		if (in_list(p1, L_NOPLAY, pp->login)) {
			pprintf(p, _("You are on %s's noplay list.\n"),
					player_globals.parray[p1].name);
			return COM_OK;
		}
		if (player_censored(p1, p)) {
			pprintf(p, _("Player \"%s\" is censoring you.\n"),
					player_globals.parray[p1].name);
			return COM_OK;
		}
		if (player_censored(p, p1)) {
			pprintf(p, _("You are censoring \"%s\".\n"),
					player_globals.parray[p1].name);
			return COM_OK;
		}

		if (param[1].type != TYPE_NULL) {
			if (!parse_match_string(p, &wt,&bt,&winc,&binc,&white,&rated,
									category, board,param[1].val.string))
				return COM_OK; /* couldn't parse */
		}

		if (rated == -1)
			rated = BoolCheckPFlag(p, PFLAG_RATED);
		if (!CheckPFlag(p, PFLAG_REG) || !CheckPFlag(p1, PFLAG_REG))
			rated = 0;

		if (winc == -1)
			winc = wt == -1 ? pp->d_inc : 0;  /* match 5 == match 5 0 */

		if (wt == -1)
			wt = pp->d_time;

		if (bt == -1)
			bt = wt;

		if (binc == -1)
			binc = winc;

		if (category[0]) {
			if (!board[0] && strcmp(category,"bughouse")) {
				pprintf(p, _("You must specify a board and a category.\n"));
				return COM_OK;
			} else if (board[0]) { /* not bughouse */
				char fname[MAX_FILENAME_SIZE];
				sprintf(fname, "%s/%s/%s", BOARD_DIR, category, board);
				if (!file_exists(fname)) {
					pprintf(p, _("No such category/board: %s/%s\n"),
							category, board);
					return COM_OK;
				}
			}
		}
		type = game_isblitz(wt, winc, bt, binc, category, board);

		if (!strcmp(category, "bughouse")) {
			type = TYPE_BUGHOUSE;
			if (rated && pp->partner >= 0 && player_globals.parray[p1].partner >= 0){
				if (!CheckPFlag(pp->partner, PFLAG_REG) ||
					!CheckPFlag(player_globals.parray[p1].partner, PFLAG_REG))
					rated = 0;
			}
		}
		if (rated && (type == TYPE_NONSTANDARD)) {
			pprintf(p, _("Game is non-standard - reverting to unrated\n"));
			rated = 0;
		}
		if (rated && (type == TYPE_UNTIMED)) {
			pprintf(p, _("Game is untimed - reverting to unrated\n"));
			rated = 0;
		}
		if ( pendfrom == NULL && !CheckPFlag(p1, PFLAG_ROPEN)
				&& rated != BoolCheckPFlag(p1, PFLAG_RATED) )
		{
			pprintf(p, _("%s only wants to play %s games.\n"),
					player_globals.parray[p1].name, rstr[!rated]);
			pprintf_highlight(p1, _("Ignoring"));
			pprintf(p1, _(" %s match request from %s.\n"),
					rated ? _("rated") : _("unrated"), pp->name);
			return COM_OK;
		}

		/* Now check formula. */
		if (!adjourned
			&& !GameMatchesFormula(p,p1, wt,winc,bt,binc, rated, type, &clauses)) {
			pprintf(p, _("Match request does not fit formula for %s:\n"),
					player_globals.parray[p1].name);
			pprintf(p, _("%s's formula: %s\n"),
					player_globals.parray[p1].name,
					player_globals.parray[p1].formula);

			ShowClauses(p, p1, clauses);
			ClearTextList(clauses);
			pprintf_highlight(p1, _("Ignoring"));

			pprintf_prompt(p1, _(" (formula): %s (%d) %s (%d) %s.\n"),
					pp->name,
					GetRating(&player_globals.parray[p], type),
					player_globals.parray[p1].name,
					GetRating(&player_globals.parray[p1], type),
					game_str(rated, wt * 60, winc, bt * 60, binc, category,
					board));
			return COM_OK;
		}

		if (type == TYPE_BUGHOUSE) {
			bh = 1;
			partner = pp->partner;
			pp1 = player_globals.parray[p1].partner;
/*
			if (pp < 0) { // <-- BUG+FIXME: this comparison makes no sense
				pprintf(p, _("You have no partner for bughouse.\n"));
				return COM_OK;
			}
*/
			if (pp1 < 0) {
				pprintf(p, _("Your opponent has no partner for bughouse.\n"));
				return COM_OK;
			}
			if (partner == pp1) {
				/* should be an impossible case - DAV */
				pprintf(p, _("You and your opponent both chose the same partner!\n"));
				return COM_OK;
			}
			if (partner == p1 || pp1 == p) {
				pprintf(p, _("You and your opponent can't choose each other as partners!\n"));
				return COM_OK;
			}
			if (player_globals.parray[partner].partner != p) {
				/* another impossible case - DAV */
				pprintf(p, _("Your partner hasn't chosen you as his partner!\n"));
				return COM_OK;
			}
			if (player_globals.parray[pp1].partner != p1) {
				/* another impossible case - DAV */
				pprintf(p, _("Your opponent's partner hasn't chosen your opponent as his partner!\n"));
				return COM_OK;
			}
			if (!CheckPFlag(partner, PFLAG_OPEN) ||
				player_globals.parray[partner].game >= 0) {
				pprintf(p, _("Your partner isn't open to play right now.\n"));
				return COM_OK;
			}
			if (!CheckPFlag(pp1, PFLAG_OPEN) ||
				player_globals.parray[pp1].game >= 0) {
				pprintf(p, _("Your opponent's partner isn't open to play right now.\n"));
				return COM_OK;
			}

			/* Bypass NOPLAY lists, censored lists, ratedness, privacy, and
			 * formula for now */
			/*  Active challenger/ee will determine these. */
		}
		/* Ok match offer will be made */

	} /* adjourned games shouldn't have to worry about that junk? */

	/* keep in case of adjourned bughouse in future*/

	if (pendto != NULL) {
		pprintf(p, _("Updating offer already made to \"%s\".\n"),
				player_globals.parray[p1].name);
	}

	if (pendfrom != NULL) {
		if (pendto != NULL) {
			pprintf(p, _("Pending list error!\n"));
			d_printf( _("CHESSD: This shouldn't happen. You can't have a match pending from and to the same person.\n"));
			return COM_OK;
		}

		if ( adjourned ||
					(wt == pendfrom->btime && winc == pendfrom->binc &&
					bt == pendfrom->wtime && binc == pendfrom->winc &&
					rated == (signed int) pendfrom->rated &&
					(white == -1 || (white + pendfrom->seek_color == 1)) &&
					!strcmp(category, pendfrom->category) &&
					!strcmp(board, pendfrom->board_type)) )
		{	/* Identical match, should accept! */
			accept_match(pendfrom,p, p1);
			return COM_OK;
		} else
			delete_pending(pendfrom);
	}

	if (pendto == NULL)
		pend = add_pending(p,p1,PEND_MATCH);
	else
		pend = pendto;

	pend->wtime = wt;
	pend->winc = winc;
	pend->btime = bt;
	pend->binc = binc;
	pend->rated = rated;
	pend->seek_color = white;
	pend->game_type = type;
	pend->category = strdup(category);
	pend->board_type = strdup(board);

	if (pendfrom != NULL) {
		pprintf(p, _("Declining offer from %s and offering new match parameters.\n"),
				player_globals.parray[p1].name);
		pprintf(p1, _("\n%s declines your match offer a match with these parameters:"),
				pp->name);
	}

	/*
	 * notify challenger & challenged
	 */
	challenge_notification(p, p1, category, board, rated, type,
			colorstr[white + 1], wt, winc, bt, binc, adjourned, pendto != NULL);

	if (bh) {
		// notify the challenger's partner
		pprintf(partner, _("\nYour bughouse partner issuing %s (%s) %s"),
				pp->name, ratstrii(GetRating(&player_globals.parray[p], type), p),
				colorstr[white + 1]);
		pprintf_highlight(partner, "%s", player_globals.parray[p1].name);
		pprintf(partner, " (%s) %s.\n",
				ratstrii(GetRating(&player_globals.parray[p1], type), p1),
				game_str(rated, wt * 60, winc, bt * 60, binc, category, board));
		pprintf(partner, _("Your game would be "));
		pprintf_highlight(partner, "%s", player_globals.parray[pp1].name);
		pprintf_prompt(partner, " (%s) %s%s (%s) %s.\n",
				ratstrii(GetRating(&player_globals.parray[pp1], type), pp1),
				colorstr[white + 1], player_globals.parray[partner].name,
				ratstrii(GetRating(&player_globals.parray[partner], type), partner),
				game_str(rated, wt * 60, winc, bt * 60, binc, category, board));
		Bell (partner);

		// notify the challenged's partner
		pprintf(pp1, _("\nYour bughouse partner was challenged "));
		pprintf_highlight(pp1, "%s", pp->name);
		pprintf(pp1, " (%s) %s", ratstrii(GetRating(&player_globals.parray[p], type), p),
				colorstr[white + 1]);
		pprintf(pp1, "%s (%s) %s.\n", player_globals.parray[p1].name,
				ratstrii(GetRating(&player_globals.parray[p1], type), p1),
				game_str(rated, wt * 60, winc, bt * 60, binc, category, board));
		pprintf(pp1, _("Your game would be %s (%s) %s"), player_globals.parray[pp1].name,
				ratstrii(GetRating(&player_globals.parray[pp1], type), pp1),
				colorstr[white + 1]);
		pprintf_highlight(pp1, "%s", player_globals.parray[partner].name);
		pprintf_prompt(pp1, " (%s) %s.\n",
				ratstrii(GetRating(&player_globals.parray[partner], type), partner),
				game_str(rated, wt * 60, winc, bt * 60, binc, category, board));
		Bell(pp1);
	}

	check_lists_match (p,p1);

	print_match_rating_info (p,p1,type,rated);

	pprintf_prompt(p1,
		_("You can \"accept\" or \"decline\", or propose different parameters.\n"));

	return COM_OK;
}


/*
  rmatch is used by mamer to start matches in tournaments
*/
int com_rmatch(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int p1, p2;

	if (!in_list(p, L_TD, pp->name)) {
		pprintf(p, _("Only TD programs are allowed to use this command.\n"));
		return COM_OK;
	}

	if ((p1 = player_find_bylogin(param[0].val.word)) < 0 ||
	    !CheckPFlag(p1, PFLAG_REG))
	{	/* can't rmatch this user ... */
		return COM_OK;
	}

	if ((p2 = player_find_bylogin(param[1].val.word)) < 0 ||
	    !CheckPFlag(p2, PFLAG_REG))
	{	/* can't rmatch this user ... */
		return COM_OK;
	}

	return pcommand(p1, "match %s %s", param[1].val.word, param[2].val.string);
}

/*
 * gets the delta changes in rating for players p and p1
 */
static void get_match_rating_info(int p, int p1, int type,
		struct delta_rating_info *p1_info, struct delta_rating_info *p2_info)
{
	int win, draw, loss;
	double newsterr;

	rating_sterr_delta(p1, p, type, time(0), RESULT_WIN, &win, &newsterr);
	rating_sterr_delta(p1, p, type, time(0), RESULT_DRAW, &draw, &newsterr);
	rating_sterr_delta(p1, p, type, time(0), RESULT_LOSS, &loss, &newsterr);
	p1_info->win = win;
	p1_info->los = loss;
	p1_info->dra = draw;
	p1_info->sterr = newsterr;

	rating_sterr_delta(p, p1, type, time(0), RESULT_WIN, &win, &newsterr);
	rating_sterr_delta(p, p1, type, time(0), RESULT_DRAW, &draw, &newsterr);
	rating_sterr_delta(p, p1, type, time(0), RESULT_LOSS, &loss, &newsterr);
	p2_info->win = win;
	p2_info->los = loss;
	p2_info->dra = draw;
	p2_info->sterr = newsterr;
}

static void challenge_notification(
					int p1, int p2, char *category, char *board,
					int rated, int type,
					const char* color,
					int wt, int wi, int bt, int bi, int adjourned,
					int update)
{
	struct player *pp1 = &player_globals.parray[p1];
	struct player *pp2 = &player_globals.parray[p2];
	struct delta_rating_info p1_info, p2_info;

	// get the delta for win/loss/draw
	get_match_rating_info(p1, p2, type, &p1_info, &p2_info);

	protocol_challenge_notify(
			p1,
			1, // p1 *is* the challenger
			update,
			pp1->name,
			ratstrii(GetRating(pp1, type), p1),
			pp2->name,
			ratstrii(GetRating(pp2, type), p2),
			color,
			category,
			board,
			rated,
			wt,					// white time
			wi,					// white inc
			bt,					// black time
			bi,					// black inc
			adjustr[adjourned], // is the game adjourned?
			p2_info				// information on p1's rating change
			);

	protocol_challenge_notify(
			p2,
			0, // p2 is *not* the challenger
			update,
			pp1->name,
			ratstrii(GetRating(pp1, type), p1),
			pp2->name,
			ratstrii(GetRating(pp2, type), p2),
			color,
			category,
			board,
			rated,
			wt,					// white time
			wi,					// white inc
			bt,					// black time
			bi,					// black inc
			adjustr[adjourned],	// is the game adjourned?
			p1_info				// information on p2's rating change
			);
}

