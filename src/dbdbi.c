/*
   Copyright (C) 2006 Toby Thain, http://sourceforge.net/users/qu1j0t3/

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

#include <stdio.h>
#include <string.h>

#include <dbi/dbi.h>

#include "utils.h"
#include "dbdbi.h"

dbi_conn dbiconn = NULL;
static int connstatus = 0;

int sql_con_status()
{
	return connstatus;
}

static void sql_err_handler(dbi_conn conn, void *userdata)
{
	const char *err;

	dbi_conn_error(conn, &err);
	d_printf("libdbi error: %s\n",err);
}

int sql_check_reconnect(void)
{
	if(connstatus && !dbi_conn_ping(dbiconn)){
		connstatus = dbi_conn_connect(dbiconn) >= 0;
	    if (!connstatus)
			d_printf("sql_check_reconnect: Database connection lost, and reconnection failed.\n");
	}
	return connstatus;
}

int sql_connect_host(tdatabase_info dbinfo)
{
	if(!connstatus){
		if(!dbiconn){
		    dbi_initialize(NULL);
		    dbiconn = dbi_conn_new(dbinfo.driver);
	    }
	    if(dbiconn){
	    	dbi_conn_error_handler(dbiconn, sql_err_handler, NULL);
		    if(dbinfo.host[0])     dbi_conn_set_option(dbiconn, "host", dbinfo.host);
		    if(dbinfo.port[0])     dbi_conn_set_option(dbiconn, "port", dbinfo.port);
		    if(dbinfo.username[0]) dbi_conn_set_option(dbiconn, "username", dbinfo.username);
		    if(dbinfo.password[0]) dbi_conn_set_option(dbiconn, "password", dbinfo.password);
		    dbi_conn_set_option(dbiconn, "dbname", dbinfo.dbname);
		    // For debugging. Available in libdbi 0.8.2:
		    dbi_conn_set_option_numeric(dbiconn, "LogQueries", 1);
		
			connstatus = dbi_conn_connect(dbiconn) >= 0;
		    if (!connstatus)
				d_printf("sql_connect_host: Connection to database '%s' failed.\n", dbinfo.dbname);
		}else 
			d_printf("sql_connect_host: dbi_conn_new() returned NULL\n");
	}else
		d_printf("sql_connect_host: already connected\n");
	return connstatus;
}

/**
 * Close the connection to the database.
 * This is currently only called on server termination, so it is final.
 */
void sql_disconnect()
{
	if(dbiconn) dbi_conn_close(dbiconn);
	dbi_shutdown();
	dbiconn = NULL;
	connstatus = 0;
}
