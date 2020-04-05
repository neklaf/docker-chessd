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

#ifndef _PLAYERDB_H
#define _PLAYERDB_H

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>
#include <dbi/dbi.h>

#include "network.h"
#include "iset.h"
#include "lists.h"
#include "utils.h"

#define MAX_OBSERVE		30		/* max # of games one person can observe */
#define MAX_PLAN		10
#define MAX_FORMULA		9
#define MAX_CENSOR		50
#define MAX_NOTIFY		80
#define MAX_SIMUL		100
#define MAX_MESSAGES	40
#define MAX_INCHANNELS	16

#define PLAYER_EMPTY 	0
#define PLAYER_NEW 		1
#define PLAYER_INQUEUE 	2
#define PLAYER_LOGIN 	3
#define PLAYER_PASSWORD 4
#define PLAYER_PROMPT 	5

#define P_LOGIN		0
#define P_LOGOUT	1

#define SORT_BLITZ	0
#define SORT_STAND	1
#define SORT_ALPHA	2
#define SORT_WILD	3

struct statistics {
	// gabrielsan:
	// it will be necessary to have the style name once we have an
	// extensible system of game types
	char *style_name;

	// the usual
	int num, win, los, dra, rating, ltime, best, whenbest;
	double sterr;
};

struct simul_info_t {
	int numBoards;
	int onBoard;
	int num_wins, num_draws, num_losses;
	int boards[MAX_SIMUL];
};

#define PFLAG_REG 		0x1
#define PFLAG_OPEN 		0x2
#define PFLAG_ROPEN 	0x4
#define PFLAG_SIMOPEN 	0x8
#define PFLAG_FLIP 		0x10
#define PFLAG_ADMINLIGHT 0x20
#define PFLAG_RATED 	0x40
#define PFLAG_BLACKSIDE 0x80  /* not done; replacing side. */
#define PFLAG_LASTBLACK 0x100
#define PFLAG_PIN 		0x200
#define PFLAG_GIN 		0x400
#define PFLAG_AVAIL 	0x800 /* to be informed about who is available for games*/

// all of these flags are obsolete -->
#define PFLAG_PRIVATE 0x1000
#define PFLAG_JPRIVATE 0x2000
#define PFLAG_AUTOMAIL 0x4000
// <--

#define PFLAG_HELPME	0x8000

#define PFLAG_SHOUT 	0x10000
#define PFLAG_CSHOUT 	0x20000
#define PFLAG_TELL 		0x40000
#define PFLAG_KIBITZ 	0x80000
#define PFLAG_NOTIFYBY 	0x100000
#define PFLAG_PGN 		0x200000
#define PFLAG_BELL 		0x400000
#define PFLAG_HIDEINFO 	0x800000
#define PFLAG_TOURNEY 	0x1000000  /* Not coded yet. */
#define PFLAG_ADS 		0x2000000

// gabrielsan: some new custom flags... just temporary
#define PFLAG_TEAM			0x4000000
#define PFLAG_DEVELOPER		0x8000000
	// when this flag the user can call some developer commands to
	// debug the server

// user is under evaluation and should be treated as guest
#define PFLAG_UNDER_EVAL	0x10000000

// this flag is set only in the very first time the user logs in the server;
#define PFLAG_FIRST_TIME	0x20000000
#define PFLAG_INVISIBLE		0x40000000

/* Note: we're starting the last byte, assuming a long int has 4 bytes;
   If we run out, we probably should make a Flag1 and a Flag2. */

#define PFLAG_DEFAULT (PFLAG_OPEN | PFLAG_ROPEN | PFLAG_SHOUT \
                | PFLAG_CSHOUT | PFLAG_KIBITZ | PFLAG_HELPME  \
                | PFLAG_ADMINLIGHT)

/* PFLAG_SAVED will make a good mask. */
#define PFLAG_SAVED  (PFLAG_OPEN | PFLAG_ROPEN | PFLAG_RATED | PFLAG_BELL \
                      | PFLAG_PIN | PFLAG_GIN | PFLAG_AVAIL | PFLAG_PRIVATE \
                      | PFLAG_JPRIVATE | PFLAG_AUTOMAIL | PFLAG_MAILMESS \
                      | PFLAG_MAILMESS | PFLAG_SHOUT | PFLAG_CSHOUT \
                      | PFLAG_TELL | PFLAG_KIBITZ | PFLAG_NOTIFYBY | PFLAG_PGN \
					  | PFLAG_UNDER_EVAL | PFLAG_HELPME)

struct player {
	int chessduserid;
	int loading; /* samuelm (14jan04): when 1 adding aliases or members in lists
					                   does not update the sql database */

