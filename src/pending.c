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

#include <string.h>
#include <stdlib.h>
#include "pending.h"
#include "malloc.h"
#include "utils.h"
#include "chessd_locale.h"
#include "globals.h"
#include "playerdb.h"
#include "matchproc.h"

#include "protocol.h"

const char *pendTypes[] = {
	"match", "draw",  "abort", "takeback", "adjourn", "switch",
	"simul", "pause", "partner", "bughouse", "unpause",
	NULL
};

extern const char *colorstr[];

/* iterator for player p */
static struct pending* next_pending(struct pending* current)
{
	if (current == NULL) {
		d_printf(_("NULL pointer passed to next_pending\n"));
		return NULL;
	} else 
		return current->next;
}

void notify_pending_status(struct pending *current, int status)
{
	/*
	 * Notify the users the this pendency status has been changed
	 * status 0 = removed
	 * status 1 = added
	 */
	protocol_pending_status(current->whofrom,
			player_globals.parray[current->whofrom].name,
			player_globals.parray[current->whoto].name,
			current->type,
			status);

	protocol_pending_status(current->whoto,
			player_globals.parray[current->whofrom].name,
			player_globals.parray[current->whoto].name,
			current->type,
			status);
}

/* deletor for pending - returns next item*/
struct pending *delete_pending(struct pending *current)
{
	struct pending *p;

	if (current == NULL) {
		d_printf(_("NULL pointer passed to delete_pending\n"));
		return NULL;
	}

	player_globals.parray[current->whofrom].number_pend_from--;
	player_globals.parray[current->whoto].number_pend_to--;

	// notify the users that this pendency has been removed
	notify_pending_status(current, 0);

	FREE(current->category);
	FREE(current->board_type);

	if (current == seek_globals.pendlist) {
		seek_globals.pendlist = current->next;
		free(current);
		return seek_globals.pendlist;
	}

	for (p=seek_globals.pendlist; p && p->next != current; p=p->next)
		;

	if (!p) {
		d_printf(_("current not on pending list??"));
		return NULL;
	}

	p->next = current->next;

	free(current);
	return p->next;
}

/* kills the whole pending list on shutdown */
void destruct_pending(void)
{
	struct pending* current = seek_globals.pendlist;

	while (current != NULL)
		current = delete_pending(current);
}

struct pending *add_pending(int from,int to,int type)
{
	struct pending* new = (struct pending*) malloc(sizeof(struct pending));

	new->whofrom = from;
	new->whoto = to;
	new->type = type;
	new->next = seek_globals.pendlist;
	new->category = NULL;
	new->board_type = NULL;

	player_globals.parray[from].number_pend_from++;
	player_globals.parray[to].number_pend_to++;

	seek_globals.pendlist = new;

	// notify the users that this pendency has been added
	notify_pending_status(new, 1);

	return new;
}

static int check_current_pend(struct pending* current, int p, int p1, int type)
{
    if( (p != -1 && current->whofrom != p) 
     || (current->whoto != p1 && p1 != -1) )
      return 0;
    return type == PEND_ALL || current->type == type 
    	   || (type < 0 && current->type != -type);
/* The above allows a type of -PEND_SIMUL to match every request
    EXCEPT simuls, for example.  I'm doing this because Heringer does
    not want to decline simul requests when he makes a move in a simul.
    -- hersco. */
}

/* from p to p1 */
struct pending *find_pend(int p, int p1, int type)
{
	struct player *pp = &player_globals.parray[p];
	/* -ve type can be used to decline all but that type */
	struct pending* current = seek_globals.pendlist;

	if ((p != -1 && !pp->number_pend_from) ||
	    (p1 != -1 && !player_globals.parray[p1].number_pend_to))
		return NULL;

	while (current != NULL) {
		if (check_current_pend(current, p, p1, type))
			return current;
		current = next_pending(current);
	}
	return NULL;
}

/* from p to p1 */
struct pending *add_request(int p, int p1, int type)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* new = NULL;

	if ((pp->number_pend_from) &&
	    (player_globals.parray[p1].number_pend_to)) {
		if ((new = find_pend(p, p1, type)) != NULL)
			return new;                  /* Already exists */
	}

	if ((new = add_pending(p,p1,type)) == NULL)
		return NULL;
	return new;
}

