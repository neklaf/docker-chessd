/*
   Copyright (c) 1993 Richard V. Nash.
   Copyright (c) 2000 Dan Papasian
   Copyright (c) Andrew Tridgell 2002
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
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "comproc.h"
#include "config_genstrc.h"
#include "adminproc.h"
#include "crypt.h"
#include "malloc.h"
#include "utils.h"
#include "chessd_main.h"
#include "globals.h"
#include "playerdb.h"
#include "gamedb.h"
#include "chessd_locale.h"
#include "configuration.h"
#include "variable.h"
#include "ratings.h"
#include "lists.h"
#include "multicol.h"
#include "talkproc.h"
#include "obsproc.h"
#include "network.h"
#include "command_network.h"
#include "dbdbi.h"

// includes the protocol functions
#include "protocol.h"

enum{
	none, blitz_rat, std_rat, wild_rat, light_rat, bug_rat
};

int com_more(int p, param_list param)
{
	pmore_text(p);
	return COM_OK;
}

int com_quit(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];

	/* Examined games are killed on logout */
	if (pp->game >= 0 &&
	    (game_globals.garray[pp->game].status != GAME_EXAMINE &&
	     game_globals.garray[pp->game].status != GAME_SETUP))
	{
		pprintf(p, _("You can't quit while you are playing a game.\n"
					 "Type 'resign' to resign the game, or request an abort with 'abort'.\n"));
		return COM_OK;
	}

	psend_logoutfile(p, MESS_DIR, MESS_LOGOUT);
	return COM_LOGOUT;
}

int com_set(int p, param_list param)
{
	int result, which;
	char *val;

	val = param[1].type == TYPE_NULL ? NULL : param[1].val.string;
	result = var_set(p, param[0].val.word, val, &which);
	switch (result) {
	case VAR_OK:
		// notify users interested in user info change
		player_status_update(p);
		break;
	case VAR_BADVAL:
		pprintf(p, _("Bad value given for variable %s.\n"), param[0].val.word);
		break;
	case VAR_NOSUCH:
		pprintf(p, _("No such variable name %s.\n"), param[0].val.word);
		break;
	case VAR_AMBIGUOUS:
		pprintf(p, _("Ambiguous variable name %s.\n"), param[0].val.word);
		break;
	}

	return COM_OK;
}

int FindPlayer(int p, char* name, int *p1, int *connected)
{
	*p1 = player_search(p, name);
	if (*p1 == 0)
		return 0;
	if (*p1 < 0) {  /* player had to be connected and will be removed later */
		*connected = 0;
		*p1 = (-*p1) - 1;
	} else {
		*connected = 1;
		*p1 = *p1 - 1;
	}
	return 1;
}

int com_password(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	char *oldpassword = param[0].val.word, *newpassword = param[1].val.word;
	char salt[3];

	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p, _("Setting a password is only for registered players.\n"));
		return COM_OK;
	}
	if (pp->passwd) {
		if (!matchpass(pp->passwd, oldpassword)) {
			pprintf(p, _("Incorrect password, password not changed!\n"));
			return COM_OK;
		}
		free(pp->passwd);
		pp->passwd = NULL;
	}
	salt[0] = 'a' + random() % 26;
	salt[1] = 'a' + random() % 26;
	salt[2] = '\0';
	pp->passwd = strdup(chessd_crypt(newpassword,salt));
	pprintf(p, _("Password changed to \"%s\".\n"), newpassword);
	return COM_OK;
}

int com_uptime(int p, param_list param)
{
	time_t uptime = time(0) - command_globals.startuptime;
	int days  = (uptime / (60*60*24));
	int hours = ((uptime % (60*60*24)) / (60*60));
	int mins  = (((uptime % (60*60*24)) % (60*60)) / 60);
	int secs  = (((uptime % (60*60*24)) % (60*60)) % 60);

	pprintf(p, _("Server location: %s\n"), config.strs[server_hostname] );
	pprintf(p, _("The server has been up since %s.\n"), strltime(&command_globals.startuptime));
	pprintf(p, _("Up for"));

	if(days)  pprintf(p,ngettext(" %d day", " %d days", days), days);
	if(hours) pprintf(p,ngettext(" %d hour", " %d hours", hours), hours);
	if(mins)  pprintf(p,ngettext(" %d minute", " %d minutes", mins), mins);
	if(secs)  pprintf(p,ngettext(" %d second", " %d seconds", secs), secs);

	pprintf(p, ".\n");
	pprintf(p, _("\nPlayer limit: %d\n"), config.ints[max_players]);
	pprintf(p, _("\nThere are currently %d players, with a high of %d since last restart.\n"), 
			player_count(1), command_globals.player_high);
	pprintf(p, _("There are currently %d games, with a high of %d since last restart.\n"), 
			game_count(), command_globals.game_high);
	return COM_OK;
}