	/* This first block is not saved between logins */
	char *login;
	int socket;
	int status;
	int game;
	int opponent; /* Only valid if game is >= 0 */
	int side;     /* Only valid if game is >= 0 */
	int ftell; /* Are you forwarding tells?  -1 if not else who from */
	int logon_time;
	int last_command_time;
	int num_observe;
	int observe_list[MAX_OBSERVE];
	struct in_addr thisHost;
	int lastshout_a;
	int lastshout_b;
	struct simul_info_t *simul_info;
	int num_comments; /* number of lines in comments file */
	int partner;
	char *more_text;
	int kiblevel;
	int number_pend_from; /* not really necessary but are used to cut down */
	int number_pend_to;   /*   search - DAV */
	struct ivariables ivariables;
	char *interface;

	/* this is a dummy variable used to tell which bits are saved in the structure */
	unsigned not_saved_marker;

	int timeOfReg;
	int totalTime;
	unsigned Flags;
	char *name;
	char *passwd;
	char *fullName;
	char *emailAddress;
	char *prompt;
	char *busy;
	char *last_tell;
	int last_channel;
	char *last_opponent;
	int last_cmd_error;

	// gabrielsan: in the near future we will allow for new game types, so
	// it is better to have an array of stats
	struct statistics user_stats[7];

	// will soon be obsolete
	struct statistics s_stats;
	struct statistics b_stats;
	struct statistics w_stats;
	struct statistics l_stats;
	struct statistics bug_stats;

	//
	int d_time;
	int d_inc;
	int d_height;
	int d_width;
	int language;
	int style;
	int promote;
	int adminLevel;
	int availmin;
	int availmax;
	int num_plan;
	char *planLines[MAX_PLAN];
	int num_formula;
	char *formulaLines[MAX_FORMULA];
	char *formula;
	int num_white;
	int num_black;
	struct in_addr lastHost;
	int numAlias;
	struct alias_type *alias_list; /*_LEN(numAlias)*/
	int highlight;
	struct List *lists;
};

typedef struct textlist {
	char *text;
	int index;
	struct textlist *next;
} textlist;


int player_new(void);
void player_free(struct player *pp);

int player_count_messages(int p);
int player_clear(int p);
int player_remove(int p);
int player_read(int p, const char *name);
int player_save(int p, int addplayer);
int player_find(int fd);
int player_find_bylogin(const char *name);
int player_find_part_login(const char *name);
int player_find_byid(int userID);
int player_censored(int p, int p1);
int player_notified(int p, int p1);
void player_notify_departure(int p);
int player_notify_present(int p);
int player_notify(int p, char *note1, char *note2);
int player_notify_pin(int p);
int showstored(int p);
int player_count(int CountAdmins);
int player_idle(int p);
int player_ontime(int p);
void player_write_login(int p);
void player_write_logout(int p);
int player_lastdisconnect(int p);
int player_is_observe(int p, int g);
int player_add_observe(int p, int g);
int player_remove_observe(int p, int g);
int player_game_ended(int g);
int player_goto_board(int p, int board_num);
int player_goto_next_board(int p);
int player_goto_prev_board(int p);
int player_goto_simulgame_bynum(int p, int num);
int player_num_active_boards(int p);
int player_simul_over(int p, int g, int result);
int player_num_messages(int p);
int player_add_message(int top, int fromp, char *message);
void SaveTextListEntry(textlist **Entry, char *string, int n);
void ClearTextList(textlist *head);
int ForwardMsgRange(char *p1, int p, int start, int end);
int ClearMsgsBySender(int p, param_list param);
int player_show_messages(int p);
int ShowMsgsBySender(int p, param_list param);
int ShowMsgRange(int p, int start, int end);
int ClrMsgRange(int p, int start, int end);
int player_clear_messages(int p);
int player_search(int p, char *name);
int player_kill(char *name);
int player_rename(char *name, char *newname);
int player_reincarn(char *name, char *newname);
int player_add_comment(int p_by, int p_to, char *comment);
int player_show_comments(int p, int p1);
int player_ishead(int p);
int GetRating(struct player *p, int gametype);
double GetRD(struct player *p, int gametype);

int write_stats_to_db(int p, int gametype);
int get_player_name_byid(int playerID, char **player_name);
int com_finger(int p, param_list param);
int getChessdUserID(char *name);
int getChessdUserFieldInt(char *name, char *field);
char *getChessdUserField(char *name, char *field);
void AddPlayerLists(int p1, char *ptmp);
// send the whoseInfo's info to toWhom
int sendUserInfo(int whoseInfo, int toWhom);
// function used to notify other users of information change
int player_status_update(int p);

#endif /* _PLAYERDB_H */