/* removes all requests from p to p1 */
void remove_request(int p, int p1, int type)
{
	struct pending* next;
	struct pending* current = seek_globals.pendlist;

	while (current != NULL) {
		next = next_pending(current);
		if (check_current_pend(current,p,p1,type))
			delete_pending(current);
		current = next;
	}
}

/* decline offers to p and withdraw offers from p */
static void do_decline_withdraw_offers(int p,int p1, int offers, int partners, int wd)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* offer = seek_globals.pendlist;
	int type, p2, decline;
	char *pName = pp->name, *p2Name, *typestr;

	if ((wd & DO_DECLINE) && (wd & DO_WITHDRAW)) { /* cut search times */
		if ((p != -1  && !pp->number_pend_to && !pp->number_pend_from)
		|| (p1 != -1 && !player_globals.parray[p1].number_pend_to
		&& !player_globals.parray[p1].number_pend_from))
			return;
	}

	if ((wd & DO_DECLINE) && !(wd & DO_WITHDRAW)) { /* cut search times */
		if ((p != -1 && !pp->number_pend_to) 
		|| (p1 != -1 && !player_globals.parray[p1].number_pend_from))
			return;
	}

	if (!(wd & DO_DECLINE) && (wd & DO_WITHDRAW)) { /* cut search times */
		if ((p != -1 && !pp->number_pend_from) 
		|| (p1 != -1 && !player_globals.parray[p1].number_pend_to))
			return;
	}

	while (offer != NULL) {

		decline = 0;
		if ((wd & DO_DECLINE) && check_current_pend(offer, p1, p, offers)) {
			p2 = offer->whofrom;
			decline = 1;
		} else if ((wd & DO_WITHDRAW) && check_current_pend(offer,p, p1, offers)) {
			p2 = offer->whoto;
		} else {
			offer = offer->next;
			continue;
		}
	
		type = offer->type;
		p2Name = player_globals.parray[p2].name;
	
		switch (type) {
		case PEND_MATCH:
			if (partners) {
				if (offer->game_type != TYPE_BUGHOUSE) {
					offer = offer->next;
					continue;
				} else if (decline) {
					if (offer->whoto == p) {
						pprintf(p, _("Either your opponent's partner or your partner has joined another match.\n"));
						pprintf_prompt(p, _("Removing the bughouse offer from %s.\n"), 
									   p2Name);
					
						pprintf(p2, _("Either your opponent's partner or your partner has joined another match.\n"));
						pprintf_prompt(p2, _("Removing the bughouse offer to %s.\n"), 
									   pName);
					} else {
						pprintf_prompt(p, _("Your partner declines the bughouse offer from %s.\n"), 
									   p2Name);
						pprintf_prompt(p2, _("%s declines the bughouse offer from your partner.\n"), 
									   pName);
					}
				} else {
					if (offer->whofrom == p) {
						pprintf(p, _("Either your opponent's partner or your partner has joined another match.\n"));
						pprintf_prompt(p, _("Removing the bughouse offer to %s.\n"), 
									   p2Name);
					
						pprintf(p2, _("Either your opponent's partner or your partner has joined another match.\n"));
						pprintf_prompt(p2, _("Removing the bughouse offer from %s.\n"), 
									   pName);
					} else {
						pprintf_prompt(p, _("Your partner withdraws the bughouse offer to %s.\n"), 
									   p2Name);
						pprintf_prompt(p2, _("%s withdraws the bughouse offer to your partner.\n"), 
									   pName);
					}
				}
				break;
			}        	
		default:
			switch(type){
			case PEND_MATCH:	typestr = _("match offer"); break;
			case PEND_DRAW:		typestr = _("draw request"); break;
			case PEND_PAUSE:	typestr = _("pause request"); break;
			case PEND_UNPAUSE:	typestr = _("unpause request"); break;
			case PEND_ABORT:	typestr = _("abort request"); break;
			case PEND_TAKEBACK:	typestr = _("takeback request"); break;
			case PEND_ADJOURN:	typestr = _("adjourn request"); break;
			case PEND_SWITCH:	typestr = _("switch sides request"); break;
			case PEND_SIMUL:	typestr = _("simul offer"); break;
			case PEND_PARTNER:	typestr = _("partnership request"); break;
			default:
				typestr = "(unknown)";
				d_printf("do_decline_withdraw_offers(): bad offer type %d", type);
			}
			if (decline) {
				pprintf_prompt(p2, _("\n%s declines the %s.\n"), pName, typestr);
				pprintf(p, _("You decline the %s from %s.\n"), typestr, p2Name);
			} else {
				pprintf_prompt(p2, _("\n%s withdraws %s.\n"), pName, typestr);
				pprintf(p, _("You withdraw the %s to %s.\n"), typestr, p2Name);
			}
		}
		offer = delete_pending(offer);
	}
}

