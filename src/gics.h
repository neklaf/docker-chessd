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

/*
 * FICS-Extensions used at GICS chess.unix-ag.uni-kl.de 5000
 * (c) 1995/1996 by Ulrich Schlechte <Ulrich.Schlechte@ipp.tu-clausthal.de>
 *		    Klaus Knopper <Knopper@unix-ag.uni-kl.de>
 *
 *
*/


#ifndef GICS_H
#define GICS_H

#include "command.h"
// #include "genparser.h"	gabrielsan: TODO

/* Needed for ustat-funktion */
/*GENSTRUCT*/ struct userstat_type {
  int  users[48];
  int  usermax;
  int usermaxtime;
  int logins;
  int games;
  int gamemax;
  int gamemaxtime;
};


void init_userstat(void);
void save_userstat(void);
int com_ping(int p, param_list param);
void game_save_playerratio(char *file, char *Opponent, int Result, int rated);
//int com_pstat(int p, param_list param);
#endif /* GICS_H */
