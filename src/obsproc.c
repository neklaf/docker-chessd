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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "configuration.h"
#include "adminproc.h"
#include "playerdb.h"
#include "gamedb.h"
#include "utils.h"
#include "chessd_locale.h"
#include "malloc.h"
#include "globals.h"
#include "gameproc.h"
#include "pending.h"
//#include "chessd_main.h"
#include "comproc.h"
#include "obsproc.h"
#include "lists.h"
#include "movecheck.h"
#include "common.h"

int GameNumFromParam(int p, int *p1, parameter *param)
{
	if (param->type == TYPE_WORD) {
		*p1 = player_find_part_login(param->val.word);
		if (*p1 < 0) {
			pprintf(p, _("No user named \"%s\" is logged in.\n"), param->val.word);
			return -1;
		}
		if (player_globals.parray[*p1].game < 0)
			pprintf(p, _("%s is not playing a game.\n"), 
					player_globals.parray[*p1].name);
		return player_globals.parray[*p1].game;
	} else {			/* Must be an integer */
		*p1 = -1;
		if (param->val.integer <= 0)
			pprintf(p, _("%d is not a valid game number.\n"), param->val.integer);
		return param->val.integer - 1;
	}
}

static int gamesortfunc(const void *i, const void *j)
{
	struct game *gi = &game_globals.garray[*(int*)i],
				*gj = &game_globals.garray[*(int*)j];

	/* examine mode games moved to top of "games" output */
	if((gi->status == GAME_EXAMINE || gi->status == GAME_SETUP) 
	!= (gj->status == GAME_EXAMINE || gj->status == GAME_SETUP))
		return gi->status == GAME_EXAMINE || gi->status == GAME_SETUP ? -1 : 1;
	else
		return GetRating(&player_globals.parray[gi->white], gi->type)
			 + GetRating(&player_globals.parray[gi->black], gi->type)
			 - GetRating(&player_globals.parray[gj->white], gj->type)
			 - GetRating(&player_globals.parray[gj->black], gj->type);
}

int com_games(int p, param_list param)
{
	struct game *gi;
	char *s = NULL;
	int i, j, wp, bp, ws, bs, totalcount;
	int selected = 0, count = 0, slen = 0;
	int *sortedgames;		/* for qsort */

	totalcount = game_count();
	if (totalcount == 0)
		pprintf(p, _("There are no games in progress.\n"));
	else {
		sortedgames = malloc(totalcount * sizeof(int));	/* for qsort */

		if (param[0].type == TYPE_WORD) {
			s = param[0].val.word;
			slen = strlen(s);
			selected = MAX(atoi(s),0);
		}
		for (i = 0; i < game_globals.g_num; i++) {
			gi = &game_globals.garray[i];
			if (gi->status != GAME_ACTIVE 
			 && gi->status != GAME_EXAMINE 
			 && gi->status != GAME_SETUP)
				continue;
			if (selected && selected != i + 1)
				continue;		/* not selected game number */
			wp = gi->white;
			bp = gi->black;
			if (selected || !s || !strncasecmp(s, gi->white_name, slen) 
							   || !strncasecmp(s, gi->black_name, slen))
				sortedgames[count++] = i;
		}
		if (!count)
			pprintf(p, _("No matching games were found (of %d in progress).\n"),
					totalcount);
		else {
			qsort(sortedgames, count, sizeof(int), gamesortfunc);
			pprintf(p, "\n");
			for (j = 0; j < count; j++) {
				i = sortedgames[j];
				gi = &game_globals.garray[i];
				wp = gi->white;
				bp = gi->black;
				board_calc_strength(&gi->game_state, &ws, &bs);
				if (gi->status != GAME_EXAMINE && gi->status != GAME_SETUP){
					pprintf_noformat(p, "%3d %4s %-11.11s %4s %-10.10s [%c%c%c%3d %3d] ",
									 i + 1,
									 ratstrii(GetRating(&player_globals.parray[wp],gi->type), wp),
									 player_globals.parray[wp].name,
									 ratstrii(GetRating(&player_globals.parray[bp],gi->type), bp),
									 player_globals.parray[bp].name,
									 gi->private ? 'p' : ' ',
									 *bstr[gi->type],
									 *rstr[gi->rated],
									 gi->wInitTime / 600,
									 gi->wIncrement / 10);
					game_update_time(i);
					pprintf_noformat(p, "%6s -", tenth_str(MAX(gi->wTime,0), 0));
					pprintf_noformat(p, "%6s (%2d-%2d) %c: %2d\n",
									 tenth_str(MAX(gi->bTime,0), 0),
									 ws, bs,
									 gi->game_state.onMove == WHITE ? 'W' : 'B',
									 gi->game_state.moveNum);
				} else {
					pprintf_noformat(p, "%3d (%s %4d %-11.11s %4d %-10.10s) [%c%c%c%3d %3d] ",
									 i + 1,
									 gi->status == GAME_EXAMINE ? _("Exam.") : _("Setup"),
									 gi->white_rating,
									 gi->white_name,
									 gi->black_rating,
									 gi->black_name,
									 gi->private ? 'p' : ' ',
									 *bstr[gi->type],
									 *rstr[gi->rated],
									 gi->wInitTime / 600,
									 gi->wIncrement / 10);
					pprintf_noformat(p, "%c: %2d\n",
									 gi->game_state.onMove == WHITE ? 'W' : 'B',
									 gi->game_state.moveNum);
				}
			}
			if (count < totalcount)
				pprintf(p, ngettext("\n  %d game displayed (of %d in progress).\n",
									"\n  %d games displayed (of %d in progress).\n",
									count), 
						count, totalcount);
		else
			pprintf(p, ngettext("\n  %d game displayed.\n",
	                            "\n  %d games displayed.\n", totalcount),
					totalcount);
		}
		free(sortedgames);
	}
	return COM_OK;
}