/* wd is DO_DECLINE to decline DO_WITHDRAW to withdraw  and both for mutual */
void decline_withdraw_offers(int p, int p1, int offerType,int wd)
{
	struct player *pp = &player_globals.parray[p];
	int partner;

	/* First get rid of bughouse offers from partner. */
	/* last param in if is a safety check - shouldn't happen */
	partner = p1 == -1 ? -1 : player_globals.parray[p1].partner;

	if ((offerType == PEND_MATCH || offerType == PEND_ALL)
	&& pp->partner >= 0 && player_globals.parray[pp->partner].partner == p)
		do_decline_withdraw_offers(pp->partner, partner , PEND_MATCH,1,wd);

	do_decline_withdraw_offers(p, p1, offerType,0,wd);
}

/* find nth offer from p */
static struct pending* find_nth_pendfrom(int p,int num)
{
	struct pending* current = seek_globals.pendlist;

	while (num) {
		if (check_current_pend(current,p,-1,PEND_ALL))
			num--;
		if (num > 0)
			current = next_pending(current);
	}
	return current;
}

/* find nth offer to p */
static struct pending* find_nth_pendto(int p,int num)
{
	struct pending* current = seek_globals.pendlist;

	while (num) {
		if (check_current_pend(current, -1, p, PEND_ALL))
			num--;
		if (num > 0)
			current = next_pending(current);
	}
	return current;
}

static int WordToOffer(int p, char *word, int *type, int *p1)
{
	/* Convert draw adjourn match takeback abort pause
	   simmatch switch partner or <name> to offer type. */
	int t;
	
	*p1 = -1;
	if(!strcmp(word, "all"))
		*type = PEND_ALL; // I think this is right?? previously not set by 'all' -- qu1j0t3
	else{
		for(t=0; pendTypes[t]; ++t)
			if(!strcmp(word, pendTypes[t])){
				*type = t;
	  			break;
			}
		if(!pendTypes[t]){ // did not match any
			*p1 = player_find_part_login(word);
			if (*p1 < 0) {
				pprintf(p, _("No user named \"%s\" is logged in.\n"), word);
				return 0;
			}
		}
	}
	return 1;
}

int com_accept(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	int type = -1, p1, number;
	struct pending* current = NULL;

	if (!(number = pp->number_pend_to)) {
		pprintf(p, _("You have no offers to accept.\n"));
		return COM_OK;
	}

	if (param[0].type == TYPE_NULL) {
		if (number != 1) {
			pprintf(p, _("You have more than one offer to accept.\n"
      					 "Use 'pending' to see them and 'accept n' to choose which one.\n"));
			return COM_OK;
		}
		current = find_pend(-1, p, PEND_ALL);
	} else if (param[0].type == TYPE_INT) {
		if (param[0].val.integer < 1 || param[0].val.integer > number) {
			pprintf(p, _("Out of range. Use 'pending' to see the list of offers.\n"));
			return COM_OK;
		}
		current = find_nth_pendto(p,param[0].val.integer);
	} else if (param[0].type == TYPE_WORD) {
		if (!WordToOffer (p,param[0].val.word,&type,&p1))
			return COM_OK;
		if ((p1 < 0) && ((current = find_pend(-1, p, type)) == NULL)) {
			pprintf(p, _("There are no pending %s offers.\n"), param[0].val.word);
			return COM_OK;
		} else if ((p1 >= 0) && (current = find_pend(p1, p, PEND_ALL)) == NULL) {
			pprintf(p, _("There are no pending offers from %s.\n"), 
					player_globals.parray[p1].name);
			return COM_OK;
		}
	}

	switch (current->type) {
    case PEND_MATCH:
		accept_match(current, p, current->whofrom);
		return COM_OK;
    case PEND_DRAW:
    case PEND_PAUSE:
    case PEND_UNPAUSE:
    case PEND_ABORT:
    case PEND_SWITCH:
    case PEND_ADJOURN:
		pcommand(p, pendTypes[current->type]);
		break;
    case PEND_TAKEBACK:
		pcommand(p, "takeback %d", current->wtime);
		break;
    case PEND_SIMUL:
		pcommand(p, "simmatch %s", player_globals.parray[current->whofrom].name);
		break;
    case PEND_PARTNER:
		pcommand(p, "partner %s", player_globals.parray[current->whofrom].name);
		break;
	}
	return COM_OK_NOPROMPT;
}

