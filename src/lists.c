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

/* lists.c
 *  New global lists code
 *
 *
 *  Added by Shaney, 29 May 1995  :)
 *
*/

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef __Linux__
#include <linux/stddef.h>
#endif

#include "lists.h"
#include "chessd_locale.h"
#include "comproc.h"
#include "configuration.h"
#include "config_genstrc.h"
#include "gamedb.h"
#include "globals.h"
#include "malloc.h"
#include "multicol.h"
#include "playerdb.h"
#include "ratings.h"
#include "talkproc.h"
#include "utils.h"
#include "dbdbi.h"

static struct List *firstGlobalList = NULL;

static ListTable ListArray[] =
{{P_PUBLIC, "admin"},
 {P_HEAD,  "removedcom"},
 {P_GOD, "filter"},
 {P_MASTER, "ban"},
 {P_ADMIN, "abuser"},
 {P_ADMIN, "muzzle"},
 {P_ADMIN, "cmuzzle"},
 {P_ADMIN, "c1muzzle"}, /* possible FICS trouble spots */
 {P_ADMIN, "c24muzzle"}, /* would prefer two param addlist - DAV */
 {P_ADMIN, "c46muzzle"}, /* is a temp solution */
 {P_ADMIN, "c49muzzle"},
 {P_ADMIN, "c50muzzle"},
 {P_ADMIN, "c51muzzle"},
 {P_PUBLIC, "fm"},
 {P_PUBLIC, "im"},
 {P_PUBLIC, "gm"},
 {P_PUBLIC, "wgm"},
 {P_PUBLIC, "blind"},
 {P_PUBLIC, "teams"},
 {P_PUBLIC, "computer"},
 {P_PUBLIC, "td"},
 {P_PERSONAL, "censor"},
 {P_PERSONAL, "gnotify"},
 {P_PERSONAL, "noplay"},
 {P_PERSONAL, "notify"},
 {P_PERSONAL, "channel"},
 {P_PERSONAL, "follow"},
 {P_ADMIN, "manager"},
 {P_ADMIN, "c2muzzle"},
 {P_ADMIN, "teacher"},
 {P_HEAD, "server_master"},
 {P_ADMIN, "silent"}, // = L_CHMUZZLE
 {P_ADMIN, "name_abuser"},
 {0, NULL}};

/* free up memory used by lists */
void lists_close(void)
{
	struct List *plst, *next;
	int i;

	for (plst = firstGlobalList; plst; ) {
		next = plst->next;
		for (i = 0; i < plst->numMembers; i++)
			FREE(plst->m_member[i]);
		FREE(plst->m_member);
		plst->numMembers = 0;
		free(plst);
		plst = next;
	}

	firstGlobalList = NULL;
}

/* find a list.  does not load from disk */
static struct List *list_find1(int p, enum ListWhich lidx)
{
	struct player *pp = &player_globals.parray[p];
	struct List *prev, *tempList, **starter;
	int personal;

	personal = ListArray[lidx].rights == P_PERSONAL;
	starter = personal ? &(pp->lists) : &firstGlobalList;

	for (prev = NULL, tempList = *starter;
		 tempList != NULL;
		 tempList = tempList->next) {

		if (lidx == tempList->which) {
			if (prev != NULL) {
				prev->next = tempList->next;
				tempList->next = *starter;
				*starter = tempList;
			}
			return tempList;
		}
		if (tempList->which >= L_LASTLIST) {
			/* THIS SHOULD NEVER HAPPEN!  But has been in personal list: bugs!
			 * */
			d_printf(_("fics: ERROR!! in list_find1()\n"));
			abort();
		}
		prev = tempList;
	}

	return NULL;
}

/* find a list.  loads from database if not in memory. */
static struct List *list_find(int p, enum ListWhich lidx)
{
	struct List *lst;
	dbi_result res;
	int i;

	lst = list_find1(p, lidx);
	if (lst)
		return lst;

	/* create the base list */
	list_add(p, lidx, NULL, 1);

	if (ListArray[lidx].rights == P_PERSONAL)
		list_find1(p, lidx);

	res = dbi_conn_queryf( dbiconn, 
		"SELECT value FROM ServerLists WHERE listID = %i", lidx);
	if(res){
		for (i=0; dbi_result_next_row(res); i++)
	        list_add (p, lidx, dbi_result_get_string_idx(res,1), 1);

		dbi_result_free(res);
	
		/* we've added some, retry */
		if (i)
			return list_find1(p, lidx);
	}
	return NULL;
}

