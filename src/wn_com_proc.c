#include "config.h"

#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "command.h"
#include "comproc.h"
#include "playerdb.h"
#include "gamedb.h"
#include "chessd_locale.h"
#include "wn_com_proc.h"
#include "wn_utils.h"
#include "wn_command_network.h"
#include "malloc.h"
#include "command_network.h"
#include "gameproc.h"
#include "utils.h"

int wn_com_nothing(int player, int* params)
{
    return SCR_OK; // just ok, nothing here
}

int wn_com_incoming_msgs(int player, int* params)
{
    // this command uses 3 parameters:
    //  0: who issued the command (sender ID)
    //  1: who is the target (destiny ID)
    //  2: what is the message ID

    int p;
    char *player_name;
    const char *notification =
		"\nYou have a new message in the site from '%s'\n";

    // check whether player is around
    if ((p = player_find_byid(params[1])) > -1) {
        // player is connected; notify a new message came from 'blah' to him;
        // get the sender name and message subject;
        get_player_name_byid(params[0], &player_name);
        pprintf_prompt(p, _(notification), player_name);
        FREE(player_name);
    }

    return SCR_OK;
}

int wn_com_kick(int player, int* params)
{
	int p;
	int fd;
	char *player_name;

	// who is kicking? the nuked guy should know who kicked his ass
	get_player_name_byid(params[0], &player_name);

	// look for the player
	if ((p = player_find_byid(params[1])) > -1)
		return SCR_NOT_LOGGED;

	pprintf_prompt(p, _("\n\n**** You have been kicked out by %s! ****\n\n"),
				   player_name);

	// fine, let's kick/nuke the bastard
	fd = player_globals.parray[p].socket;
	process_disconnection(fd);
	net_close_connection(fd);

	return SCR_OK;
}

int wn_com_adjudicate(int player, int* params)
{
	// this command uses 4 parameters:
	// 	0: who issued the command (adminID)
	// 	1: which game is the target (gameID)
	// 	2: what is the chosen resolution
	// 	3: who won (if applicabble

	struct game *gg;
	char result_string[1024], res[1024];
	int wp, bp, g;
	int p1=-1, p2=-1;
	char *white_name, *black_name;

	// for now, it won't be possible to adjudicate a game in progress;
	// the players should first choose to adjourn and then request the
	// adjudication

	g = game_new();
	if (findGameByID(g, params[1]) < -1) {
		// the game does not exist
		game_remove(g);
		return SCR_OBJ_NOT_EXIST;
	}
	gg = &game_globals.garray[g];

	// copy the names, so we can dispose of the game
	white_name = strdup(gg->white_name);
	black_name = strdup(gg->black_name);

	// if the players are not around, load'em
	if ((wp = player_find_bylogin(white_name)) <= 0) {
		p1 = player_new();
		player_read(p1, white_name);
		gg->white = p1;
	}
	else
		gg->white = wp;

	if ((bp = player_find_bylogin(black_name)) <= 0) {
		p2 = player_new();
		player_read(p2, black_name);
		gg->black = p2;
	}
	else
		gg->black = bp;

	// find the players; if they're connected, it will be used to tell
	// about the adjudication

	// gabrielsan:
	// bugfix: the game_ended removed the game before the game was
	// actually used for the following information
	// the fix was to copy the names, the only information really
	// used..

	// now, for the types of adjudication
	// NOTICE: the game_ended removes the game 'g'
	game_ended(g, params[3], params[2]);

	strcpy(res, "");
	switch (params[2]) {
	case END_ADJWIN:
		sprintf(res, _("'%s' declared the winner."),
				params[3]==WHITE ? white_name : black_name);
		break;

	case END_ABORT:
		sprintf(res, _("Game was aborted. No rating changed."));
		break;

	case END_ADJDRAW:
		sprintf(res, _("Game was declared a draw."));
		break;
	}
	d_printf("CHESSD: adjudication: strings built\n");

	sprintf(result_string,
			_("Game %s (white) vs %s (black), Result: %s\n"),
			white_name, black_name, res);

	// remove the players if they were loaded just for this operation
	if (p1 > -1)
		player_remove(p1);
	if (p2 > -1)
		player_remove(p2);
	d_printf("CHESSD: adjudication: players removed\n");

	// tell the results to the players
	if ((wp = player_find_bylogin(white_name)) > -1)
		pprintf_prompt(wp, _("Your game against '%s' was adjudicated\n"
							 "Decision:\n\t%s\n"
							 "See the message in the site for details.\n"),
					   black_name, result_string);
	d_printf("CHESSD: adjudication: player %s informed\n", white_name);

	if ((bp = player_find_bylogin(black_name)) > -1)
		pprintf_prompt(bp, _("Your game against '%s' was adjudicated\n"
							 "Decision: \n\t%s\n"
							 "See the message in the site for details.\n"),
					   white_name, result_string);
	d_printf("CHESSD: adjudication: player %s informed\n", black_name);

	FREE(white_name);
	FREE(black_name);

	// remove the other from the db
	game_delete_by_id(params[1]);
	d_printf("CHESSD: adjudication: game removed from base OK\n");

///	game_remove(g);
//	d_printf("CHESSD: adjudication: game removed OK\n");

	return SCR_OK;
}

int wn_com_list_update(int player, int* params)
{
	// TODO: waiting for new definition of lists

	d_printf("read from website: LIST UPDATE, params %d %d\n",
			params[0], params[1]);

	return SCR_OK;
}

int wn_com_read_msgs(int player, int* params)
{
	// this command uses 3 parameters:
	// 	0: who issued the command (who sent ID)
	// 	1: who is the target (who read ID)
	// 	2: what is the message ID

	int p;
	char *player_name;
	const char *notification =
		"\n'%s' has just read your message on the site.\n";

	// check whether sender player is around
	if ((p = player_find_byid(params[0])) > -1) {
		// player is connected; notify a new message came from 'blah' to him;

		// get the receiver name
		get_player_name_byid(params[1], &player_name);

		pprintf_prompt(p, _(notification), player_name);
		FREE(player_name);
	}

	return SCR_OK;
}


int wn_com_logout(int player, int *params)
{
	return COM_LOGOUT;
}

int wn_com_get_usr_level(int player, int* params)
{
	// command parameter is actually a string, the username
	// sends the xml with user's level
	wn_data(player, 1, SRO_XML);
	wn_pprintf(player, "<adminlevel>%s</adminlevel>",
			   getChessdUserFieldInt((char*)params, "adminlevel"));
	return SCR_OK;
}

int wn_com_get_usr_id(int player, int* params)
{
	// command parameter is actually a string, the username
	// sends the xml with user's id
	wn_data(player, 1, SRO_XML);
	wn_pprintf(player, "<chessduserid>%d</chessduserid>", 
			   getChessdUserID((char*)params)); // -1 if unknown
	return SCR_OK;
}
