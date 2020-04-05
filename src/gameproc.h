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


#ifndef _GAMEPROC_H
#define _GAMEPROC_H

#include "gamedb.h"
#include "playerdb.h"

#define MAX_SIMPASS 3


void getnotavailmess(int p, char* message);
void announce_avail(int p);
void announce_notavail(int p);
void game_ended(int g, int winner, int why);
int pIsPlaying (int p);
void timeseal_update_clocks(struct player *pp, struct game *gg);
void process_move(int p, char *command);
int com_resign(int p, param_list param);
int com_draw(int p, param_list param);
int com_pause(int p, param_list param);
int com_unpause(int p, param_list param);
int com_abort(int p, param_list param);
int com_flag(int p, param_list param);
int com_adjourn(int p, param_list param);
int com_takeback(int p, param_list param);
int com_switch(int p, param_list param);
int com_time(int p, param_list param);
int com_ptime(int p, param_list param);
int com_boards(int p, param_list param);
int com_simmatch(int p, param_list param);
int com_goboard(int p, param_list param);
int com_simnext(int p, param_list param);
int com_simprev(int p, param_list param);
int com_simgames(int p, param_list param);
int com_simpass(int p, param_list param);
int com_simabort(int p, param_list param);
int com_simallabort(int p, param_list param);
int com_simadjourn(int p, param_list param);
int com_simalladjourn(int p, param_list param);
int com_moretime(int p, param_list param);

#endif /* _GAMEPROC_H */