/* add item to list */
/*
 * if int loading is 1 then we don't update the tables.
 */
int list_add(int p, enum ListWhich lidx, const char *s, int loading)
{
	struct player *pp = &player_globals.parray[p];
	struct List *gl = list_find1(p, lidx);
	char *sql, *esc_str;

	if (!gl) {
		gl = calloc(1, sizeof(*gl));
		gl->which = lidx;
		if (ListArray[lidx].rights == P_PERSONAL) {
			gl->next = pp->lists;
			pp->lists = gl;
		} else {
			gl->next = firstGlobalList;
			firstGlobalList = gl;
		}
	}

	if (!s) return 0;

	if (ListArray[lidx].rights == P_PERSONAL 
	&& gl->numMembers >= config.ints[max_user_list_size])
		return 1;

	while (isspace(*s)) 
		s++;

	gl->m_member = realloc(gl->m_member, sizeof(char*) * (gl->numMembers+1));
	gl->m_member[gl->numMembers] = strndup(s, strcspn(s, " \t\r\n"));
	gl->numMembers++;

	/* update the SQL database if the user is a registered user */
	/* gabrielsan:
	 *		if the user does not have a valid chessduserid, don't
	 *		bother trying to insert anything
	 */
	if ( !loading && CheckPFlag(p, PFLAG_REG) && !pp->loading && pp->chessduserid > 0 ){

		dbi_conn_quote_string_copy(dbiconn, s, &esc_str);

		switch (lidx) {
		case L_NOTIFY:
			sql = "INSERT INTO UserNotify (ownerid, targetuser) VALUES (%i, %s)";
			break;
		case L_GNOTIFY:
			sql = "INSERT INTO UserGNotify (ownerid, targetuser) VALUES (%i, %s)";
			break;
		case L_FOLLOW:
			sql = "INSERT INTO UserFollow (ownerid, targetuser) VALUES (%i, %s)";
			break;
		case L_NOPLAY:
			sql = "INSERT INTO UserNoPlay (ownerid, targetuser) VALUES (%i, %s)";
			break;
		case L_CENSOR:
			sql = "INSERT INTO UserCensor (ownerid, targetuser) VALUES (%i, %s)";
			break;
		case L_CHANNEL:
			sql = "INSERT INTO UserChannel (ownerid, channel) VALUES (%i, %s);";
			break;
		default:
			dbi_result_free( dbi_conn_queryf( dbiconn,
				"INSERT INTO ServerLists (listID, value) VALUES (%i, %s);", 
											  lidx, esc_str ) );
			return 0;
		}
		dbi_result_free(dbi_conn_queryf(dbiconn, sql, pp->chessduserid, esc_str));
		
		FREE(esc_str);
	}/* if registered user */

	return 0;
}

/* remove item from list */
static int list_sub(int p, enum ListWhich lidx, char *s)
{
	struct player *pp = &player_globals.parray[p];
	struct List *gl = list_find(p, lidx);
	int i, pid;
	char *sql;

	if (!gl)
		return 1;

	for (i = 0; i < gl->numMembers; i++) {
		if (!strcasecmp(s, gl->m_member[i]))
			break;
	}
	if (i == gl->numMembers)
		return 1;

	FREE(gl->m_member[i]);

	for (; i < (gl->numMembers - 1); i++)
		gl->m_member[i] = gl->m_member[i+1];

	gl->numMembers--;
	gl->m_member = realloc(gl->m_member, sizeof(char *) * gl->numMembers);

	/* update the SQL database if the user is a registered user */
	if (CheckPFlag(p, PFLAG_REG)){
		switch (lidx) {
		case L_NOTIFY:
			sql = "DELETE FROM UserNotify WHERE ownerid = %i AND UPPER(targetuser) = UPPER('%s')";
			break;
		case L_GNOTIFY:
			sql = "DELETE FROM UserGNotify WHERE ownerid = %i AND UPPER(targetuser) = UPPER('%s')";
			break;
		case L_FOLLOW:
			sql = "DELETE FROM UserFollow WHERE ownerid = %i AND UPPER(targetuser) = UPPER('%s')";
			break;
		case L_NOPLAY:
			sql = "DELETE FROM UserNoPlay WHERE ownerid = %i AND UPPER(targetuser) = UPPER('%s')";
			break;
		case L_CENSOR:
			sql = "DELETE FROM UserCensor WHERE ownerid = %i AND UPPER(targetuser) = UPPER('%s')";
			break;
		case L_CHANNEL:
			sql = "DELETE FROM UserChannel WHERE ownerid = %i AND channel = '%s'";
			break;
		case L_MANAGER:
			// gabrielsan
			// if one wants to remove the user from the manager
			// list, the user should be removed from channel
			pid = player_find_bylogin(s);
			if (pid > 0) {
				char ch_str[10];
				sprintf(ch_str, "%d", MANAGER_CHANNEL);
				list_sub(pid, L_CHANNEL, ch_str);
				pprintf_prompt(pid, _("You lost your manager rights\n"));
			}
			else {
				pid = getChessdUserID(s);
				if (pid >= 0)
					dbi_result_free( dbi_conn_queryf( dbiconn,
						"DELETE FROM UserChannel "
						"WHERE ownerid = %i AND channel = %i",
						pid, MANAGER_CHANNEL ) );
			}
			return 0;
		default:
			dbi_result_free( dbi_conn_queryf( dbiconn,
				"DELETE FROM ServerLists "
				"WHERE listID = %i AND UPPER(value) = UPPER('%s')", lidx, s ) );
			return 0;
		}
		dbi_result_free(dbi_conn_queryf(dbiconn, sql, pp->chessduserid, s));
	}

	return 0;
}