int com_date(int p, param_list param)
{
	time_t t = time(0);
	pprintf(p, _("Local time     - %s\n"), strltime(&t));
	pprintf(p, _("Greenwich time - %s\n"), strgtime(&t));
	return COM_OK;
}

static const char *inout_string[] = {
  "login", "logout"
};

static int plogins(int p, int limit, char *username)
{
	/*
	 *	gabrielsan: (04/02/2004)
	 *		the login information is now recovered from the database
	 */

	/* Wed May 25 09:17:48 BRT 2005
	 *
	 * We use a subquery to get the real information and the query to
	 * format the date; In this way, we avoid the user of date_part
	 * on every login and the query is performed a lot faster.
	 */
	const char *sql =
		" SELECT "
		"	userid, "
		" 	islogin, "
		"	DATE_PART('EPOCH', eventtime) AS eventtime, "
		"	username, "
		"	fromip "
		" FROM ( "
		"    SELECT "
		"    	userid, "
		"    	islogin, "
		"    	eventtime, "
		"    	C.username, "
		"    	fromip "
		"    FROM "
		"    	UserLogon "
		"    	INNER JOIN ChessdUser C "
		"    	ON C.chessduserid = userid "
		"    %s "
		"    ORDER BY "
		"    	eventtime DESC "
		"    LIMIT %d "
		") as t1";

	char where[256];
	int userid;
	time_t thetime;
	char *loginName, *fromip, *islogin;
	dbi_result res;

	if (username == NULL)
		strcpy(where, "");
	else { // get userid
		int uid = getChessdUserID(username);
		sprintf(where, "WHERE userid=%i", uid);
	}

	if ( (res = dbi_conn_queryf(dbiconn, sql, where, limit))
		 && dbi_result_get_numrows(res) )
	{
		dbi_result_bind_fields(res,
			"userid.%i islogin.%s username.%s eventtime.%m fromip.%s",
			&userid,  &islogin,  &loginName, &thetime,    &fromip);
		while(dbi_result_next_row(res)){
			// gabrielsan: admins will be able to see user ip in the logon
			// command family
			if (!check_admin(p, ADMIN_HELPER))
				fromip = "";
			pprintf(p, "%s: %-17s %-6s\t %s\n", strltime(&thetime),
					loginName, inout_string[GETBOOLEAN(islogin)], fromip);
		}
	}else
		pprintf(p, _("Sorry, no login information available.\n"));

	if(res) dbi_result_free(res);
	return COM_OK;
}

int com_llogons(int p, param_list param)
{
	/*	gabrielsan: (04/02/2004)
	 *		now recover login information from the database
	 */
	if (!CheckPFlag(p, PFLAG_REG)) {
		pprintf(p,_("Sorry, guest users may not use this command\n"));
		return COM_OK;
	}
	return plogins(p, 30, NULL);
}

int com_logons(int p, param_list param)
{
	/*
	 *	gabrielsan: (04/02/2004)
	 *		now recover login information from the database
	 */
	int uid;
	struct player *pp = &player_globals.parray[p];
	char username[MAX_LOGIN_NAME];

	if (param[0].type == TYPE_WORD) {
		strcpy(username, param[0].val.word);
		uid = getChessdUserID(username);
		if (uid == -1) {
			pprintf(p, _("User '%s' does not exist.\n"), username);
			return COM_OK;
		}
	} else
		strcpy(username, pp->login);

	return plogins(p, 10, username);
}

#define WHO_OPEN		 0x01
#define WHO_CLOSED		 0x02
#define WHO_RATED		 0x04
#define WHO_UNRATED		 0x08
#define WHO_FREE		 0x10
#define WHO_PLAYING		 0x20
#define WHO_REGISTERED	 0x40
#define WHO_UNREGISTERED 0x80
#define WHO_BUGTEAM		0x100

#define WHO_ALL			 0xff