static int do_observe(int p, int obgame)
{
	struct player *pp = &player_globals.parray[p];
  
	if (game_globals.garray[obgame].private && pp->adminLevel < ADMIN_ADMIN) {
		pprintf(p, _("Sorry, game %d is a private game.\n"), obgame + 1);
		return COM_OK;
	}
	if( (game_globals.garray[obgame].white == p || game_globals.garray[obgame].black == p)
		&& (game_globals.garray[obgame].status != GAME_EXAMINE 
    	 || game_globals.garray[obgame].status != GAME_SETUP) )
	{
		pprintf(p, _("You cannot observe a game that you are playing.\n"));
		return COM_OK;
	}

	if (player_is_observe(p, obgame)) {
		pprintf(p, U_("Removing game %d from observation list.\n"), obgame + 1); /* xboard */
		player_remove_observe(p, obgame);
	} else {
		if (!player_add_observe(p, obgame)) {
			pprintf(p, _("You are now observing game %d.\n"), obgame + 1);
			send_board_to(obgame, p);
		} else
			pprintf(p, _("You are already observing the maximum number of games.\n"));
	}
	return COM_OK;
}

void unobserveAll(int p)
{
	struct player *pp = &player_globals.parray[p];
	int i;

	for (i = 0; i < pp->num_observe; i++) {
		pprintf(p, U_("Removing game %d from observation list.\n"), /* xboard */
				pp->observe_list[i] + 1);
	}
	pp->num_observe = 0;
}

int com_unobserve(int p, param_list param)
{
	int gNum, p1;

	if (param[0].type == TYPE_NULL) {
		unobserveAll(p);
		return COM_OK;
	}
	gNum = GameNumFromParam(p, &p1, &param[0]);
	if (gNum < 0)
		return COM_OK;
	if (!player_is_observe(p, gNum)) {
		pprintf(p, _("You are not observing game %d.\n"), gNum);
	} else {
		player_remove_observe(p, gNum);
		pprintf(p, U_("Removing game %d from observation list.\n"), gNum + 1); /* xboard */
	}
	return COM_OK;
}

