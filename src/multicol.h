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

#ifndef _MULTICOL_H
#define _MULTICOL_H

typedef struct multicol
{
  int arraySize;
  int num;
  char **strArray;
} multicol;


struct multicol *multicol_start(int maxArray);
int multicol_store(multicol * m, char *str);
int multicol_store_sorted(multicol * m, char *str);
int multicol_pprint(multicol * m, int player, int cols, int space);
int multicol_end(multicol * m);

#endif /* _MULTICOL_H */
