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

/*
 *	the WN network is actually a backdoor network infrastructure which
 *	will be used to receive commands from the web site;
 *
 */

#ifndef WN_NETWORK_H
#define WN_NETWORK_H

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>
#include "command.h"
//#include "network.h"

int wn_net_send_string(int fd, char *str, int len);
int wn_net_init(int port);
void wn_net_close(void);
void wn_net_close_connection(int fd);
void wn_select_loop(void );

#endif /* WN_NETWORK_H */