int com_observe(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int i, p1, obgame;

	if (param[0].type == TYPE_NULL)
		return COM_BADPARAMETERS;
	if ((pp->game >=0) &&(game_globals.garray[pp->game].status == GAME_EXAMINE)) {
		pprintf(p, _("You are still examining a game.\n"));
		return COM_OK;
	}
	if ((pp->game >=0) &&(game_globals.garray[pp->game].status == GAME_SETUP)) {
		pprintf(p, _("You are still setting up a position.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_NULL) {
		unobserveAll(p);
		return COM_OK;
	}
	obgame = GameNumFromParam(p, &p1, &param[0]);
	if (obgame < 0)
		return COM_OK;

	if (obgame >= game_globals.g_num 
		|| (game_globals.garray[obgame].status != GAME_ACTIVE &&
			game_globals.garray[obgame].status != GAME_EXAMINE &&
			game_globals.garray[obgame].status != GAME_SETUP))
	{
		pprintf(p, _("There is no such game.\n"));
		return COM_OK;
	}
	if (p1 >= 0 && player_globals.parray[p1].simul_info != NULL 
		&& player_globals.parray[p1].simul_info->numBoards)
	{
		for (i = 0; i < player_globals.parray[p1].simul_info->numBoards; i++)
			if (player_globals.parray[p1].simul_info->boards[i] >= 0)
				do_observe(p, player_globals.parray[p1].simul_info->boards[i]);
	} else
		do_observe(p, obgame);
	return COM_OK;
}

int com_allobservers(int p, param_list param)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	int obgame, p1, start, end, g, first;

	if (param[0].type == TYPE_NULL) {
		obgame = -1;
	} else {
		obgame = GameNumFromParam(p, &p1, &param[0]);
		if (obgame < 0)
			return COM_OK;
	}
	if (obgame == -1) {
		start = 0;
		end = game_globals.g_num;
	} else if ( obgame >= game_globals.g_num || (obgame < game_globals.g_num
				&& game_globals.garray[obgame].status != GAME_ACTIVE
				&& game_globals.garray[obgame].status != GAME_EXAMINE
				&& game_globals.garray[obgame].status != GAME_SETUP) ) {
		pprintf(p, _("There is no such game.\n"));
		return COM_OK;
	} else {
		start = obgame;
		end = obgame + 1;
	}

	/* list games being played */

	for (g = start; g < end; g++) {
		gg = &game_globals.garray[g];
		if (gg->status == GAME_ACTIVE && (pp->adminLevel > 0 || gg->private == 0)) {
			for (first = 1, p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status != PLAYER_EMPTY 
					&& player_is_observe(p1, g))
				{
					if (first) {
						pprintf(p, _("Observing %2d [%s vs. %s]:"), g + 1,
								player_globals.parray[gg->white].name,
								player_globals.parray[gg->black].name);
						first = 0;
					}
					pprintf(p, " %s%s", (player_globals.parray[p1].game >=0) ? "#" : "", 
							player_globals.parray[p1].name);
				}
			}
			if (!first)
				pprintf(p, "\n");
		}
	}

	/* list games being examined last */

	for (g = start; g < end; g++) {
		gg = &game_globals.garray[g];
		if ((gg->status == GAME_EXAMINE || gg->status == GAME_SETUP) 
			&& (pp->adminLevel > 0 || !gg->private))
		{
			for (first = 1, p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status != PLAYER_EMPTY 
				&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
				{
					if (first) {
						if (strcmp(gg->white_name, gg->black_name))
							pprintf(p, _("Examining %2d [%s vs %s]:"), g + 1,
									gg->white_name, gg->black_name);
						else
							pprintf(p, _("Examining %2d (scratch):"), g + 1);
						first = 0;
					}
					pprintf(p, " %s%s",
							player_globals.parray[p1].game == g ? "#" : "", 
							player_globals.parray[p1].name);
				}
			}
			if (!first)
				pprintf(p, "\n");
		}
	}
	return COM_OK;
}

int com_unexamine(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, p1, flag = 0;

	if (pp->game <0 || (game_globals.garray[pp->game].status != GAME_EXAMINE 
					 && game_globals.garray[pp->game].status != GAME_SETUP))
	{
		pprintf(p, _("You are not examining any games.\n"));
		return COM_OK;
	}
	g = pp->game;
	pp->game = -1;
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_globals.parray[p1].status != PLAYER_PROMPT)
			continue;
		if (player_globals.parray[p1].game == g && p != p1) {
			/* ok - there are other examiners to take over the game */
			flag = 1;
		}
		if (player_is_observe(p1, g) || player_globals.parray[p1].game == g)
			pprintf(p1, _("%s stopped examining game %d.\n"), pp->name, g + 1);
	}
	if (!flag) {
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (player_globals.parray[p1].status == PLAYER_PROMPT
				&& player_is_observe(p1, g))
			{
				pprintf(p1, _("There are no examiners.\n"));
				pcommand(p1, "unobserve %d", g + 1);
			}
		}
		game_remove(g);
	}
	pprintf(p, U_("You are no longer examining game %d.\n"), g + 1); /* xboard */
	announce_avail(p);
	return COM_OK;
}

