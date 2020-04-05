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

/* Revision history:
   name				yy/mm/dd	Change
   hersco and Marsalis         	95/07/24	Created
*/

#ifndef _TALKPROC_H
#define _TALKPROC_H

#define MAX_CHANNELS 256
#define TELL_TELL 0
#define TELL_SAY 1
#define TELL_WHISPER 2
#define TELL_KIBITZ 3
#define TELL_CHANNEL 4
#define TELL_LTELL 5

#define ADMIN_CHANNEL 	0
#define HELP_CHANNEL 	1
#define SHOUT_CHANNEL	2
#define CSHOUT_CHANNEL	3

#define MASTER_CHANNEL 	252
#define HELPER_CHANNEL 	253
#define MANAGER_CHANNEL 254


extern const char * gl_tell_name[];

int com_shout(int p, param_list param);
int com_cshout(int p, param_list param);
int com_it(int p, param_list param);
int com_ptell(int p, param_list param);
int com_whisper(int p, param_list param);
int com_kibitz(int p, param_list param);
int com_tell(int p, param_list param);
int com_ltell(int p, param_list param);
int com_xtell(int p, param_list param);
int com_say(int p, param_list param);
int com_inchannel(int p, param_list param);
int com_messages(int p, param_list param);
int com_forwardmess(int p, param_list param);
int com_clearmessages(int p, param_list param);
int com_znotify(int p, param_list param);
int com_qtell(int p, param_list param);

#endif /* _TALKPROC_H */
