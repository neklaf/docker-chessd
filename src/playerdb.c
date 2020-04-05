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

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#ifdef __Linux__
#include <linux/stddef.h>
#endif
#include "board.h"
#include "chessd_locale.h"
#include "common.h"
#include "comproc.h"
#include "configuration.h"
#include "config_genstrc.h"
#include "chessd_main.h"
#include "gameproc.h"
#include "globals.h"
#include "lists.h"
#include "malloc.h"
#include "multicol.h"
#include "pending.h"
#include "playerdb.h"
//#include "playerdb_old.h"
#include "ratings.h"
#include "utils.h"
#include "dbdbi.h"

#include "protocol.h"

int player_count_messages(int p)
{
	dbi_result res;
	char *sql = "SELECT COUNT(status) FROM Message "
				"WHERE touserid = %i AND status = 0 AND messagetype = 0";

	if( (res = dbi_conn_queryf(dbiconn, sql, player_globals.parray[p].chessduserid))
		&& dbi_result_next_row(res) )
	{
		int sum = dbi_result_get_longlong_idx(res,1); // COUNT() returns BIGINT, PgSQL & MySQL
		dbi_result_free(res);
		return sum;
	}
	return 0;
}

static int player_zero(int p);

static int get_empty_slot(void)
{
	int i;

	for (i = 0; i < player_globals.p_num; i++) {
		if (player_globals.parray[i].status == PLAYER_EMPTY)
			return i;
	}

	if (i < player_globals.parray_size) {
		player_globals.p_num++;
		ZERO_STRUCT(player_globals.parray[i]);
		player_globals.parray[i].status = PLAYER_EMPTY;
		return i;
	}

	if (player_globals.parray_size >= config.ints[max_players]) {
		d_printf(_("Too many players connected! (%d)\n"), config.ints[max_players]);
		return -1;
	}

	player_globals.parray_size += 100;
	player_globals.parray = realloc(player_globals.parray,
						player_globals.parray_size * sizeof(struct player));

	player_globals.p_num++;
	ZERO_STRUCT(player_globals.parray[i]);
	player_globals.parray[i].status = PLAYER_EMPTY;
	return i;
}

int player_new(void)
{
	int new = get_empty_slot();
	player_zero(new);
	return new;
}

static void ResetStats(struct statistics *ss)
{
	ss->num = 0;
	ss->win = 0;
	ss->los = 0;
	ss->dra = 0;
	ss->rating = config.ints[default_rating];
	ss->sterr = config.ints[default_rd];
	ss->ltime = 0;
	ss->best = 0;
	ss->whenbest = 0;
}

static int player_zero(int p)
{
	struct player *pp = &player_globals.parray[p];

	ZERO_STRUCTP(pp);

	pp->prompt = strdup(config.strs[default_prompt]);
	pp->partner = -1;
	pp->socket = -1;
	pp->status = PLAYER_NEW;

	ResetStats(&pp->s_stats);
	ResetStats(&pp->b_stats);
	ResetStats(&pp->l_stats);
	ResetStats(&pp->w_stats);
	ResetStats(&pp->bug_stats);

	pp->d_time = config.ints[default_time];
	pp->d_inc = config.ints[default_increment];
	pp->d_height = 24;
	pp->d_width = 79;
//	pp->language = LANG_DEFAULT;
	pp->promote = QUEEN;
	pp->game = -1;
	pp->last_channel = -1;
	pp->ftell = -1;
	pp->Flags = PFLAG_DEFAULT;
	pp->opponent = -1;
	// gabrielsan: make sure this string is null, so a free can tell
	// whether this is set
	pp->last_opponent = NULL;

	// we should make sure that the formula is, at first, NULL
	pp->formula = NULL;
	pp->num_formula = 0;
	pp->alias_list = NULL;

	return 0;
}

void player_free(struct player *pp)
{
	int i;

	FREE(pp->login);
	FREE(pp->more_text);
	FREE(pp->name);
	FREE(pp->passwd);
	FREE(pp->fullName);
	FREE(pp->emailAddress);
	FREE(pp->prompt);
	FREE(pp->formula);
	FREE(pp->busy);
	FREE(pp->last_tell);
	FREE(pp->last_opponent);
	FREE(pp->simul_info);
	for (i = 0; i < pp->num_plan; i++)
		FREE(pp->planLines[i]);
	for (i = 0; i < pp->num_formula; i++)
		FREE(pp->formulaLines[i]);
	list_free(pp->lists);
	for (i = 0; i < pp->numAlias; i++) {
		FREE(pp->alias_list[i].comm_name);
		FREE(pp->alias_list[i].alias);
	}
}

int player_clear(int p)
{
	player_free(&player_globals.parray[p]);
	player_zero(p);
	return 0;
}

/**
 * used to remove a player from memory, not to delete it.
 */
int player_remove(int p)
{
	struct player *pp = &player_globals.parray[p];
	int i;

	decline_withdraw_offers(p, -1, -1, DO_DECLINE | DO_WITHDRAW);
	if (pp->simul_info != NULL)
		/* Player disconnected in middle of simul */
		for (i = 0; pp->simul_info != NULL
			     && i < pp->simul_info->numBoards; i++)
			if (pp->simul_info->boards[i] >= 0)
				game_disconnect(pp->simul_info->boards[i], p);

	if (pp->game >=0 && pIsPlaying(p)) {
		/* Player disconnected in the middle of a game! */
		pprintf(pp->opponent, _("Your opponent has lost contact or quit."));
		game_disconnect(pp->game, p);
	}
	for (i = 0; i < player_globals.p_num; i++) {
		if (player_globals.parray[i].status == PLAYER_EMPTY)
			continue;
		if (player_globals.parray[i].partner == p) {
            pprintf_prompt(i, U_("Your partner has disconnected.\n")); /* xboard */
			decline_withdraw_offers(i, -1, PEND_BUGHOUSE, DO_DECLINE | DO_WITHDRAW);
			player_globals.parray[i].partner = -1;
		}
	}
	player_clear(p);
	pp->status = PLAYER_EMPTY;

	return 0;
}

/**
 * Reads statistics from line i of result r into *s
 */