int com_mexamine(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g, p1, p2;

	if (pp->game < 0 || (game_globals.garray[pp->game].status != GAME_EXAMINE 
					  && game_globals.garray[pp->game].status != GAME_SETUP)) {
		pprintf(p, _("You are not examining any games.\n"));
		return COM_OK;
	}
	p1 = player_find_part_login(param[0].val.word);
	if (p1 < 0) {
		pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
		return COM_OK;
	}
	g = pp->game;
	if (!player_is_observe(p1, g)) {
		pprintf(p, _("%s must observe the game you are analysing.\n"), 
				player_globals.parray[p1].name);
		return COM_OK;
	} else {
		if (player_globals.parray[p1].game >=0) {
			pprintf(p, _("%s is already analysing the game.\n"), 
					player_globals.parray[p1].name);
			return COM_OK;
		}
		/* if we get here - let's make him examiner of the game */
		unobserveAll(p1);		/* fix for Xboard */
		decline_withdraw_offers(p1, -1, PEND_MATCH, DO_DECLINE | DO_WITHDRAW);
		decline_withdraw_offers(p1, -1, PEND_SIMUL, DO_WITHDRAW);
	
		player_globals.parray[p1].game = g;	/* yep - it really is that easy :-) */
		pprintf(p1, _("You are now examiner of game %d.\n"), g + 1);
		send_board_to(g, p1);	/* pos not changed - but fixes Xboard */
		for (p2 = 0; p2 < player_globals.p_num; p2++) {
			if (player_globals.parray[p2].status == PLAYER_PROMPT && p2 != p1
				&& (player_is_observe(p2, g) || player_globals.parray[p2].game == g))
			{
				pprintf_prompt(p2, _("%s is now examiner of game %d.\n"), 
				player_globals.parray[p1].name, g + 1);
			}
		}
	}
	if (CheckPFlag(p2, PFLAG_OPEN)) /* was open */
		announce_notavail(p2);
	return COM_OK;
}

int com_moves(int p, param_list param)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	int g, p1;

	if (param[0].type == TYPE_NULL) {
		if (pp->game >=0) {
			g = pp->game;
		} else if (pp->num_observe) {
			for (g = 0; g < pp->num_observe; g++)
				pprintf(p, "%s\n", movesToString(pp->observe_list[g], 0));
			return COM_OK;
		} else {
			pprintf(p, _("You are neither playing, observing nor examining a game.\n"));
			return COM_OK;
		}
	} else {
		g = GameNumFromParam(p, &p1, &param[0]);
		if (g < 0)
			return COM_OK;
	}
	
	gg = &game_globals.garray[g];
	if (g < 0 || g >= game_globals.g_num || (gg->status != GAME_ACTIVE &&
	  	gg->status != GAME_EXAMINE && gg->status != GAME_SETUP))
	{
		pprintf(p, _("There is no such game.\n"));
		return COM_OK;
	}
	if (gg->white != p && gg->black != p 
		&& gg->private && pp->adminLevel < ADMIN_ADMIN)
	{
		pprintf(p, _("Sorry, that is a private game.\n"));
		return COM_OK;
	}
	pprintf(p, "%s\n", movesToString(g, 0));	/* pgn may break interfaces? */
	return COM_OK;
}

int com_mailmoves(int p, param_list param)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	int g, p1;
	char subj[81];

	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf (p,_("Unregistered players cannot use mailmoves.\n"));
		return COM_OK;
	}

	if (param[0].type == TYPE_NULL) {
		if (pp->game >=0) {
			g = pp->game;
		} else {
			pprintf(p, _("You are neither playing, observing nor examining a game.\n"));
			return COM_OK;
		}
	} else {
		g = GameNumFromParam(p, &p1, &param[0]);
		if (g < 0)
			return COM_OK;
	}
	gg = &game_globals.garray[g];
	if (g < 0 || g >= game_globals.g_num || (gg->status != GAME_ACTIVE 
										  && gg->status != GAME_EXAMINE)) {
		pprintf(p, _("There is no such game.\n"));
		return COM_OK;
	}
	if (gg->white != p && gg->black != p 
	 && gg->private && pp->adminLevel < ADMIN_ADMIN) {
		pprintf(p, _("Sorry, that is a private game.\n"));
		return COM_OK;
	}
	
	sprintf(subj, _("FICS game report %s vs %s"), gg->white_name, gg->black_name);
	if (mail_string_to_user(p, subj, movesToString(g, CheckPFlag(p, PFLAG_PGN))))
		pprintf(p, _("Moves NOT mailed, perhaps your address is incorrect.\n"));
	else
		pprintf(p, _("Moves mailed.\n"));
	return COM_OK;
}

