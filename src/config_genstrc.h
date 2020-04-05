/*
  global configuration module for chessd, genstruct interface

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

#ifndef _CONFIG_GENSTRC_H
#define _CONFIG_GENSTRC_H

#include <stddef.h>
// #include "genparser.h" gabrielsan: TODO
#include "command.h"

#define MAX_CFGVAR_SIZE 80

#define NUM_CFG_STRS 10
#define NUM_CFG_INTS 14


/*GENSTRUCT*/ struct config_t {
        char **strs;
        int *ints;
};

struct config_t config;

enum config_strs_enum {
        default_prompt = 0,
        guest_login,
        head_admin,
        head_admin_email,
        registration_address,
        server_hostname,
        server_location,
        server_name,
		webinterface_url,
		site_since_when
};


enum config_ints_enum {
        default_time = 0,
        default_rating,
        default_rd,
        default_increment,
        idle_timeout,
        guest_prefix_only,
        login_timeout,
        max_aliases,
        max_players,
        max_players_unreg,
        max_sought,
        max_user_list_size,
		site_visitor_count,
		site_max_online_users
};


int config_open(void);
int com_aconfig(int p, param_list param);

#endif /* _CONFIG_GENSTRC_H */