void read_stats_from_db (struct statistics *s, dbi_result res){
	dbi_result_get_fields(res,
		"num.%i wins.%i losses.%i draws.%i rating.%i "
		"lasttime.%i best.%i whenbest.%i sterr.%d",
		&s->num, &s->win, &s->los, &s->dra, &s->rating,
		&s->ltime, &s->best, &s->whenbest, &s->sterr);
}

/**
 * Write the stats of specified player (p) and game type (gametype)
 * to the database
 * Return 0 on success, -1 on error.
 */
int write_stats_to_db (int p, int gametype){

	struct player *pp = &player_globals.parray[p];
	struct statistics *st;
	int n;
	dbi_result res;

	// no need to save stats for a non-registered user
	if (pp->chessduserid == -1)
		return 0;

	switch (gametype) {
	case TYPE_BLITZ:	st = &(pp->b_stats); break;
	case TYPE_STAND:	st = &(pp->s_stats); break;
	case TYPE_WILD:		st = &(pp->w_stats); break;
	case TYPE_BUGHOUSE:	st = &(pp->bug_stats); break;
	case TYPE_LIGHT:	st = &(pp->l_stats); break;
	default:
		pprintf(p, _("WARNING: Invalid gametype saving stats. [%d]\n"), gametype);
		d_printf(_("Invalid gametype saving stats. [%d]\n"), gametype);
	case TYPE_UNTIMED: // should not save
		return -1;
	}

	/* update or insert ? */
	res = dbi_conn_queryf( dbiconn, "SELECT COUNT(*) FROM UserStats "
								    "WHERE userid = %i AND gametypeid = %i",
						   pp->chessduserid, gametype );
	n = res && dbi_result_next_row(res) ? dbi_result_get_longlong_idx(res,1) : 0;
	if(res) dbi_result_free(res);

	if (n==0) { /* insert */
		// note that dbi_conn_queryf() format string follows printf()
		// and not dbi_result_get_fields(): a double parameter is
		// represented in the format string by '%f', '%g' or '%e', NOT '%d'!
		// (note %i means an 'int' in either context)
		res = dbi_conn_queryf( dbiconn,
			"INSERT INTO UserStats (userid, gametypeid, num, wins, losses, draws,"
								   "rating, lasttime, whenbest, best, sterr) "
						   "VALUES (%i, %i, %i, %i, %i, %i, "
								   "%i, %i, %i, %i, %f)",
			pp->chessduserid, gametype, st->num, st->win, st->los, st->dra,
			st->rating, st->ltime, st->whenbest, st->best, st->sterr );
		if(!res){
			pprintf(p, _("WARNING: Error saving stats (gametype=%d)!\n"), gametype);
			d_printf("Error saving stats (playerid=%d gametype=%d)\n", pp->chessduserid, gametype);
		}
	}else{
		/* user already has stats for this gametype. we will update them */
		res = dbi_conn_queryf( dbiconn,
			"UPDATE UserStats SET num=%i, wins=%i, losses=%i, draws=%i, "
								 "rating=%i, lasttime=%i, whenbest=%i, "
								 "best=%i, sterr=%f "
			"WHERE userid=%i AND gametypeid=%i",
			st->num, st->win, st->los, st->dra, 
			st->rating, st->ltime, st->whenbest, 
			st->best, st->sterr, 
			pp->chessduserid, gametype );
		if(!res){
			pprintf(p, _("WARNING: Error updating your stats (gametype=%d)\n"), gametype);
			d_printf("Error updating stats (playerid=%d gametype=%d)\n", pp->chessduserid, gametype);
		}
	}
	if(res) dbi_result_free(res);

	return 0;
}

void listaddusers(int p, int list, char *table, int ownerid)
{
	dbi_result res;

	if( (res = dbi_conn_queryf( dbiconn, "SELECT targetuser FROM %s "
										 "WHERE ownerid = %i", table, ownerid)) )
	{
		while(dbi_result_next_row(res))
			list_add(p, list, dbi_result_get_string_idx(res,1), 1);
		dbi_result_free(res);
	}
}

void listaddchannels(int p, int list, char *table, int ownerid)
{
	dbi_result res;
	char tmp[10];

	if( (res = dbi_conn_queryf( dbiconn, "SELECT channel FROM %s "
										 "WHERE ownerid = %i", table, ownerid)) )
	{
		while(dbi_result_next_row(res)){
			sprintf(tmp, "%d", dbi_result_get_int_idx(res,1));
			list_add(p, list, tmp, 1);
		}
		dbi_result_free(res);
	}
}