/* find list by name, doesn't have to be the whole name */
struct List *list_findpartial(int p, char *which, int gonnado)
{
  struct player *pp = &player_globals.parray[p];
  struct List *gl;
  int i, foundit, slen;

  slen = strlen(which);
  for (i = 0, foundit = -1; ListArray[i].name != NULL; i++) {
	  if (!strncasecmp(ListArray[i].name, which, slen)) {
		  if (foundit == -1)
			  foundit = i;
		  else
			  return NULL;		/* ambiguous */
	  }
  }

  if (foundit != -1) {
    int rights = ListArray[foundit].rights;
    int youlose = 0;

    switch (rights) {		/* check rights */
    case P_HEAD:
      if (gonnado && !player_ishead(p))
		  youlose = 1;
      break;
    case P_GOD:
      if ((gonnado && (pp->adminLevel < ADMIN_GOD)) ||
	  	  (!gonnado && (pp->adminLevel < ADMIN_ADMIN)))
		  youlose = 1;
      break;
    case P_DEMIGOD:
	  if ((gonnado && (pp->adminLevel < ADMIN_DEMIGOD)) ||
		  (!gonnado && (pp->adminLevel < ADMIN_ADMIN)))
		  youlose = 1;
      break;
    case P_MASTER:
	  if ((gonnado && (pp->adminLevel < ADMIN_MASTER)) ||
		  (!gonnado && (pp->adminLevel < ADMIN_ADMIN)))
		  youlose = 1;
      break;
    case P_ADMIN:
      if (pp->adminLevel < ADMIN_ADMIN)
		  youlose = 1;
      break;
    case P_PUBLIC:
      if (gonnado && (pp->adminLevel < ADMIN_ADMIN))
		  youlose = 1;
      break;
    }

    if (youlose) {
      pprintf(p, _("\"%s\" is not an appropriate list name or you have insufficient rights.\n"), which);
      return NULL;
    }

    gl = list_find(p, foundit);

  } else {
    pprintf(p, _("\"%s\" does not match any list name.\n"), which);
    return NULL;
  }
  return gl;
}

/* see if something is in a list */
int in_list(int p, enum ListWhich which, char *member)
{
	struct List *gl;
	int i;
	int filterList = (which == L_FILTER);

	gl = list_find(p, which);
	if ((gl == NULL) || (member == NULL))
		return 0;
	for (i = 0; i < gl->numMembers; i++) {
		if (filterList) {
			if (!strncasecmp(member, gl->m_member[i], strlen(gl->m_member[i])))
				return 1;
		} else {
			if (!strcasecmp(member, gl->m_member[i]))
				return 1;
		}
	}
	return 0;
}