static int com_decline_withdraw(int p, param_list param,int wd)
{
	struct player *pp = &player_globals.parray[p];
	int type = -1, p1, number;
	struct pending* current = seek_globals.pendlist;

	if (wd & DO_DECLINE) {
		if (!(number = pp->number_pend_to)) {
			pprintf(p, _("You have no pending offers from other players.\n"));
			return COM_OK;
		}
	} else {
		if (!(number = pp->number_pend_from)) {
			pprintf(p, _("You have no pending offers to other players.\n"));
			return COM_OK;
		}
	}

	if (param[0].type == TYPE_NULL) {
		if (number == 1) {
			if (wd & DO_DECLINE) {
				current = find_pend(-1,p,PEND_ALL);
				p1 = current->whofrom;
			} else {
				current = find_pend(p,-1,PEND_ALL);
				p1 = current->whoto;
			}
			type = current->type;
		} else {
			if (wd & DO_DECLINE)
				pprintf(p, _("You have more than one pending offer.\n"
  							 "Use 'pending' to see them and 'decline n' to choose which one.\n"));
			else
				pprintf(p, _("You have more than one pending offer.\n"
       						 "Use 'pending' to see them and 'withdraw n' to choose which one.\n"));
			return COM_OK;
		}
	} else if (param[0].type == TYPE_INT) {
		if (param[0].val.integer < 1 || param[0].val.integer > number) {
			pprintf(p, _("Out of range. Use 'pending' to see the list of offers.\n"));
			return COM_OK;
		}
		if (wd & DO_DECLINE) {
			current = find_nth_pendto(p,param[0].val.integer);
			p1 = current->whofrom;
		} else {
			current = find_nth_pendfrom(p,param[0].val.integer);
			p1 = current->whoto;
		}
		type = current->type;
	} else if (!WordToOffer(p, param[0].val.word, &type, &p1))
		return COM_OK;

	decline_withdraw_offers(p, p1, type,wd);
	return COM_OK;
}

int com_decline(int p, param_list param)
{
	return com_decline_withdraw(p, param, DO_DECLINE);
}

int com_withdraw(int p, param_list param)
{
	return com_decline_withdraw(p, param, DO_WITHDRAW);
}

static void pend_print(int p, struct pending *pend)
{
	if (p == pend->whofrom)
		pprintf(p, _("You are offering %s "), player_globals.parray[pend->whoto].name);
	else
		pprintf(p, _("%s is offering "), player_globals.parray[pend->whofrom].name);

	switch (pend->type) {
	case PEND_MATCH:
		pprintf(p, _("a challenge: %s (%s) %s%s (%s) %s.\n"),
				player_globals.parray[pend->whofrom].name,
				ratstrii(GetRating(&player_globals.parray[pend->whofrom], pend->game_type),
						 pend->whofrom),
				colorstr[pend->seek_color + 1],
				player_globals.parray[pend->whoto].name,
				ratstrii(GetRating(&player_globals.parray[pend->whoto], pend->game_type),
						 pend->whoto),
				game_str(pend->rated, pend->wtime * 60, pend->winc,
						 pend->btime * 60, pend->binc, pend->category, pend->board_type));
		break;
	case PEND_DRAW:		pprintf(p, _("a draw.\n")); break;
	case PEND_PAUSE:	pprintf(p, _("to pause the clock.\n")); break;
	case PEND_UNPAUSE:	pprintf(p, _("to unpause the clock.\n")); break;
	case PEND_ABORT:	pprintf(p, _("to abort the game.\n")); break;
	case PEND_TAKEBACK:	pprintf(p, _("to takeback the last %d half moves.\n"), pend->wtime); break;
	case PEND_SIMUL:	pprintf(p, _("to play a simul match.\n")); break;
	case PEND_SWITCH:	pprintf(p, _("to switch sides.\n")); break;
	case PEND_ADJOURN:	pprintf(p, _("an adjournment.\n")); break;
	case PEND_PARTNER:	pprintf(p, _("to be bughouse partners.\n")); break;
	}
}