int player_read(int p, const char *name)
{
	struct player *pp = &player_globals.parray[p];
	int n, i, gtype, pdtime, pdinc, pdheight, pdwidth, 
		ppromote, plastchannel, pflags;
	dbi_result res;
	char *f_lines_aux, *fstart, *fp, *pprompt;
	char *sql = "SELECT * FROM ChessdUser "
				"WHERE UPPER(username) = UPPER('%s') AND NOT deleted";

	// BUG? need to FREE(pp->login) too?
	pp->login = stolower(strdup(name));

	FREE(pp->name); 
	pp->name = strdup(name);
		
	res = dbi_conn_queryf(dbiconn, sql, pp->login); // FIXME: should escape this string?
	if (!res || !dbi_result_next_row(res)) { /* unregistered player */
		if(res) dbi_result_free(res);
		// strtolower removed
		PFlagOFF(p, PFLAG_REG);
		return -1;
	}

	PFlagON(p, PFLAG_REG); /* let's load the file */
	pp->loading = 1;

	/* load data */
	// several fields are loaded into temporary variables
	// rather than directly into the player record, because they may
	// be NULLs that will clobber good defaults in the existing struct
	dbi_result_get_fields( res,
		"chessduserid.%i adminlevel.%i passwd.%S prompt.%s emailaddress.%S "
		"fullname.%S lastopponent.%S lasttell.%S formula.%S numFormulaLines.%i "
		"formulalines.%s lastchannel.%i timeofreg.%i defaulttime.%i "
		"defaultinc.%i availmax.%i availmin.%i defaultheight.%i defaultwidth.%i "
		"highlight.%i language.%i flags.%i numblack.%i "
		"numwhite.%i promote.%i totaltime.%i style.%i",
		&pp->chessduserid, &pp->adminLevel, &pp->passwd, &pprompt, &pp->emailAddress,
		&pp->fullName, &pp->last_opponent, &pp->last_tell, &pp->formula, &pp->num_formula,
		&f_lines_aux, &plastchannel, &pp->timeOfReg, &pdtime,
		&pdinc, &pp->availmax, &pp->availmin, &pdheight, &pdwidth, 
		&pp->highlight, &pp->language, &pflags, &pp->num_black, 
		&pp->num_white, &ppromote, &pp->totalTime, &pp->style );

	// unless actually set in db, don't stomp on defaults set by player_zero()
	if(pprompt){ FREE(pp->prompt); pp->prompt = strdup(pprompt); }
	if(pdtime) pp->d_time = pdtime;
	if(pdinc) pp->d_inc = pdinc;
	if(pdheight) pp->d_height = pdheight;
	if(pdwidth) pp->d_width = pdwidth;
	if(ppromote) pp->promote = ppromote;
	if(plastchannel) pp->last_channel = plastchannel;
	if(pflags) pp->Flags = pflags;

	// gabrielsan: the formulas were missing
	// just make sure we don't get some weird value around here
	if (pp->num_formula < 0 || pp->num_formula > MAX_FORMULA)
		pp->num_formula = 0;

	if (pp->num_formula > 0) {
		// recover the formula lines
		for ( i = 0, fstart = fp = f_lines_aux ; *fp ; ++fp ){
			if (*fp == '\n'){
				n = fp - fstart;
				pp->formulaLines[i] = malloc(n+1);
				memcpy(pp->formulaLines[i], fstart, n);
				pp->formulaLines[i][n] = 0; // terminate string
				fstart = fp+1;
				++i;
				if(i == pp->num_formula)
					break;
			}
		}
	}
	dbi_result_free(res);

	// load some minimum values for width and height, because it could crash
	// the server if 0

	if (pp->d_height < 5) pp->d_height = 20;
	if (pp->d_width < 20) pp->d_width = 72;

	/* aliases */
	res = dbi_conn_queryf( dbiconn, "SELECT * FROM UserCommandAlias "
									"WHERE ownerid = %i", pp->chessduserid );
	if(res){
		char *ua,*ut;
		
		dbi_result_bind_fields(res, "alias.%s targetcommand.%s", &ua, &ut);
		while(dbi_result_next_row(res))
			alias_add(p, ua, ut);
		dbi_result_free(res);
	}

	listaddusers(p, L_CENSOR,  "UserCensor",  pp->chessduserid);
	listaddusers(p, L_NOTIFY,  "UserNotify",  pp->chessduserid);
	listaddusers(p, L_GNOTIFY, "UserGNotify", pp->chessduserid);
	listaddusers(p, L_FOLLOW,  "UserFollow",  pp->chessduserid);
	listaddusers(p, L_NOPLAY,  "UserNoPlay",  pp->chessduserid);
	listaddchannels(p, L_CHANNEL, "UserChannel", pp->chessduserid);

	/* statistics  */
	res = dbi_conn_queryf( dbiconn, "SELECT * FROM UserStats WHERE userid = %i",
						   pp->chessduserid );
	if(res){
		while(dbi_result_next_row(res)){
			gtype = dbi_result_get_int(res,"gametypeid");
			switch(gtype){
			case TYPE_BLITZ:    read_stats_from_db(&pp->b_stats, res); break;
			case TYPE_STAND:    read_stats_from_db(&pp->s_stats, res); break;
			case TYPE_WILD:     read_stats_from_db(&pp->w_stats, res); break;
			case TYPE_LIGHT:    read_stats_from_db(&pp->l_stats, res); break;
			case TYPE_BUGHOUSE: read_stats_from_db(&pp->bug_stats, res); break;
			default: 
				d_printf("bad gametypeid=%d in UserStats table",gtype);
			}
		}
		dbi_result_free(res);
	}
	pp->loading = 0;

	return 0;
}

int player_save(int p, int addplayer)
{
	struct player *pp = &player_globals.parray[p];
	dbi_result res;
	int i, esc_formula_lines_num;
	char *esc_prompt, *esc_lastopponent, *esc_lasttell, *esc_username,
		 *esc_passwd, *esc_formula, *esc_formula_lines, *f_lines_aux;

	if (!CheckPFlag(p, PFLAG_REG)) /* Player not registered */
		return -1;

	/**
	 * samuelm (12 jan 2004):
	 * Fields to be updated in chessduser table:
	 * Other files will be updated via web, we will not save them here.
	 */

	 // compute the 'formula lines' field size; and make sure we don't
	 // get an odd num_formula value around here...
	 if (pp->num_formula < 0 || pp->num_formula > MAX_FORMULA)
		 pp->num_formula = 0;

	 esc_formula_lines_num = 0;
	 for (i=0; i < pp->num_formula; i++)
		 if(pp->formulaLines[i])
		 	esc_formula_lines_num += strlen(pp->formulaLines[i]);
	 d_printf("CHESSD: chars in formula: %d\n", esc_formula_lines_num);

	 // we'll put some \n's, up to the number of lines
	 esc_formula_lines_num += pp->num_formula;

	 if(!pp->prompt || !dbi_conn_quote_string_copy(dbiconn, pp->prompt, &esc_prompt))
		esc_prompt = strdup("NULL");
	 if(!pp->passwd || !dbi_conn_quote_string_copy(dbiconn, pp->passwd, &esc_passwd))
		esc_passwd = strdup("NULL");
	 if(!pp->last_opponent || !dbi_conn_quote_string_copy(dbiconn, pp->last_opponent, &esc_lastopponent))
		esc_lastopponent = strdup("NULL");
	 if(!pp->last_tell || !dbi_conn_quote_string_copy(dbiconn, pp->last_tell, &esc_lasttell))
		esc_lasttell = strdup("NULL");
	 if(!pp->name || !dbi_conn_quote_string_copy(dbiconn, pp->name, &esc_username))
		esc_username = strdup("NULL");
	 if(!pp->formula || !dbi_conn_quote_string_copy(dbiconn, pp->formula, &esc_formula))
		esc_formula = strdup("NULL");

	 // gabrielsan: store the formula lines and escape aggregated string
	 if (esc_formula_lines_num > 0) {
		 d_printf("pp->num_formula = %d\n", pp->num_formula);
		 f_lines_aux = esc_formula_lines = malloc(esc_formula_lines_num);
		 for (i=0; i < pp->num_formula; i++) {
			int len = strlen(pp->formulaLines[i]);
			memcpy(f_lines_aux, pp->formulaLines[i], len);
			f_lines_aux += len;
			*f_lines_aux++ = '\n';
		 }
		f_lines_aux[-1] = 0; // terminate, removing last newline
		if(!dbi_conn_quote_string(dbiconn, &esc_formula_lines)){
			free(esc_formula_lines);
			esc_formula_lines = NULL;
		}
	 }
	 else
		 esc_formula_lines = NULL;

	 dbi_result_free( res = dbi_conn_queryf( dbiconn,
		"UPDATE ChessdUser "
		"SET adminlevel = %i, prompt = %s, passwd = %s, lasthost = '%s', "
			"lastopponent = %s, lasttell = %s, lastchannel = %i, "
			"defaulttime = %i, defaultinc = %i, availmax = %i, availmin = %i, "
			"defaultheight = %i, defaultwidth = %i, highlight = %i, "
			"language = %i, flags = %i, numblack = %i, numwhite = %i, "
			"promote = %i, totaltime = %i, style = %i, formula = %s, "
			"formulalines = %s, numFormulaLines = %i "
		"WHERE UPPER(username) = UPPER(%s)",
	 	pp->adminLevel, esc_prompt, esc_passwd, inet_ntoa(pp->lastHost),
		esc_lastopponent, esc_lasttell, pp->last_channel,
		pp->d_time, pp->d_inc, pp->availmax, pp->availmin,
		pp->d_height, pp->d_width, pp->highlight,
		pp->language, pp->Flags, pp->num_black, pp->num_white,
		pp->promote, pp->totalTime, pp->style, esc_formula,
		esc_formula_lines ? esc_formula_lines : "NULL", pp->num_formula, 
		esc_username ) );

	FREE(esc_prompt);
	FREE(esc_lastopponent);
	FREE(esc_lasttell);
	FREE(esc_username);
	FREE(esc_formula);
	FREE(esc_formula_lines);

	return 0;
}