static void who_terse(int p, int num, int *plist, int type)
{
	struct player *pp = &player_globals.parray[p], *pp1;
	char ptmp[80 + 20];		/* for highlight */
	multicol *m = multicol_start(player_globals.parray_size);
	int i, p1, rat = 0;

	/* altered DAV 3/15/95 */

	for (i = 0; i < num; i++) {
		p1 = plist[i];
		pp1 = &player_globals.parray[p1];

		switch(type){
		case blitz_rat:	rat = pp1->b_stats.rating; break;
		case wild_rat:	rat = pp1->w_stats.rating; break;
		case std_rat:	rat = pp1->s_stats.rating; break;
		case light_rat:	rat = pp1->l_stats.rating; break;
		case bug_rat:	rat = pp1->bug_stats.rating; break;
		}

		if (type == none)
			strcpy(ptmp, "     ");
		else {
			sprintf(ptmp, "%-4s", ratstrii(rat, p1));
			if (pp1->simul_info && pp1->simul_info->numBoards)
				strcat(ptmp, "~");
			else if ( pp1->game >= 0 &&
					  (game_globals.garray[pp1->game].status == GAME_EXAMINE ||
					   game_globals.garray[pp1->game].status == GAME_SETUP) )
				strcat(ptmp, "#");
			else if (pp1->game >= 0)
				strcat(ptmp, "^");
			else if (!CheckPFlag(p1, PFLAG_OPEN))
				strcat(ptmp, ":");
			else if (player_idle(p1) > 300)
				strcat(ptmp, ".");
			else
				strcat(ptmp, " ");
		}

		if (p == p1)
			psprintf_highlight(p, ptmp + strlen(ptmp), "%s", pp1->name);
		else
			strcat(ptmp, pp1->name);

		AddPlayerLists(p1, ptmp);
		multicol_store(m, ptmp);
	}
	multicol_pprint(m, p, pp->d_width, 2);
	multicol_end(m);
	pprintf(p, _("\n %d players displayed (of %d). (*) indicates system administrator.\n"),
			num, player_count(1));
}

static void who_verbose(int p, int num, int plist[])
{
	struct player *pp1;
  int i, p1;
  char playerLine[255], tmp[255];	/* +8 for highlight */
  char p1WithAttrs[255];

  pprintf(p,
		  " +---------------------------------------------------------------+\n"
		 );
  pprintf(p,
		_(" |      User              Light       Blitz        On for   Idle |\n")
		 );
  pprintf(p,
		  " +---------------------------------------------------------------+\n"
		 );

  for (i = 0; i < num; i++) {
    p1 = plist[i];
    pp1 = &player_globals.parray[p1];

    strcpy(playerLine, " |");

    if (pp1->game >= 0)
      sprintf(tmp, "%3d", pp1->game + 1);
    else
      strcpy(tmp, "   ");
    strcat(playerLine, tmp);

    strcpy(tmp, CheckPFlag(p1, PFLAG_OPEN) ? " " : "X");
    strcat(playerLine, tmp);

    if (CheckPFlag(p1, PFLAG_REG))
		strcpy(tmp, CheckPFlag(p1, PFLAG_RATED) ? " " : "u");
    else
		strcpy(tmp, "U");
    strcat(playerLine, tmp);

    /* Modified by hersco to include lists in 'who v.' */
    strcpy (p1WithAttrs, pp1->name);
    AddPlayerLists(p1, p1WithAttrs);
    p1WithAttrs[17] = '\0';

    /* Modified by DAV 3/15/95 */
    if (p == p1) {
      strcpy(tmp, " ");
      psprintf_highlight(p, tmp + strlen(tmp), "%-17s", p1WithAttrs);
    } else {
      sprintf(tmp, " %-17s", p1WithAttrs);
    }
    strcat(playerLine, tmp);

    sprintf(tmp, " %4s        %-4s        %5s  ",
	    ratstrii(pp1->l_stats.rating, p1),
	    ratstrii(pp1->b_stats.rating, p1),
	    hms(player_ontime(p1), 0, 0, 0));
    strcat(playerLine, tmp);

    if (player_idle(p1) >= 60)
      sprintf(tmp, "%5s   |\n", hms(player_idle(p1), 0, 0, 0));
    else
      strcpy(tmp, "        |\n");
    strcat(playerLine, tmp);

    pprintf(p, "%s", playerLine);
  }

  pprintf(p,
		  " |                                                               |\n"
		 );
  pprintf(p,
		_(" |    %3d Players Displayed                                      |\n"),
		  num );
  pprintf(p,
		  " +---------------------------------------------------------------+\n"
		 );
}