/* add or subtract something to/from a list */
int list_addsub(int p, char* list, char* who, int addsub)
{
	struct player *pp = &player_globals.parray[p];
	struct List *gl;
	int p1, connected, loadme, personal, ch;
	char *listname, *member, junkChar, *yourthe, *addrem;

	gl = list_findpartial(p, list, addsub);
	if (!gl)
		return COM_OK;

	personal = ListArray[gl->which].rights == P_PERSONAL;
	loadme = gl->which != L_FILTER
		  && gl->which != L_REMOVEDCOM
		  && gl->which != L_CHANNEL;
	listname = ListArray[gl->which].name;
	yourthe = personal ? _("your") : _("the");
	addrem = addsub == 1 ? _("added to") : _("removed from");

	if (loadme) {
		if (!FindPlayer(p, who, &p1, &connected)) {
			if (addsub == 1)
				return COM_OK;
			member = who;		/* allow sub removed/renamed player */
			loadme = 0;
		} else
			member = player_globals.parray[p1].name;
	} else
		member = who;

	if (addsub == 1) {		/* add to list */

		if (gl->which == L_CHANNEL) {

			if (sscanf (who, "%d%c", &ch, &junkChar) == 1 && ch >= 0 && ch < 255) {
				switch (ch) {
				case 0:
					// gabrielsan: perhaps temporary
				case MASTER_CHANNEL:
					if (!in_list(p,L_ADMIN,pp->name)) {
						pprintf(p, _("Only admins may join channel %d!\n"), ch);
						return COM_OK;
					}
					break;
				case HELPER_CHANNEL:
					if (!check_admin(p, ADMIN_HELPER)) {
						pprintf(p,
							_("Only admins and helpers may join channel %d!\n"), ch);
						return COM_OK;
					}
					break;
				case MANAGER_CHANNEL:
					if (!in_list(p,L_MANAGER,pp->name) &&
							!check_admin(p, ADMIN_HELPER)) {
						pprintf(p, _("Only manager, admins and helpers "
									 "may join channel %d!\n"), ch);
						return COM_OK;
					}
					break;
				}
			} else {
				pprintf (p,
						_("The channel to add must be a number between 0 and %d.\n"),
						MAX_CHANNELS - 1);
				return COM_OK;
			}
		}
		if (in_list(p, gl->which, member)) {
			pprintf(p, _("[%s] is already on %s %s list.\n"),
					member, yourthe, listname);
			if (loadme && !connected)
				player_remove(p1);
			return COM_OK;
		}
		if (list_add(p, gl->which, member, 0)) {
			pprintf(p, _("Sorry, %s %s list is full.\n"), yourthe, listname);
			if (loadme && !connected)
				player_remove(p1);
			return COM_OK;
		}
	} else if (addsub == 2) {	/* subtract from list */
		if (!in_list(p, gl->which, member)) {
			pprintf(p, _("[%s] is not in %s %s list.\n"), member, yourthe, listname);
			if (loadme && !connected)
				player_remove(p1);
			return COM_OK;
		}
		list_sub(p, gl->which, member);
	}
	pprintf(p, _("[%s] %s %s %s list.\n"), member, addrem, yourthe, listname);

	if (!personal) {

		switch (gl->which) {
		case L_MUZZLE:
		case L_CMUZZLE:
		case L_C1MUZZLE:
		case L_C2MUZZLE:
		case L_C24MUZZLE:
		case L_C46MUZZLE:
		case L_C49MUZZLE:
		case L_C50MUZZLE:
		case L_C51MUZZLE:
		case L_ABUSER:
		case L_BAN:
			pprintf(p, _("Please leave a comment to explain why %s was %s the %s list.\n"), member, addrem, listname);
			/*		pcommand(p, "addcomment %s %s %s list.\n", member, addrem, listname); */
			break;
		case L_COMPUTER:
			/*
			if (player_globals.parray[p1].b_stats.rating > 0)
				UpdateRank(TYPE_BLITZ, member, &player_globals.parray[p1].b_stats, member);
			if (player_globals.parray[p1].s_stats.rating > 0)
				UpdateRank(TYPE_STAND, member, &player_globals.parray[p1].s_stats, member);
			if (player_globals.parray[p1].w_stats.rating > 0)
				UpdateRank(TYPE_WILD, member, &player_globals.parray[p1].w_stats,
						   member);
			*/
			break;
		case L_ADMIN:
			if (addsub == 1) {	/* adding to list */
				if (player_globals.parray[p1].adminLevel == 0) {
					pcommand(p, "asetadmin %s 10", player_globals.parray[p1].login);
					pprintf(p, _("%s has been given an admin level of 10 - change with asetadmin.\n"), member);
				}
			} else {
				if (check_admin2(p, p1) || p == p1)
					pcommand(p, "asetadmin %s 0", player_globals.parray[p1].login);
				//player_globals.parray[p1].adminLevel = 0;
			}
			break;
		case L_FILTER:
		case L_REMOVEDCOM:
		default:
			break;
		}

		/*	if (addsub==1)
			list_add (p, gl->which, who, 0);
			else
			list_sub (p, gl->which, who);
			*/
	}
	if (loadme && !connected)
		player_remove(p1);

	return COM_OK;
}