int player_find(int fd)
{
	int i;

	for (i = 0; i < player_globals.p_num; i++) {
		if ( player_globals.parray[i].status != PLAYER_EMPTY
		  && player_globals.parray[i].socket == fd )
			return i;
	}
	return -1;
}

/* incorrectly named */
int player_find_bylogin(const char *name)
{
	int i;

	for (i = 0; i < player_globals.p_num; i++) {
		if ( player_globals.parray[i].status != PLAYER_EMPTY
		  && player_globals.parray[i].status != PLAYER_LOGIN
		  && player_globals.parray[i].status != PLAYER_PASSWORD
		  && player_globals.parray[i].login
		  && !strcasecmp(player_globals.parray[i].name, name) )
			return i;
	}
	return -1;
}

/* Now incorrectly named */
int player_find_part_login(const char *name)
{
	int i = player_find_bylogin(name);
	if (i >= 0)
		return i;
	for (i = 0; i < player_globals.p_num; i++) {
		if ( player_globals.parray[i].status != PLAYER_EMPTY
		  && player_globals.parray[i].status != PLAYER_LOGIN
		  && player_globals.parray[i].status != PLAYER_PASSWORD
		  && player_globals.parray[i].name
		  && !strncasecmp(player_globals.parray[i].name, name, strlen(name)) )
			return i;
	}
	return -1;
}

/*	gabrielsan:
 *		function created to find online user by ID;
 */
int player_find_byid(int userID)
{
	int i;

	for (i = 0; i < player_globals.p_num; i++) {
		if ( player_globals.parray[i].status != PLAYER_EMPTY 
		  && player_globals.parray[i].status != PLAYER_LOGIN 
		  && player_globals.parray[i].status != PLAYER_PASSWORD 
		  && player_globals.parray[i].login 
		  && player_globals.parray[i].chessduserid == userID )
			return i;
	}
	return -1;
}

int player_censored(int p, int p1)
{
	return in_list(p, L_CENSOR, player_globals.parray[p1].login);
}

/* is p1 on p's notify list? */
int player_notified(int p, int p1)
{
	struct player *pp = &player_globals.parray[p];

	if (!CheckPFlag(p1, PFLAG_REG) && !CheckPFlag(p1, PFLAG_UNDER_EVAL))
		return 0;

	/* possible bug: p has just arrived! */
	if (!pp->name)
		return 0;

	return (in_list(p, L_NOTIFY, player_globals.parray[p1].login));
}

void player_notify_departure(int p)
/* Notify those with notifiedby set on a departure */
{
	struct player *pp = &player_globals.parray[p];
	int p1;
	int invisible;

	if (!CheckPFlag(p, PFLAG_REG) && !CheckPFlag(p, PFLAG_UNDER_EVAL))
		return;

	invisible = CheckPFlag(p, PFLAG_INVISIBLE);

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (!invisible || check_admin2(p1, p)) {
			if (//CheckPFlag(p1, PFLAG_NOTIFYBY) &&
					!player_notified(p1, p)
					&& player_notified(p, p1) &&
					(player_globals.parray[p1].status == PLAYER_PROMPT))
			{
				Bell (p1);
				pprintf(p1, _("\nNotification: "));
				pprintf_highlight(p1, "%s", pp->name);
				pprintf_prompt(p1, _(" has departed and isn't on your notify list.\n"));
			}
		}
	}
}

int player_notify_present(int p)
/* output Your arrival was notified by..... */
/* also notify those with notifiedby set if necessary */
{
	struct player *pp = &player_globals.parray[p];
	int p1, count = 0, invisible;

	if (!CheckPFlag(p, PFLAG_REG) && !CheckPFlag(p, PFLAG_UNDER_EVAL))
		return count;

	invisible = CheckPFlag(p, PFLAG_INVISIBLE);

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (player_notified(p, p1) && player_globals.parray[p1].status == PLAYER_PROMPT) {
			if (!count)
				pprintf(p, _("Present company includes:"));
			count++;
			pprintf(p, " %s", player_globals.parray[p1].name);

			if (!invisible || check_admin2(p1, p)) {
				// p1 will be notified if p is not invisible OR his/her
				// admin level is higher than p's level
				if (//CheckPFlag(p1, PFLAG_NOTIFYBY) &&
					!player_notified(p1, p)
					&& player_globals.parray[p1].status == PLAYER_PROMPT)
				{
					Bell (p1);
					pprintf(p1, _("\nNotification: "));
					pprintf_highlight(p1, "%s", pp->name);
					pprintf_prompt(p1, _(" has arrived and isn't on your notify list.\n"));
				}
			}
		}
	}

	if (count)
		pprintf(p, ".\n");
	return count;
}