static void who_fullinfo(int p, int num, int plist[])
{
  int i, p1;

  // this call is used to tell the client that a user list will be sent
  // p: the requesting user   num: the list size
  protocol_init_userlist(p, num);

  for (i = 0; i < num; i++) {
    p1 = plist[i];

	// send p1's info to p
	sendUserInfo(p1, p);
  }
  protocol_end_userlist(p);
}

static void who_winloss(int p, int num, int plist[])
{
	struct player *pp1;
  int i, p1;
  char playerLine[255], tmp[255];	/* for highlight */
  char p1WithAttrs[255];

  pprintf(p,
		_("Name               Light     win loss draw   Blitz    win loss draw    idle\n")
		 );
  pprintf(p,
		  "----------------   -----     -------------   -----    -------------    ----\n"
		 );

  for (i = 0; i < num; i++) {
    playerLine[0] = '\0';
    p1 = plist[i];
	pp1 = &player_globals.parray[p1];

    /* Modified by hersco to include lists in 'who n.' */
    strcpy(p1WithAttrs, pp1->name);
    AddPlayerLists(p1, p1WithAttrs);
    p1WithAttrs[17] = '\0';

    if (p1 == p)
      psprintf_highlight(p, playerLine, "%-17s", p1WithAttrs);
    else
      sprintf(playerLine, "%-17s", p1WithAttrs);

    sprintf(tmp, "  %4s     %4d %4d %4d   ",
			ratstrii(pp1->l_stats.rating, p1),
			pp1->l_stats.win, pp1->l_stats.los, pp1->l_stats.dra);
    strcat(playerLine, tmp);

    sprintf(tmp, "%4s    %4d %4d %4d   ",
			ratstrii(pp1->b_stats.rating, p1),
			pp1->b_stats.win, pp1->b_stats.los, pp1->b_stats.dra);
    strcat(playerLine, tmp);

    if (player_idle(p1) >= 60)
      sprintf(tmp, "%5s\n", hms(player_idle(p1), 0, 0, 0));
    else
      strcpy(tmp, "     \n");

    strcat(playerLine, tmp);

    pprintf(p, "%s", playerLine);
  }
  pprintf(p, _("    %3d Players Displayed.\n"), num);
}

static int who_ok(int p, unsigned int sel_bits)
{
	struct player *pp = &player_globals.parray[p];
	int p2;

	if (pp->status != PLAYER_PROMPT || CheckPFlag(p, PFLAG_INVISIBLE)) // hide a user from the who-list
		return 0;

	if (sel_bits == WHO_ALL)
		return 1;

	if ( ((sel_bits & WHO_OPEN)         && (!CheckPFlag(p, PFLAG_OPEN) || CheckPFlag(p, PFLAG_TOURNEY)))
	  || ((sel_bits & WHO_CLOSED)       && CheckPFlag(p, PFLAG_OPEN))
	  || ((sel_bits & WHO_RATED)        && !CheckPFlag(p, PFLAG_RATED))
	  || ((sel_bits & WHO_UNRATED)      && CheckPFlag(p, PFLAG_RATED))
	  || ((sel_bits & WHO_FREE)         && pp->game >= 0)
	  || ((sel_bits & WHO_PLAYING)      && pp->game < 0)
	  || ((sel_bits & WHO_REGISTERED)   && !CheckPFlag(p, PFLAG_REG))
	  || ((sel_bits & WHO_UNREGISTERED) && CheckPFlag(p, PFLAG_REG)) )
		return 0;
	if (sel_bits & WHO_BUGTEAM) {
		p2 = pp->partner;
		if (p2 < 0 || player_globals.parray[p2].partner != p)
			return 0;
	}
	return 1;
}


static int blitz_cmp(const void *pp1, const void *pp2)
{
  register int p1 = *(int*)pp1;
  register int p2 = *(int*)pp2;
  if (player_globals.parray[p1].status != PLAYER_PROMPT)
    return player_globals.parray[p2].status != PLAYER_PROMPT ? 0 : -1;
  if (player_globals.parray[p2].status != PLAYER_PROMPT)
    return 1;
  if (player_globals.parray[p1].b_stats.rating > player_globals.parray[p2].b_stats.rating)
    return -1;
  if (player_globals.parray[p1].b_stats.rating < player_globals.parray[p2].b_stats.rating)
    return 1;
  if (CheckPFlag(p1, PFLAG_REG) && !CheckPFlag(p2, PFLAG_REG))
    return -1;
  if (!CheckPFlag(p1, PFLAG_REG) && CheckPFlag(p2, PFLAG_REG))
    return 1;
  return strcmp(player_globals.parray[p1].login, player_globals.parray[p2].login);
}