void ExamineScratch(int p,  param_list param, int setup)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	char category[100], board[100], parsebuf[100];
	char *val;
	int confused = 0, g;

	category[0] = '\0';
	board[0] = '\0';

	if (param[0].val.string != pp->name && param[1].type == TYPE_WORD) {
		strncpy(category, param[0].val.string, sizeof(category));
		strncpy(board, param[1].val.string, sizeof(board));
		category[sizeof(category)-1] = '\0';
		board[sizeof(board)-1] = '\0';
	} else if (param[1].type != TYPE_NULL) {
		val = param[1].val.string;
		while (!confused && sscanf(val, " %99s", parsebuf) == 1) {
			val = eatword(eatwhite(val));
			if ((category[0] != '\0') && (board[0] == '\0'))
				strcpy(board, parsebuf);
			else if (isdigit(*parsebuf)) {
				pprintf(p, _("You can't specify time controls.\n"));
				return;
			} else if (category[0] == '\0')
				strcpy(category, parsebuf);
			else
				confused = 1;
		}
		if (confused) {
			pprintf(p, _("Can't interpret %s in match command.\n"), parsebuf);
			return;
		}
	}

	if (category[0] && !board[0]) {
		pprintf(p, _("You must specify a board and a category.\n"));
		return;
	}

	g = game_new();
	gg = &game_globals.garray[g];

	unobserveAll(p);

	decline_withdraw_offers(p, -1, PEND_MATCH, DO_DECLINE | DO_WITHDRAW);
	decline_withdraw_offers(p, -1, PEND_SIMUL, DO_WITHDRAW);

	gg->wInitTime = gg->wIncrement = 0;
	gg->bInitTime = gg->bIncrement = 0;
	gg->timeOfStart = tenth_secs();
	gg->wTime = gg->bTime = 0;
	gg->rated = 0;
	gg->clockStopped = 0;
	gg->type = TYPE_UNTIMED;
	gg->white = gg->black = p;
	gg->startTime = tenth_secs();
	gg->lastMoveTime = gg->lastDecTime = gg->startTime;
	gg->totalHalfMoves = 0;

	pp->side = WHITE;       /* oh well... */
	pp->game = g;

	pprintf(p, _("Starting a game in examine (%s) mode.\n"), 
			setup ? _("setup") : _("scratch"));

	if (category[0])
		pprintf(p, _("Loading from category: %s, board: %s.\n"), category, board);

	if (setup) {
		board_clear(&gg->game_state);
		gg->status = GAME_SETUP;
	} else {
		gg->status = GAME_EXAMINE;
		if (board_init(g,&gg->game_state, category, board)) {
			pprintf(p, _("PROBLEM LOADING BOARD. Examine aborted.\n"));
			d_printf( _("CHESSD: PROBLEM LOADING BOARD. Examine aborted.\n"));
			pp->game = -1;
			game_remove(g);
			return;
		}
	}

	gg->game_state.gameNum = g;
	strcpy(gg->white_name, pp->name);
	strcpy(gg->black_name, pp->name);
	gg->white_rating = gg->black_rating = pp->s_stats.rating;

	send_boards(g);
	if (CheckPFlag(p, PFLAG_OPEN)) /*was open */
		announce_notavail(p);
}

int com_wname(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g = pp->game;

	if (g < 0 || !(game_globals.garray[g].status != GAME_EXAMINE 
  				|| game_globals.garray[g].status != GAME_SETUP) )
	{
		pprintf(p, _("You are not examining or setting up a game.\n"));
		return COM_OK;
	}

	if (param[0].type == TYPE_NULL)
		strcpy(game_globals.garray[g].white_name, pp->name);
	else {
		if (strlen(param[0].val.word) > MAX_LOGIN_NAME - 1) {
			pprintf(p,_("The maximum length of a name is %d characters.\n"),
					MAX_LOGIN_NAME - 1);
			return COM_OK;
		} else
			strcpy(game_globals.garray[g].white_name, param[0].val.word);
	}

	send_boards(g);
	return COM_OK;
}