/* notify those interested that p has arrived/departed */

int player_notify(int p, char *note1, char *note2)
{
	struct player *pp = &player_globals.parray[p];
	int p1, count = 0, invisible;

	invisible = CheckPFlag(p, PFLAG_INVISIBLE);

	if (!CheckPFlag(p, PFLAG_REG) && !CheckPFlag(p, PFLAG_UNDER_EVAL))
		return count;

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (!invisible || (invisible && check_admin2(p1, p))) {
			if (player_notified(p1, p) &&
				player_globals.parray[p1].status == PLAYER_PROMPT)
			{
				Bell (p1);
				pprintf(p1, _("\nNotification: "));
				pprintf_highlight(p1, "%s", pp->name);
				pprintf_prompt(p1, _(" has %s.\n"), note1);
				if (!count)
					pprintf(p, _("Your %s was noted by:"), note2);
				count++;
				pprintf(p, " %s", player_globals.parray[p1].name);
			}
		}
	}
	if (count)
		pprintf(p, ".\n");
	return count;
}

/*
 * Notify those with "PIN" set that 'p' has arrived
 *
 */
int player_notify_pin(int p)
{
	struct player *pp = &player_globals.parray[p];
	int p1;
	int invisible;

	invisible = CheckPFlag(p, PFLAG_INVISIBLE);

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p || player_globals.parray[p1].status != PLAYER_PROMPT
			|| !CheckPFlag(p1, PFLAG_PIN))
			continue;

		// invisible users' arrivals are only notified to users of higher
		// admin level
		if (!invisible || (invisible && check_admin2(p1, p))) {
			protocol_user_online_status(p1,
					pp->name,
					dotQuad(pp->thisHost),
					player_globals.parray[p1].adminLevel > 0,
					CheckPFlag(p, PFLAG_REG),
					0 // in
					);

			if (player_globals.parray[p1].ivariables.ext_protocol) {
				// send p's user info to p1
				sendUserInfo(p, p1);
			}
		}
	}

	return COM_OK;
}

/* this function may be called to tell the interested people about user
 * information changes */
int player_status_update(int p)
{
	int p1;
	int invisible;

	invisible = CheckPFlag(p, PFLAG_INVISIBLE);

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (p1 == p)
			continue;
		if (player_globals.parray[p1].status != PLAYER_PROMPT)
			continue;
		// only those using the new protocol will be notified
		if (!player_globals.parray[p1].ivariables.ext_protocol)
			continue;

		// invisible users' arrival are only notified to users of higher
		// admin level
		if (!invisible || (invisible && check_admin2(p1, p))) {
			// send p's user info to p1
			sendUserInfo(p, p1);
		}
	}

	return COM_OK;
}

int showstored(int p)
{
	/* gabrielsan:
	 *		function modified to find adjourned games from the database
	 */
	struct player *pp = &player_globals.parray[p];
	multicol *m = multicol_start(50); /* Limit to 50, should be enough*/
	int c, p1, wID, bID, opponentID, userID = pp->chessduserid;
	dbi_result res;

	/*
	 * fill the query values, resulttype (always * adjourn/courtesyadjourn)
	 * and white/black user (the current user id)
	 */
	res = dbi_conn_queryf( dbiconn,
						"SELECT WhitePlayerID, BlackPlayerID FROM Game "
						"WHERE GameResultID IN (%i,%i) "
						      "AND (WhitePlayerID = %i OR BlackPlayerID = %i)",
						END_ADJOURN, END_COURTESYADJOURN, userID, userID );

	if (res){
		/*
		 *	for every adjourned game, check whether the opponent is online
		 */
		dbi_result_bind_int(res, "WhitePlayerID", &wID);
		dbi_result_bind_int(res, "BlackPlayerID", &bID);
		for( c = 0 ; dbi_result_next_row(res) ; ) {
			opponentID = userID == wID ? bID : wID;
	
			// is the user online?
			p1 = player_find_byid(opponentID);
			if (p1 >= 0) {
				// tell the opponent the current user has arrived
				if (c<50)
					multicol_store(m, player_globals.parray[p1].name);
				pprintf(p1, _("\nNotification: "));
				pprintf_highlight(p1,"%s",pp->name);
				pprintf_prompt(p1,
					_(", who has an adjourned game with you, has arrived.\n"));
				c++;
			}
		}
		dbi_result_free(res);
	
		if(c){
			// tell the current user how many adjourned opponents are online
			pprintf(p, ngettext("\n%d player, who has an adjourned game with you, is online:\007",
					"\n%d players, who have adjourned games with you, are online:\007",
					c), c);
			multicol_pprint(m, p, pp->d_width, 2);
		}
		multicol_end(m);
	}
	return COM_OK;
}

int player_count(int CountAdmins)
{
	int count, i;
	
	for (count = i = 0; i < player_globals.p_num; i++) {
		if (player_globals.parray[i].status == PLAYER_PROMPT &&
			(CountAdmins || !check_admin(i, ADMIN_ADMIN)))
			count++;
	}
	if (count > command_globals.player_high)
		command_globals.player_high = count;
	
	return count;
}

int player_idle(int p)
{
	struct player *pp = &player_globals.parray[p];
	return time(0) - (pp->status != PLAYER_PROMPT ? pp->logon_time :
													pp->last_command_time);
}

int player_ontime(int p)
{
	return time(0) - player_globals.parray[p].logon_time;
}

/**
 * Write player login/logout
 */
void log_player_inout (int inout, int p)
{
	struct player *pp = &player_globals.parray[p];

 	if (CheckPFlag(p, PFLAG_REG))
 	{
		dbi_result_free( dbi_conn_queryf( dbiconn,
				"INSERT INTO UserLogon (userid, islogin, eventtime, fromip) "
							   "VALUES (%i, %s, NOW(), '%s')",
						 pp->chessduserid, PUTBOOLEAN(inout == P_LOGIN),
						 inet_ntoa(pp->thisHost) ) );
	}
}