static int compare_stats(int p1, int p2, struct statistics s_p1, struct statistics s_p2)
{
	if (player_globals.parray[p1].status != PLAYER_PROMPT)
		return player_globals.parray[p2].status != PLAYER_PROMPT ? 0 : -1;

	if (player_globals.parray[p2].status != PLAYER_PROMPT)
		return 1;

	if (s_p1.rating > s_p2.rating)
		return -1;
	if (s_p1.rating < s_p2.rating)
		return 1;

	return strcmp(player_globals.parray[p1].login, player_globals.parray[p2].login);
}

static int light_cmp(const void *pp1, const void *pp2)
{
	register int p1 = *(int*)pp1, p2 = *(int*)pp2;
	return compare_stats(p1, p2, player_globals.parray[p1].l_stats,
								 player_globals.parray[p2].l_stats);
}

static int bug_cmp(const void *pp1, const void *pp2)
{
	register int p1 = *(int*)pp1, p2 = *(int*)pp2;
	return compare_stats(p1, p2, player_globals.parray[p1].bug_stats,
								 player_globals.parray[p2].bug_stats);
}

static int stand_cmp(const void *pp1, const void *pp2)
{
	register int p1 = *(int*)pp1, p2 = *(int*)pp2;
	return compare_stats(p1, p2, player_globals.parray[p1].s_stats,
								 player_globals.parray[p2].s_stats);
}

static int wild_cmp(const void *pp1, const void *pp2)
{
	register int p1 = *(int*)pp1, p2 = *(int*)pp2;
	return compare_stats(p1, p2, player_globals.parray[p1].w_stats,
								 player_globals.parray[p2].w_stats);
}

static int alpha_cmp(const void *pp1, const void *pp2)
{
	register int p1 = *(int*)pp1, p2 = *(int*)pp2;

	if (player_globals.parray[p1].status != PLAYER_PROMPT)
		return player_globals.parray[p2].status != PLAYER_PROMPT ? 0 : -1;
	if (player_globals.parray[p2].status != PLAYER_PROMPT)
		return 1;
	return strcmp(player_globals.parray[p1].login, 
				  player_globals.parray[p2].login);
}

static void sort_players(int *players, int ((*cmp_func) (const void *, const void *)))
{
	int i;

	for (i = 0; i < player_globals.p_num; i++)
		players[i] = i;

	qsort(players, player_globals.p_num, sizeof(int), cmp_func);
}

int com_admins(int p, param_list param)
{
	// display admin list and how long have they been idle
	char admin_name[50];
	int p1;

	pprintf(p, "  %-50s %s\n", _("Administrator Name"), _("Idle Time"));
	pprintf(p, "  %-50s %s\n",   "------------------",    "---------");

	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if ( player_globals.parray[p1].status == PLAYER_PROMPT
			 && check_admin(p1, ADMIN_ADMIN) )
		{
			snprintf(admin_name, 50, "%-50s", player_globals.parray[p1].login);
			pprintf(p, "  %s %s\n", admin_name, hms_desc(player_idle(p1)));
		}
	}

	return COM_OK;
}