int com_bname(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int g = pp->game;

	if ( g < 0 || !(game_globals.garray[g].status != GAME_EXAMINE 
				 || game_globals.garray[g].status != GAME_SETUP) )
	{
		pprintf(p, _("You are not examining or setting up a game.\n"));
		return COM_OK;
	}

	if (param[0].type == TYPE_NULL)
		strcpy(game_globals.garray[g].black_name,pp->name);
	else {
		if (strlen(param[0].val.word) > MAX_LOGIN_NAME - 1) {
			pprintf(p,_("The maximum length of a name is %d characters.\n"),MAX_LOGIN_NAME - 1);
			return COM_OK;
		} else
			strcpy(game_globals.garray[g].black_name,param[0].val.word);
	}

	send_boards(g);
	return COM_OK;
}

/*
 * gabrielsan:
 * TODO: need to change this function to access the database instead;
 *
 */
int com_examine(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	//int p1, p2 = p, p1conn, p2conn = 1;
	int p1, p1conn;
//  char* wincstring;
	char fname[MAX_FILENAME_SIZE];

	if (pp->game >= 0) {
		if (game_globals.garray[pp->game].status == GAME_EXAMINE 
		 || game_globals.garray[pp->game].status == GAME_SETUP)
			pprintf(p, _("You are already examining a game.\n"));
		else
			pprintf(p, _("You are playing a game.\n"));
	}
	else if (param[0].type == TYPE_NULL)
		ExamineScratch(p, param, 0);
	else if (param[0].type != TYPE_NULL) {
		if (param[1].type == TYPE_NULL && !strcmp(param[0].val.word,"setup")) {
			ExamineScratch(p, param, 1);
			return COM_OK;
		} else if (param[1].type == TYPE_WORD) {
			sprintf(fname, "%s/%s/%s", BOARD_DIR, param[0].val.word,
					  param[1].val.word);
			if (file_exists(fname)) {
				ExamineScratch(p, param, 0);
				return COM_OK;
			}
		}
		if (!FindPlayer(p, param[0].val.word, &p1, &p1conn))
			return COM_OK;

#if 0
    if (param[1].type == TYPE_INT)
      ExamineHistory(p, p1, param[1].val.integer);
    else {
      if (param[1].type == TYPE_WORD) {

        /* Lets check the journal */
        wincstring = param[1].val.word;
        if ((strlen(wincstring) == 1) && (isalpha(wincstring[0]))) {
          ExamineJournal(p,p1,wincstring[0]);
          if (!p1conn)
            player_remove(p1);
          return COM_OK;
        } else {
          if (!FindPlayer(p, param[1].val.word, &p2, &p2conn)) {
            if (!p1conn)
              player_remove(p1);
            return COM_OK;
          }
        }
      }
      ExamineAdjourned(p, p1, p2);
      if (!p2conn)
       player_remove(p2);
    }
    if (!p1conn)
     player_remove(p1);
#endif
	}
	return COM_OK;
}

/* Tidied up a bit but still messy */

int com_sposition(int p, param_list param)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	int wp, wconnected, bp, bconnected, confused = 0, g;

	if (!FindPlayer(p, param[0].val.word, &wp, &wconnected))
		return COM_OK;
	if (!FindPlayer(p, param[1].val.word, &bp, &bconnected)) {
		if (!wconnected)
			player_remove(wp);
		return COM_OK;
	}

	g = game_new();
	gg = &game_globals.garray[g];
	if (game_read(g, p, bp) < 0) {	/* if no game white-black, */
		if (game_read(g, bp, p) < 0) {	/* look for black-white */
			confused = 1;
			pprintf(p, _("There is no stored game %s vs. %s\n"),
					player_globals.parray[wp].name,
					player_globals.parray[bp].name);
		} else {
			int tmp = wp;
			wp = bp;
			bp = tmp;
			tmp = wconnected;
			wconnected = bconnected;
			bconnected = tmp;
		}
	}

	if (!confused) {
		if (wp != p && bp != p && gg->private && pp->adminLevel < ADMIN_ADMIN)
			pprintf(p, _("Sorry, that is a private game.\n"));
		else {
			gg->white = wp;
			gg->black = bp;
			gg->startTime = tenth_secs();
			gg->lastMoveTime = gg->lastDecTime = gg->startTime;
			pprintf(p, _("Position of stored game %s vs. %s\n"),
					player_globals.parray[wp].name, player_globals.parray[bp].name);
			send_board_to(g, p);
		}
	}
	game_remove(g);
	if (!wconnected)
		player_remove(wp);
	if (!bconnected)
		player_remove(bp);
	return COM_OK;
}