void player_write_login(int p)
{
	log_player_inout(P_LOGIN, p);
	InsertServerEvent( seUSER_COUNT_CHANGE, player_count(1) );

	// gabrielsan: insert the incoming player in the online users table
	ActiveUser(auINSERT, player_globals.parray[p].chessduserid,
	 		   player_globals.parray[p].name, 
	 		   inet_ntoa(player_globals.parray[p].thisHost), 0);
}

void player_write_logout(int p)
{
	log_player_inout(P_LOGOUT, p);
	InsertServerEvent( seUSER_COUNT_CHANGE, player_count(1) - 1 );
	/* we'll subtract the no of players after */

	// remove the outgoing user from the online user list
	ActiveUser(auDELETE, player_globals.parray[p].chessduserid,
			   player_globals.parray[p].login, NULL, 0);
}

int player_is_observe(int p, int g)
{
	struct player *pp = &player_globals.parray[p];
	int i;
	
	for (i = 0; i < pp->num_observe; i++)
		if (pp->observe_list[i] == g)
			break;
	return i != pp->num_observe;
}

int player_add_observe(int p, int g)
{
	struct player *pp = &player_globals.parray[p];
	if (pp->num_observe == MAX_OBSERVE)
		return -1;
	pp->observe_list[pp->num_observe] = g;
	pp->num_observe++;
	return 0;
}

int player_remove_observe(int p, int g)
{
	struct player *pp = &player_globals.parray[p];
	int i;
	
	for (i = 0; i < pp->num_observe; i++)
		if (pp->observe_list[i] == g)
			break;
	if (i == pp->num_observe)
		return -1;			/* Not found! */
	for (; i < pp->num_observe - 1; i++)
		pp->observe_list[i] = pp->observe_list[i + 1];
	pp->num_observe--;
	return 0;
}

int player_game_ended(int g)
{
	struct game *gg = &game_globals.garray[g];
	int p;

	for (p = 0; p < player_globals.p_num; p++) {
		if (player_globals.parray[p].status != PLAYER_EMPTY)
			player_remove_observe(p, g);
	}
	remove_request(gg->white, gg->black, -1);
	remove_request(gg->black, gg->white, -1);
	// player_save(gg->white,0); /* Hawk: Added to save finger-info after each game */
	// player_save(gg->black,0);

	// gabrielsan: update user stats for the type of ended game
	write_stats_to_db(gg->white, gg->type);
	write_stats_to_db(gg->black, gg->type);

	return 0;
}

int player_goto_board(int p, int board_num)
{
	struct game *gg;
	struct player *pp = &player_globals.parray[p];
	int start, count = 0, on, g;
	
	if ( pp->simul_info == NULL
		|| board_num < 0 || board_num >= pp->simul_info->numBoards
		|| pp->simul_info->boards[board_num] < 0 )
		return -1;
	
	pp->simul_info->onBoard = board_num;
	pp->game = pp->simul_info->boards[board_num];
	pp->opponent = game_globals.garray[pp->game].black;
	if (pp->simul_info->numBoards == 1)
		return 0;
	
	send_board_to(pp->game, p);
	start = pp->game;
	on = pp->simul_info->onBoard;
	do {
		g = pp->simul_info->boards[on];
		if (g >= 0) {
			gg = &game_globals.garray[g];
			pprintf(gg->black, "\n");
			pprintf_highlight(gg->black, "%s", pp->name);
			if (count == 0) {
				Bell (gg->black);
				pprintf_prompt(gg->black, _(" is at your board!\n"));
			} else if (count == 1) {
				Bell (gg->black);
				pprintf_prompt(gg->black, _(" will be at your board NEXT!\n"));
			} else {
				pprintf_prompt(gg->black, _(" is %d boards away.\n"), count);
			}
			count++;
		}
		on++;
		if (on >= pp->simul_info->numBoards)
			on = 0;
	} while (start != pp->simul_info->boards[on]);
	return 0;
}

int player_goto_next_board(int p)
{
	struct player *pp = &player_globals.parray[p];
	int on, start, g;
	
	if (pp->simul_info == NULL)
		return -1;
	
	on = pp->simul_info->onBoard;
	start = on;
	g = -1;
	do {
		on++;
		if (on >= pp->simul_info->numBoards)
			on = 0;
		g = pp->simul_info->boards[on];
	} while (g < 0 && start != on);
	if (g == -1) {
		pprintf(p, _("\nMajor Problem! Can't find your next board.\n"));
		return -1;
	}
	return player_goto_board(p, on);
}

int player_goto_prev_board(int p)
{
	struct player *pp = &player_globals.parray[p];
	int on, start, g;
	
	if (pp->simul_info == NULL)
		return -1;
	
	on = pp->simul_info->onBoard;
	start = on;
	g = -1;
	do {
		--on;
		if (on < 0)
			on = (pp->simul_info->numBoards) - 1;
		g = pp->simul_info->boards[on];
	} while (g < 0 && start != on);
	if (g == -1) {
		pprintf(p, _("\nMajor Problem! Can't find your previous board.\n"));
		return -1;
	}
	return player_goto_board(p, on);
}

int player_goto_simulgame_bynum(int p, int num)
{
	struct player *pp = &player_globals.parray[p];
	int on, start, g;
	
	if (pp->simul_info == NULL)
		return -1;
	
	on = pp->simul_info->onBoard;
	start = on;
	do {
		on++;
		if (on >= pp->simul_info->numBoards)
			on = 0;
		g = pp->simul_info->boards[on];
	} while (g != num && start != on);
	if (g != num) {
		pprintf(p, _("\nYou aren't playing that game!!\n"));
		return -1;
	}
	return player_goto_board(p, on);
}

int player_num_active_boards(int p)
{
	struct player *pp = &player_globals.parray[p];
	int count = 0, i;
	
	if (pp->simul_info == NULL || !pp->simul_info->numBoards)
		return 0;
	
	for (i = 0; i < pp->simul_info->numBoards; i++)
		if (pp->simul_info->boards[i] >= 0)
			count++;
	return count;
}

///////////
/* Why was simul removed? - gabrielsan
 *
 */

