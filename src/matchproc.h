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


#ifndef _MATCHPROC_H
#define _MATCHPROC_H

extern const char *colorstr[];

struct delta_rating_info {
	int win;
	int los;
	int dra;
	double sterr;
};

#include "pending.h"
int create_new_match(int g, int white_player, int black_player,
                             int wt, int winc, int bt, int binc,
                             int rated, char *category, char *board,
                             int white, int simul);
int accept_match(struct pending *pend, int p, int p1);
int com_match(int p, param_list param);
int com_rmatch(int p, param_list param);

#endif /* _MATCHPROC_H */