/* This is the of the most complex commands in terms of parameters */
int com_who(int p, param_list param)
{
	int ((*cmp_func) (const void *, const void *));
	struct player *pp = &player_globals.parray[p];
	float stop_perc = 1.0;
	float start_perc = 0;
	unsigned int sel_bits = WHO_ALL;
	int *sortlist, *plist;
	int style = 0, startpoint, stoppoint, i, len, tmpI, tmpJ,
		p1, count, num_who, sort_type, total;
	char c;

	sortlist = malloc(sizeof(int) * player_globals.p_num);
	plist = malloc(sizeof(int) * player_globals.p_num);

	total = pp->d_time * 60 + pp->d_inc * 40;
	if (total < 180) {
		sort_type = light_rat;
		cmp_func = light_cmp;
	} else if (total >= 900) {
		sort_type = std_rat;
		cmp_func = stand_cmp;
	} else {
		sort_type = blitz_rat;
		cmp_func = blitz_cmp;
	}

	if (param[0].type != TYPE_NULL) {
		len = strlen(param[0].val.string);
		for (i = 0; i < len; i++) {
			c = param[0].val.string[i];
			if (isdigit(c)) {
				if (i == 0 || !isdigit(param[0].val.string[i - 1])) {
					tmpI = c - '0';
					if (tmpI == 1) {
						start_perc = 0.0;
						stop_perc = 0.333333;
					} else if (tmpI == 2) {
						start_perc = 0.333333;
						stop_perc = 0.6666667;
					} else if (tmpI == 3) {
						start_perc = 0.6666667;
						stop_perc = 1.0;
					} else if ((i == len - 1) || (!isdigit(param[0].val.string[i + 1])))
						goto bad_parameters;
				} else {
					tmpI = c - '0';
					tmpJ = param[0].val.string[i - 1] - '0';
					if (tmpI == 0 || tmpJ > tmpI)
						goto bad_parameters;
					start_perc = ((float) tmpJ - 1.0) / (float) tmpI;
					stop_perc = ((float) tmpJ) / (float) tmpI;
				}
			} else {
				switch (c) {
				case ' ':
				case '\n':
				case '\t':
					break;
				case 'o':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_OPEN;
					else
						sel_bits |= WHO_OPEN;
					break;
				case 'r':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_RATED;
					else
						sel_bits |= WHO_RATED;
					break;
				case 'f':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_FREE;
					else
						sel_bits |= WHO_FREE;
					break;
				case 'a':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_FREE | WHO_OPEN;
					else
						sel_bits |= (WHO_FREE | WHO_OPEN);
					break;
				case 'R':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_REGISTERED;
					else
						sel_bits |= WHO_REGISTERED;
					break;
				case 'l':		/* Sort order */
					cmp_func = alpha_cmp;
					sort_type = none;
					break;
				case 'A':		/* Sort order */
					cmp_func = alpha_cmp;
					break;
				case 'w':		/* Sort order */
					cmp_func = wild_cmp;
					sort_type = wild_rat;
					break;
				case 's':		/* Sort order */
					cmp_func = stand_cmp;
					sort_type = std_rat;
					break;
				case 'b':		/* Sort order */
					cmp_func = blitz_cmp;
					sort_type = blitz_rat;
					break;
				case 'L':               /* Sort order */
					cmp_func = light_cmp;
					sort_type = light_rat;
					break;
				case 't':		/* format */
					style = 0;
					break;
				case 'v':		/* format */
					style = 1;
					break;
				case 'n':		/* format */
					style = 2;
					break;
				case 'z':
					if (pp->ivariables.ext_protocol)
						style = 3;
					else
						goto bad_parameters;
					break;
				case 'U':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_UNREGISTERED;
					else
						sel_bits |= WHO_UNREGISTERED;
					break;
				case 'B':
					if (sel_bits == WHO_ALL)
						sel_bits = WHO_BUGTEAM;
					else
						sel_bits |= WHO_BUGTEAM;
					cmp_func = bug_cmp;
					sort_type = bug_rat;
					break;
				default:
					goto bad_parameters;
				}
			}
		}
	}
	sort_players(sortlist, cmp_func);
	count = 0;
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (who_ok(sortlist[p1], sel_bits))
			count++;
	}
	startpoint = floor((float) count * start_perc);
	stoppoint = ceil((float) count * stop_perc) - 1;
	num_who = 0;
	count = 0;
	for (p1 = 0; p1 < player_globals.p_num; p1++) {
		if (!who_ok(sortlist[p1], sel_bits))
			continue;
		if (count >= startpoint && count <= stoppoint)
			plist[num_who++] = sortlist[p1];
		count++;
	}

	if (num_who == 0) {
		pprintf(p,
			_("No logged in players match the flags in your who request.\n"));
		FREE(plist);
		FREE(sortlist);
		return COM_OK;
	}

	switch (style) {
	case 0: who_terse(p, num_who, plist, sort_type); break;
	case 1: who_verbose(p, num_who, plist); break;
	case 2: who_winloss(p, num_who, plist); break;
	case 3: who_fullinfo(p, num_who, plist); break;
	default: goto bad_parameters;
	}

	FREE(plist);
	FREE(sortlist);
	return COM_OK;

bad_parameters:
	FREE(plist);
	FREE(sortlist);
	return COM_BADPARAMETERS;
}