int com_addlist(int p, param_list param)
{
	return list_addsub(p, param[0].val.word, param[1].val.word, 1);
}

int com_sublist(int p,param_list param)
{
	return list_addsub(p, param[0].val.word, param[1].val.word, 2);
}

/* toggle being in a list */
int com_togglelist(int p,param_list param)
{
	char *list = param[0].val.word;
	char *val = param[1].val.word;
	struct List *gl = list_findpartial(p, list, 0);
	if (!gl) {
		pprintf(p, _("'%s' does not match any list name.\n"), list);
		return COM_FAILED;
	}
	if (in_list(p, gl->which, val))
		return com_sublist(p, param);

	return com_addlist(p, param);
}

int com_showlist(int p, param_list param)
{
  struct player *pp = &player_globals.parray[p];
  struct List *gl;
  int i, rights;

  char *rightnames[] = {"EDIT HEAD, READ ADMINS", "EDIT GODS, READ ADMINS",
	  "READ/WRITE ADMINS", "PUBLIC", "PERSONAL", "READ/WRITE DEMIGODS",
  	  "READ/WRITE MASTERS"};

  if (param[0].type == 0) {	/* Show all lists */
	  pprintf(p, _("Lists:\n\n"));
	  for (i = 0; ListArray[i].name != NULL; i++) {
		  rights = ListArray[i].rights;
		  if ((rights > P_ADMIN) || (pp->adminLevel >= ADMIN_ADMIN))
			  pprintf(p, "%-20s is %s\n", ListArray[i].name, rightnames[rights]);
	  }
  } else {			/* find match in index */
    gl = list_findpartial(p, param[0].val.word, 0);
    if (!gl) {
      return COM_OK;
    }
    rights = ListArray[gl->which].rights;
    /* display the list */
    {
      multicol *m = multicol_start(gl->numMembers);

      pprintf(p, _("-- %s list: %d %s --"), ListArray[gl->which].name,
	      gl->numMembers,
	      !strcmp(ListArray[gl->which].name,"filter") ? "ips" 
	      	: !strcmp(ListArray[gl->which].name,"removedcom") ? "commands" 
	      	  : !strcmp(ListArray[gl->which].name,"channel") ? "channels" : "names" );
      for (i = 0; i < gl->numMembers; i++)
		  multicol_store_sorted(m, gl->m_member[i]);
      multicol_pprint(m, p, pp->d_width, 2);
      multicol_end(m);
    }
  }
  return COM_OK;
}

int list_channels(int p,int p1)
{
	struct player *pp = &player_globals.parray[p];
	struct List *gl;
	int i, rights;
	
	gl = list_findpartial(p1, "channel", 0);
	if (!gl)
		return 1;
	
	rights = ListArray[gl->which].rights;
	/* display the list */
	if (gl->numMembers == 0)
		return 1;
	
	multicol *m = multicol_start(gl->numMembers);
	
	for (i = 0; i < gl->numMembers; i++)
		multicol_store_sorted(m, gl->m_member[i]);
	multicol_pprint(m, p, pp->d_width, 1);
	multicol_end(m);
	
	return 0;
}

/* free the memory used by a list */
void list_free(struct List * gl)
{
	int i;
	struct List *temp;

	while (gl) {
		for (i = 0; i < gl->numMembers; i++)
			FREE(gl->m_member[i]);
		FREE(gl->m_member);
		temp = gl->next;
		free(gl);
		gl = temp;
	}
}

/* check lists for validity - pure paranoia */
void lists_validate(int p)
{
	struct player *pp = &player_globals.parray[p];
	struct List *gl;

	for (gl=pp->lists; gl; gl=gl->next) {
		if (gl->numMembers && !gl->m_member)
			gl->numMembers = 0;
	}
}

int titled_player(int p,char* name)
{
	return in_list(p, L_FM, name)
		|| in_list(p, L_IM, name)
		|| in_list(p, L_GM, name)
		|| in_list(p, L_WGM, name);
}

