/*
   Copyright (c) 2004 Federal University of Parana

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

#ifndef _DBDBI_H
#define _DBDBI_H

#include <ctype.h>
#include <dbi/dbi.h>

// To cater for both PgSQL BOOLEAN ('f'/'t') and MySQL (use CHAR(1) instead),
// retrieve field as a string (%s) and use this macro to test that value.
#define GETBOOLEAN(str) ((str)[0]=='1' || toupper((str)[0])=='T')

// These are standard values for BOOLEAN and should work in most databases. 
// In MySQL these identifiers are stored as 0 or 1 in both string and numeric fields.
// In the libdbi query template, for BOOLEAN / CHAR(1) fields, use %s, without ''.
#define PUTBOOLEAN(flag) ((flag) ? "TRUE" : "FALSE")

typedef struct tdatabase_info {
	char *driver,
		 *host,
		 *port,
		 *dbname,
		 *username,
		 *password;
} tdatabase_info;

extern dbi_conn dbiconn; // global connection variable, used by all SQL code

/* the following functions are ONLY used by chessd_main.c
 * all other parts of the code use libdbi functions (dbi_*) directly.
 */

/**
 * Return the connection status
 * 0: no connection
 * 1: connected
 */

int sql_con_status();

/**
 * connects to database in a given host
 */
int sql_connect_host(tdatabase_info);

// pings the db connection and attempts to reconnect if it has gone
int sql_check_reconnect(void);

/**
 * Close the connection to the database.
 */
void sql_disconnect();

#endif