static int player_num_results(int p, int result)
{
	struct player *pp = &player_globals.parray[p];
	switch (result) {
	case RESULT_WIN:
		return pp->simul_info->num_wins;
	case RESULT_DRAW:
		return pp->simul_info->num_draws;
	case RESULT_LOSS:
		return pp->simul_info->num_losses;
	default:
		return -1;
	}
}

static void new_simul_result(struct simul_info_t *sim, int result)
{
	switch (result) {
	case RESULT_WIN:
		sim->num_wins++;
		return;
	case RESULT_DRAW:
		sim->num_draws++;
		return;
	case RESULT_LOSS:
		sim->num_losses++;
		return;
	}
}

int player_simul_over(int p, int g, int result)
{
	struct player *pp = &player_globals.parray[p];
	int on, ong, p1, which;
	char tmp[1024];

	if (pp->simul_info == NULL)
		return -1;

	for (which = 0; which < pp->simul_info->numBoards; which++) {
		if (pp->simul_info->boards[which] == g)
			break;
	}
	if (which == pp->simul_info->numBoards) {
		pprintf(p, _("I can't find that game!\n"));
		return -1;
	}
	pprintf(p, _("\nBoard %d has completed.\n"), which + 1);
	on = pp->simul_info->onBoard;
	ong = pp->simul_info->boards[on];
	pp->simul_info->boards[which] = -1;
	new_simul_result(pp->simul_info, result);
	if (player_num_active_boards(p) == 0) {
		sprintf(tmp, _("\n{Simul (%s vs. %d) is over.}\n"
					   "Results: %d Wins, %d Losses, %d Draws, %d Aborts\n"),
				pp->name,
				pp->simul_info->numBoards,
				player_num_results(p, RESULT_WIN),
				player_num_results(p, RESULT_LOSS),
				player_num_results(p, RESULT_DRAW),
				player_num_results(p, RESULT_ABORT));
		for (p1 = 0; p1 < player_globals.p_num; p1++) {
			if (pp->status != PLAYER_PROMPT)
				continue;
			if (!CheckPFlag(p1, PFLAG_GIN) && !player_is_observe(p1, g) && (p1 != p))
				continue;
			pprintf_prompt(p1, "%s", tmp);
		}
		pp->simul_info->numBoards = 0;
		pprintf_prompt(p, _("\nThat was the last board, thanks for playing.\n"));
		free(pp->simul_info);
		pp->simul_info = NULL;
		return 0;
	}
	if (ong == g)		/* This game is over */
		player_goto_next_board(p);
	else
		player_goto_board(p, pp->simul_info->onBoard);

	pprintf_prompt(p, _("\nThere are %d boards left.\n"), player_num_active_boards(p));
	return 0;
}

/////////// Simul Functions end

void SaveTextListEntry(textlist **Entry, char *string, int n)
{
	*Entry = (textlist *) malloc(sizeof(textlist));
	(*Entry)->text = strdup(string);
	(*Entry)->index = n;
	(*Entry)->next = NULL;
}

static textlist *ClearTextListEntry(textlist *entry)
{
	textlist *ret = entry->next;
	free(entry->text);
	free(entry);
	return ret;
}

void ClearTextList(textlist *head)
{
	textlist *cur;
	
	for (cur = head; cur != NULL; cur = ClearTextListEntry(cur))
		;
}

int player_search(int p, char *name)
/*
 * Find player matching the given string. First looks for exact match
 *  with a logged in player, then an exact match with a registered player,
 *  then a partial unique match with a logged in player, then a partial
 *  match with a registered player.
 *  Returns player number if the player is connected, negative (player number)
 *  if the player had to be connected, and 0 if no player was found
 */
{
	int p1, count = 0;
	const char *username_found = NULL;
	char *esc_name;
	dbi_result res;

	/* exact match with connected player? */
	if ((p1 = player_find_bylogin(name)) >= 0)
		return p1 + 1;

	/* exact match with registered player? */
	dbi_conn_quote_string_copy(dbiconn, name, &esc_name);
	res = dbi_conn_queryf( dbiconn, "SELECT username FROM ChessdUser "
  									"WHERE UPPER(username) = UPPER(%s)",
  						   esc_name );
	if (res){
		count = dbi_result_get_numrows(res);
		if(dbi_result_next_row(res))
			username_found = dbi_result_get_string_idx(res,1);
		else
			count = 0;
	}
	FREE(esc_name);

	if (username_found /* && !strcmp(name, *buffer) */){
		/* found an unconnected registered player */
		p1 = player_new();
		if (player_read(p1, username_found)) {
			player_remove(p1);
			d_printf(_("ERROR: a player named %s was expected but not found\n"),
					 username_found);
			pprintf(p, _("ERROR: a player named %s was expected but not found!\n"
	    				 "Please tell an admin about this incident. Thank you.\n"),
					username_found);
			if(res) dbi_result_free(res);
			return 0;
		}
		if(res) dbi_result_free(res);
		return (-p1) - 1; /* negative to indicate player was not connected */
	}
	else if ((p1 = player_find_part_login(name)) >= 0) /* partial match with connected player? */
		return p1 + 1;
	else if (p1 == -2) /* ambiguous; matches too many connected players. */
		pprintf (p, _("Ambiguous name '%s'; matches more than one player.\n"), name);
	else if (count < 1) /* partial match with registered player? */
		pprintf(p, _("There is no player matching that name.\n"));
	else if (count > 1)
		pprintf(p, _("-- Matches: %d names --"), count);

    return 0;
}

/* returns 1 if player is head admin, 0 otherwise */
int player_ishead(int p)
{
	return !player_globals.parray[p].name 
		|| !strcasecmp(player_globals.parray[p].name, config.strs[head_admin]);
}

/* GetRating chooses between blitz, standard and other ratings. */
int GetRating(struct player *p, int gametype)
{
    switch(gametype){
    case TYPE_BLITZ:	return (p->b_stats.rating);
    case TYPE_STAND:	return (p->s_stats.rating);
    case TYPE_WILD:		return (p->w_stats.rating);
    case TYPE_LIGHT:	return (p->l_stats.rating);
    case TYPE_BUGHOUSE:	return (p->bug_stats.rating);
    }
    return 0;
}    /* end of function GetRating. */