int com_pending(int p, param_list param)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* current = seek_globals.pendlist;
	int num = 0, num2 = 0, total = 0;

	if ((total = pp->number_pend_from)) {
		while (current != NULL && num != total) {
			if (check_current_pend(current, p, -1, PEND_ALL)) {
				if (!num)
					pprintf(p, _("Offers TO other players:\n"));
				num++;
				pprintf(p, " %d: ",num);
				pend_print(p, current);
			}
			current = next_pending(current);
		}
		pprintf(p, _("\nIf you wish to withdraw any of these offers type 'withdraw n'\n"
					 "or just 'withdraw' if there is only one offer.\n\n"));
	} else
		pprintf(p, _("There are no offers pending TO other players.\n\n"));

	current = seek_globals.pendlist;

	if ((total = pp->number_pend_to)) {
		while (current != NULL && num2 != total) {
			if (check_current_pend(current, -1, p, PEND_ALL)) {
				if (!num2)
					pprintf(p, _("Offers FROM other players:\n"));
				num2++;
				pprintf(p, " %d: ",num2);
				pend_print(p, current);
			}
			current = next_pending(current);
		}
		pprintf(p, _("\nIf you wish to accept any of these offers type 'accept n'\n"
    				 "or just 'accept' if there is only one offer.\n"));
	} else
		pprintf(p, _("There are no offers pending FROM other players.\n"));

	return COM_OK;
}

void pend_join_match (int p, int p1)
{
	struct player *pp = &player_globals.parray[p];
	struct pending* current;
	struct pending* next = seek_globals.pendlist;

	if (pp->number_pend_from == 0 && pp->number_pend_to == 0
	 && player_globals.parray[p1].number_pend_from == 0 
	 && player_globals.parray[p1].number_pend_to == 0)
		return;

	while (next != NULL) {
		current = next;
		next = next_pending(current);

		if (check_current_pend(current,p,-1,-PEND_PARTNER)) {
			pprintf_prompt(current->whoto, _("\n%s, who was challenging you, has joined a match with %s.\n"),
						   pp->name, player_globals.parray[p1].name);
			pprintf(p, _("Challenge to %s withdrawn.\n"), player_globals.parray[current->whoto].name);
			delete_pending(current);
		}else if (check_current_pend(current,p1,-1,-PEND_PARTNER)) {
			pprintf_prompt(current->whoto, _("\n%s, who was challenging you, has joined a match with %s.\n"), 
						   player_globals.parray[p1].name, pp->name);
			pprintf(p1, _("Challenge to %s withdrawn.\n"), player_globals.parray[current->whoto].name);
			delete_pending(current);
		}else if (check_current_pend(current,-1,p,-PEND_PARTNER)) {
			pprintf_prompt(current->whofrom, _("\n%s, whom you were challenging, has joined a match with %s.\n"), 
						   pp->name, player_globals.parray[p1].name);
			pprintf(p, _("Challenge from %s removed.\n"), player_globals.parray[current->whofrom].name);
			delete_pending(current);
		}else if (check_current_pend(current,-1,p1,-PEND_PARTNER)) {
			pprintf_prompt(current->whofrom, _("\n%s, whom you were challenging, has joined a match with %s.\n"), 
						   player_globals.parray[p1].name, pp->name);
			pprintf(p1, _("Challenge from %s removed.\n"),
			player_globals.parray[current->whofrom].name);
			delete_pending(current);
		}
  }
}