int com_open(int p, param_list param)
{
  int retval = pcommand(p, "set open");
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_simopen(int p, param_list param)
{
  int retval = pcommand(p, "set simopen");
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_bell(int p, param_list param)
{
  int retval = pcommand(p, "set bell");
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_flip(int p, param_list param)
{
  int retval = pcommand(p, "set flip");
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_style(int p, param_list param)
{
  int retval = pcommand(p, "set style %d", param[0].val.integer);
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

int com_promote(int p, param_list param)
{
  int retval = pcommand(p, "set promote %s", param[0].val.word);
  return retval == COM_OK ? COM_OK_NOPROMPT : retval;
}

void alias_add(int p, const char *name, const char *value)
{
	char *esc_name, *esc_value;
	struct player *pp = &player_globals.parray[p];

	pp->alias_list = realloc(pp->alias_list,
						      sizeof(struct alias_type) * (pp->numAlias+1));
	pp->alias_list[pp->numAlias].comm_name = strdup(name);
	pp->alias_list[pp->numAlias].alias = strdup(value);
	pp->numAlias++;

	if (CheckPFlag(p, PFLAG_REG) && !pp->loading ){
		if( dbi_conn_quote_string_copy(dbiconn, name, &esc_name)
		 && dbi_conn_quote_string_copy(dbiconn, value, &esc_value) )
		{
			dbi_result_free( dbi_conn_queryf( dbiconn,
				"INSERT INTO UserCommandAlias (ownerid, alias, targetcommand) "
									  "VALUES (%i, %s, %s)",
							 pp->chessduserid, esc_name, esc_value ) );
			free(esc_name);
			free(esc_value);
		}
	}
}

int com_alias(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int al, i;
	const char *noalias[] = {"quit", "unalias", NULL};

	if (param[0].type == TYPE_NULL) {
		for (al = 0; al < pp->numAlias; al++) {
			pprintf(p, "%s -> %s\n",
				pp->alias_list[al].comm_name,
				pp->alias_list[al].alias);
		}
		return COM_OK;
	}

	al = alias_lookup(param[0].val.word, pp->alias_list, pp->numAlias);
	if (param[1].type == TYPE_NULL) {
		if (al < 0) {
			pprintf(p, _("You have no alias named '%s'.\n"), param[0].val.word);
		} else {
			pprintf(p, "%s -> %s\n",
				pp->alias_list[al].comm_name,
				pp->alias_list[al].alias);
		}
		return COM_OK;
	}

	if (al >= 0) {
		FREE(pp->alias_list[al].alias);
		pp->alias_list[al].alias = strdup(param[1].val.string);

		if (CheckPFlag(p, PFLAG_REG))
			dbi_result_free( dbi_conn_queryf( dbiconn,
					"UPDATE UserCommandAlias SET targetcommand = '%s' "
					"WHERE ownerid = %i AND alias = '%s'",
					pp->alias_list[al].alias,
					pp->chessduserid, pp->alias_list[al].comm_name) );

		pprintf(p, _("Alias %s replaced.\n"), param[0].val.string);
		return COM_OK;
	}

	if (pp->numAlias >= config.ints[max_aliases]) {
		pprintf(p, _("You have your maximum number of aliases.\n"));
		return COM_OK;
	}

	for (i=0;noalias[i];i++) {
		if (strcasecmp(param[0].val.string, noalias[i]) == 0) {
			pprintf(p, _("Sorry, you can't alias this command.\n"));
			return COM_OK;
		}
	}

	alias_add(p, param[0].val.word, param[1].val.string);

	pprintf(p, _("Alias set.\n"));

	return COM_OK;
}

int com_unalias(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int al, i;

	al = alias_lookup(param[0].val.word, pp->alias_list, pp->numAlias);
	if (al < 0) {
		pprintf(p, _("You have no alias named '%s'.\n"), param[0].val.word);
		return COM_OK;
	}

	FREE(pp->alias_list[al].comm_name);
	FREE(pp->alias_list[al].alias);
	for (i = al; i < pp->numAlias-1; i++) {
		pp->alias_list[i].comm_name = pp->alias_list[i+1].comm_name;
		pp->alias_list[i].alias = pp->alias_list[i+1].alias;
	}
	pp->numAlias--;
	pp->alias_list = realloc(pp->alias_list,
						      sizeof(struct alias_type) * pp->numAlias);

	if (CheckPFlag(p, PFLAG_REG))
		dbi_result_free( dbi_conn_queryf( dbiconn,
			"DELETE FROM usercommandalias WHERE ownerid = %i AND alias = '%s'",
			pp->chessduserid, param[0].val.word ) );

	pprintf(p,_("Alias removed.\n"));

	return COM_OK;
}

int com_getgi(int p, param_list param)
{
  int p1, g;
  struct player *pp = &player_globals.parray[p];

  if (!in_list(p, L_TD, pp->name)) {
    pprintf(p, _("Only TD programs are allowed to use this command.\n"));
    return COM_OK;
  }
  if (((p1 = player_find_bylogin(param[0].val.word)) < 0)
      || (!CheckPFlag(p1, PFLAG_REG))) {
    /* Darkside suggested not to return anything */
    return COM_OK;
  }
  if (!CheckPFlag(p1, PFLAG_REG)) {
    pprintf(p, "*getgi %s none none -1 -1 -1 -1 -1*\n", player_globals.parray[p1].name);
  } else if (player_globals.parray[p1].game >= 0) {
    g = player_globals.parray[p1].game;
    if (game_globals.garray[g].status == GAME_ACTIVE) {
      pprintf(p, "*getgi %s %s %s %d %d %d %d %d*\n",
	      player_globals.parray[p1].name,
	      game_globals.garray[g].white_name,
	      game_globals.garray[g].black_name,
	      g + 1,
	      game_globals.garray[g].wInitTime,
	      game_globals.garray[g].wIncrement,
	      game_globals.garray[g].rated,
	      game_globals.garray[g].private);
    } else {
      pprintf(p, _("%s is not playing a game.\n"), player_globals.parray[p1].name);
    }
  } else {
    pprintf(p, _("%s is not playing a game.\n"), player_globals.parray[p1].name);
  }
  return COM_OK;
}

int com_getpi(int p, param_list param)
{
  struct player *pp = &player_globals.parray[p];
  int p1;

  if (!in_list(p, L_TD, pp->name)) {
    pprintf(p, _("Only TD programs are allowed to use this command.\n"));
    return COM_OK;
  }
  if (((p1 = player_find_bylogin(param[0].val.word)) < 0)
      || (!CheckPFlag(p1, PFLAG_REG))) {
    /* Darkside suggested not to return anything */
    return COM_OK;
  }
  if (!CheckPFlag(p1, PFLAG_REG)) {
    pprintf(p, "*getpi %s -1 -1 -1*\n", player_globals.parray[p1].name);
  } else {
    pprintf(p, "*getpi %s %d %d %d %d*\n", player_globals.parray[p1].name,
	    player_globals.parray[p1].w_stats.rating,
	    player_globals.parray[p1].b_stats.rating,
	    player_globals.parray[p1].s_stats.rating,
	    player_globals.parray[p1].l_stats.rating);
  }
  return COM_OK;
}

int com_checkIP(int p, param_list param)
{
	char *ipstr;
	int p1;
	unsigned len;

	if (!check_admin(p, ADMIN_ADMIN))
		return COM_OK;

	if (isdigit(param[0].val.word[0]))
		ipstr = param[0].val.word;
	else {
		if ((p1 = player_find_part_login(param[0].val.word)) >= 0)
			ipstr = strdup(dotQuad(player_globals.parray[p1].thisHost));
		else {
			pprintf(p, _("Player %s is not logged on.\n"), param[0].val.word);
			return COM_OK;
		}
	}
	len=0;
	while(len < strlen(ipstr) && ipstr[len] != '*')
		len++;
	pprintf(p, _("Matches the following player(s): \n\n"));
	for (p1 = 0; p1 < player_globals.parray_size; p1++)
		if (!strncmp(dotQuad(player_globals.parray[p1].thisHost), ipstr, len)
				&& (player_globals.parray[p1].status == PLAYER_PROMPT))
			pprintf(p, "%16.16s %s\n", player_globals.parray[p1].name,
					dotQuad(player_globals.parray[p1].thisHost));
	if (!isdigit(param[0].val.word[0]))
		free(ipstr);
	return COM_OK;
}

int com_limits(int p, param_list param)
{
	pprintf(p, _("\nCurrent hardcoded limits:\n"));
	pprintf(p, _("  Max number of channels and max capacity: %d\n"), MAX_CHANNELS);
	pprintf(p, _("  Max number of channels one can be in: %d\n"), MAX_INCHANNELS);
	pprintf(p, _("  Max number of people on the notify list: %d\n"), MAX_NOTIFY);
	pprintf(p, _("  Max number of people on the censor list: %d\n"), MAX_CENSOR);
	pprintf(p, _("  Max number of people in a simul game: %d\n"), MAX_SIMUL);
	pprintf(p, _("  Min number of games to be active: %d\n"), PROVISIONAL);

	pprintf(p, _("\nAdmin settable limits:\n"));
	pprintf(p, _("  Shout quota gives two shouts per %d seconds.\n"), seek_globals.quota_time);
	return COM_OK;
}