int com_forward(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int nHalfMoves = 1, g, i, p1;
	unsigned now;

	if (!(pp->game >=0 && (game_globals.garray[pp->game].status == GAME_EXAMINE 
						|| game_globals.garray[pp->game].status == GAME_SETUP)))
	{
		pprintf(p, _("You are not examining any games.\n"));
		return COM_OK;
	}
	if (game_globals.garray[pp->game].status == GAME_SETUP) {
		pprintf (p,_("You can't move forward yet, the position is still being set up.\n"));
		return COM_OK;
	}
	g = pp->game;
	gg = &game_globals.garray[g];
	if (!strcmp(gg->white_name, gg->black_name)) {
		pprintf(p, _("You cannot go forward; no moves are stored.\n"));
		return COM_OK;
	}
	if (param[0].type == TYPE_INT)
		nHalfMoves = param[0].val.integer;
	if (gg->numHalfMoves > gg->revertHalfMove) {
		pprintf(p, _("Game %u: No more moves.\n"), g);
		return COM_OK;
	}
	if (gg->numHalfMoves < gg->totalHalfMoves) {
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (player_globals.parray[p1].status == PLAYER_PROMPT
				&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g) )
			{
				pprintf(p1, ngettext("Game %u: %s goes forward %d move.\n",
									 "Game %u: %s goes forward %d moves.\n",
									 nHalfMoves),
						g, pp->name, nHalfMoves);
			}
		}
	}
	for (i = 0; i < nHalfMoves; i++) {
		if (gg->numHalfMoves < gg->totalHalfMoves) {
			execute_move(&gg->game_state, &gg->moveList[gg->numHalfMoves], 1);
			if (gg->numHalfMoves + 1 > gg->examMoveListSize) {
				gg->examMoveListSize += 20;	/* Allocate 20 moves at a time */
				gg->examMoveList = realloc(gg->examMoveList, 
								sizeof(struct move_t) * gg->examMoveListSize);
			}
			gg->examMoveList[gg->numHalfMoves] = gg->moveList[gg->numHalfMoves];
			gg->revertHalfMove++;
			gg->numHalfMoves++;
		} else {
			for (p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status == PLAYER_PROMPT 
				&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
					pprintf(p1, _("Game %u: End of game.\n"), g);
			}
			break;
		}
	}
	/* roll back time */
	if (gg->game_state.onMove == WHITE)
		gg->wTime += (gg->lastDecTime - gg->lastMoveTime);
	else
		gg->bTime += (gg->lastDecTime - gg->lastMoveTime);

	now = tenth_secs();
	if (gg->numHalfMoves == 0)
		gg->timeOfStart = now;
	gg->lastMoveTime = gg->lastDecTime = now;
	send_boards(g);

	if (gg->revertHalfMove == gg->totalHalfMoves) {
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (player_globals.parray[p1].status == PLAYER_PROMPT
			&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
			  pprintf(p1, _("Game %u: %s %s\n"), g, EndString(g,0), EndSym(g));
		}
	}

	return COM_OK;
}

