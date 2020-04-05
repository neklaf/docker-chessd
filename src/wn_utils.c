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

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "globals.h"
#include "playerdb.h"
#include "wn_utils.h"
#include "wn_network.h"

int wn_vpprintf(int p, char *format, va_list ap)
{
	char *tmp;
	int retval = vasprintf(&tmp, format, ap);

	if(retval != -1){
		wn_net_send_string(player_globals.parray[p].socket, tmp, retval+1 );
		free(tmp);
	}
	return retval;
}

int wn_pprintf(int p, char *format, ...)
{
	int retval;
	va_list ap;
	
	va_start(ap, format);
	retval = wn_vpprintf(p, format, ap);
	va_end(ap);
	return retval;
}

int wn_data(int p, int count, ...)
{
	va_list ap;
	int *data, *ip, i;

	ip = data = malloc(sizeof(int) * (count + 2));
	if (data == NULL)
		return -1;

	va_start(ap, count);
	for (i=0; i < count; i++)
		*(ip++) = va_arg(ap, int);
	va_end(ap);
	*ip = 0;

// pretend the int array is a char array and send it away
	wn_net_send_string(player_globals.parray[p].socket,
					   (char*)data, sizeof(int) * (count));
	free(data);
	return 0;
}