/* GetRD chooses between blitz, standard and other RD's. */
double GetRD(struct player *p, int gametype)
{
	struct statistics *s;

	switch(gametype) {
    case TYPE_BLITZ:	s = &p->b_stats; break;
    case TYPE_STAND:	s = &p->s_stats; break;
    case TYPE_WILD:		s = &p->w_stats; break;
    case TYPE_LIGHT:	s = &p->l_stats; break;
    case TYPE_BUGHOUSE:	s = &p->bug_stats; break;
    default: return 0.0;
  }
  // BUG: due to default above, we never reach this return statement??
  return current_sterr(s->sterr, time(NULL) - s->ltime);
}    /* end of function GetRD. */

/**
 * getChessdUserID (char *name)
 * Return ChessdUserID for username *name
 * If there is no user with username *name, returns -1
 */
int getChessdUserID(char *name)
{
	return getChessdUserFieldInt(name, "chessduserid");
}

int getChessdUserFieldInt(char *name, char *field)
{
	int ret;
	char *esc;
	dbi_result res;

	dbi_conn_quote_string_copy(dbiconn, name, &esc);
	res = dbi_conn_queryf( dbiconn, "SELECT %s FROM ChessdUser "
									"WHERE UPPER(username) = UPPER(%s)", 
						   field, esc );
	ret = res && dbi_result_next_row(res) ? dbi_result_get_int_idx(res,1) : -1;

	if(res) dbi_result_free(res);
	FREE(esc);
	
	return ret;
}

char* getChessdUserField(char *name, char* field)
{
	char *esc, *ret;
	dbi_result res;

	dbi_conn_quote_string_copy(dbiconn, name, &esc);
	res = dbi_conn_queryf( dbiconn, "SELECT %s FROM ChessdUser "
									"WHERE UPPER(username) = UPPER(%s)",
						   field, esc );
	if (res && dbi_result_next_row(res))
		ret = dbi_result_get_string_copy_idx(res,1);
	else
		ret = NULL;

	if(res) dbi_result_free(res);
	FREE(esc);

	return ret;
}

int get_player_name_byid(int playerID, char **player_name)
{
	int ret;
	dbi_result res = dbi_conn_queryf( dbiconn,
		"SELECT username FROM ChessdUser WHERE chessduserid = %i", playerID );

	if (res && dbi_result_next_row(res)){
		*player_name = dbi_result_get_string_copy_idx(res,1);
		ret = 0;
	}else
		ret = -1;
		
	if(res) dbi_result_free(res);
	
	return ret;
}

/**
 * samuelm: Only show the URL of the user's info on
 * 		    the web interface.
 */
int com_finger (int p, param_list param)
{
	pprintf(p, "\n\n");
	pprintf(p, _("This information now is available at %s/users/%s"),
			config.strs[webinterface_url], param[0].val.string);
	pprintf(p, "\n\n");
	return COM_OK;
}

void AddPlayerLists (int p1, char *ptmp)
{
	// admin level based tag
	if (check_admin(p1, ADMIN_ADMIN) && CheckPFlag(p1, PFLAG_ADMINLIGHT))
		strcat(ptmp, "(*)");
	if (check_admin_level(p1, ADMIN_HELPER))
		strcat(ptmp, "(H)");
	if (check_admin_level(p1, ADMIN_ADJUDICATOR))
		strcat(ptmp, "(AD)");

	// tag based on lists
	if (in_list(p1, L_COMPUTER, player_globals.parray[p1].name))
		strcat(ptmp, "(C)");
	if (in_list(p1, L_FM, player_globals.parray[p1].name))
		strcat(ptmp, "(FM)");
	if (in_list(p1, L_IM, player_globals.parray[p1].name))
		strcat(ptmp, "(IM)");
	if (in_list(p1, L_GM, player_globals.parray[p1].name))
		strcat(ptmp, "(GM)");
	if (in_list(p1, L_WGM, player_globals.parray[p1].name))
		strcat(ptmp, "(WGM)");
	if (in_list(p1, L_TD, player_globals.parray[p1].name))
		strcat(ptmp, "(TD)");
	if (in_list(p1, L_TEAMS, player_globals.parray[p1].name))
		strcat(ptmp, "(T)");
	if (in_list(p1, L_BLIND, player_globals.parray[p1].name))
		strcat(ptmp, "(B)");
	if (in_list(p1, L_MANAGER, player_globals.parray[p1].name))
		strcat(ptmp, "(TM)");
	if (in_list(p1, L_TEACHER, player_globals.parray[p1].name))
		strcat(ptmp, "(Te)");
}

int sendUserInfo(int whoseInfo, int toWhom)
{
	static char whoseInfoAttrs[255];
	struct statistics user_stats[5];
	int i;

	user_stats[RATE_STAND] 		= player_globals.parray[whoseInfo].s_stats;
	user_stats[RATE_BLITZ] 		= player_globals.parray[whoseInfo].b_stats;
	user_stats[RATE_WILD]		= player_globals.parray[whoseInfo].w_stats;
	user_stats[RATE_LIGHT] 		= player_globals.parray[whoseInfo].l_stats;
	user_stats[RATE_BUGHOUSE] 	= player_globals.parray[whoseInfo].bug_stats;

	for (i=0; i < NUM_RATEDTYPE; i++) {
		// no need to copy as the user_stats array will be gone for good
		user_stats[i].style_name = (char*)ratedStyles[i];
	}

	strcpy(whoseInfoAttrs, "");
    AddPlayerLists(whoseInfo, whoseInfoAttrs);

	// send p1's info to the requester
	protocol_send_user_info(
			toWhom,
			player_globals.parray[whoseInfo].name,	// username
			CheckPFlag(whoseInfo, PFLAG_REG),		// is registered
			CheckPFlag(whoseInfo, PFLAG_OPEN),		// is open for match requests
			CheckPFlag(whoseInfo, PFLAG_RATED),		// is playing rated games
			player_globals.parray[whoseInfo].game, // playing in which game?

			// stat information
			user_stats, NUM_RATEDTYPE, // user stats array and array size

			whoseInfoAttrs, // special attributes (earned from lists)
			player_ontime(whoseInfo),				// online time
			player_idle(whoseInfo)					// idle time
			);

	return 0;
}