int com_backward(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int nHalfMoves = 1, g, i, p1;
	unsigned now;

	if (!(pp->game >=0 && (game_globals.garray[pp->game].status == GAME_EXAMINE 
						|| game_globals.garray[pp->game].status == GAME_SETUP)))
	{
		pprintf(p, _("You are not examining any games.\n"));
		return COM_OK;
	}
	if (game_globals.garray[pp->game].status == GAME_SETUP) {
		pprintf (p,_("You can't move backward yet, the position is still being set up.\n"));
		return COM_OK;
	}

	g = pp->game;
	gg = &game_globals.garray[g];
	if (param[0].type == TYPE_INT)
		nHalfMoves = param[0].val.integer;
	if (gg->numHalfMoves != 0) {
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (player_globals.parray[p1].status == PLAYER_PROMPT
			&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
				pprintf(p1, ngettext("Game %u: %s backs up %d move.\n",
									 "Game %u: %s backs up %d moves.\n",
									 nHalfMoves),
						g, pp->name, nHalfMoves);
		}
	}
	for (i = 0; i < nHalfMoves; i++) {
		if (backup_move(g, REL_EXAMINE) != MOVE_OK) {
			for (p1 = 0; p1 < player_globals.p_num; p1++) {
				if (player_globals.parray[p1].status == PLAYER_PROMPT
				&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
				  pprintf(p1, _("Game %u: Beginning of game.\n"), g);
			}
			break;
		}
	}
	if (gg->numHalfMoves < gg->revertHalfMove)
		gg->revertHalfMove = gg->numHalfMoves;

	/* roll back time */
	if (gg->game_state.onMove == WHITE)
		gg->wTime += (gg->lastDecTime - gg->lastMoveTime);
	else
		gg->bTime += (gg->lastDecTime - gg->lastMoveTime);

	now = tenth_secs();
	if (gg->numHalfMoves == 0)
		gg->timeOfStart = now;
	gg->lastMoveTime = gg->lastDecTime = now;
	send_boards(g);
	return COM_OK;
}

int com_revert(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int nHalfMoves = 1, g, i, p1;
	unsigned now;

	if (!(pp->game >=0 && (game_globals.garray[pp->game].status == GAME_EXAMINE
						|| game_globals.garray[pp->game].status == GAME_SETUP)))
	{
		pprintf(p, _("You are not examining any games.\n"));
		return COM_OK;
	}
	if (game_globals.garray[pp->game].status == GAME_SETUP) {
		pprintf (p,_("You can't move revert yet, the position is still being set up.\n"));
		return COM_OK;
	}
	g = pp->game;
	gg = &game_globals.garray[g];
	nHalfMoves = gg->numHalfMoves - gg->revertHalfMove;
	if (nHalfMoves == 0) {
		pprintf(p, _("Game %u: Already at mainline.\n"), g);
		return COM_OK;
	}
	if (nHalfMoves < 0) {		/* eek - should NEVER happen! */
		d_printf( _("OUCH! in com_revert: nHalfMoves < 0\n"));
		return COM_OK;
	}
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_globals.parray[p1].status == PLAYER_PROMPT
		&& (player_is_observe(p1, g) || player_globals.parray[p1].game == g))
			pprintf(p1, _("Game %u: %s reverts to mainline.\n"), g, pp->name);
	}
	for (i = 0; i < nHalfMoves; i++)
		backup_move(g, REL_EXAMINE); /* should never return error */
	
	/* roll back time */
	if (gg->game_state.onMove == WHITE)
		gg->wTime += (gg->lastDecTime - gg->lastMoveTime);
	else
		gg->bTime += (gg->lastDecTime - gg->lastMoveTime);
	
	now = tenth_secs();
	if (gg->numHalfMoves == 0)
		gg->timeOfStart = now;
	gg->lastMoveTime = gg->lastDecTime = now;
	send_boards(g);
	return COM_OK;
}

int com_refresh(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct game *gg;
	int g, p1;

	if (param[0].type == TYPE_NULL) {
		if (pp->game >= 0)
			send_board_to(pp->game, p);
		else {                    /* Do observing in here */
			if (pp->num_observe) {
				for (g = 0; g < pp->num_observe; g++)
					send_board_to(pp->observe_list[g], p);
			} else {
				pprintf(p, _("You are neither playing, observing nor examining a game.\n"));
				return COM_OK;
			}
		}
	} else {
		g = GameNumFromParam (p, &p1, &param[0]);
		gg = &game_globals.garray[g];
		if (g < 0)
			return COM_OK;
		if (g >= game_globals.g_num || (gg->status != GAME_ACTIVE
		&& (gg->status != GAME_EXAMINE || gg->status != GAME_SETUP)))
			pprintf(p, _("No such game.\n"));
		else {
			int link = gg->link;
			if ((gg->private && pp->adminLevel==ADMIN_USER) &&
				gg->white != p && gg->white != p1 && link != -1
				&& game_globals.garray[link].white != p 
				&& game_globals.garray[link].black != p)
			{
				pprintf (p, _("Sorry, game %d is a private game.\n"), g+1);
				return COM_OK;
			}
		
			if (gg->private)
				pprintf(p, _("Refreshing static game %d\n"), g+1);
			send_board_to(g, p);
		}
	}
	return COM_OK;
}

int com_prefresh(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int retval, part = pp->partner;

	if (part < 0) {
		pprintf(p, _("You do not have a partner.\n"));
		return COM_OK;
	}
	retval = pcommand (p, "refresh %s", player_globals.parray[part].name);
	return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}
