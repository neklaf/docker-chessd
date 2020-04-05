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

/* Configure file locations in this include file. */

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#define DEFAULT_CONFIG SYSCONFDIR "/chessd.conf"

#define DEFAULT_PORT 		5000
#define DEFAULT_WN_PORT 	5051

#define DEFAULT_DB_DRIVER 	"pgsql" // or "mysql"
#define DEFAULT_DB_HOST 	"localhost"
#define DEFAULT_DB_PORT     "5432" // PostgreSQL default. MySQL default = 3306
#define DEFAULT_DATABASE	"chessd"
#define DEFAULT_DB_USER		"chessd"
#define DEFAULT_DB_USER_PWD ""

/* Which is the default language for help files, see variable.h for the
    current available settings */

#define LANG_DEFAULT      LANG_ENGLISH

/* CONFIGURE THESE: Locations of the data, players, and games directories */
/* These must be absolute paths because some mail daemons may be called */
/* from outside the pwd */

#define MAX_ADVERTS 24
/* These small files are printed when users log in
   They can be used for advertisements or to promote server features
   MAX_ADVERTS = the number of adverts you have
   They should be put in the DEFAULT_ADVERTS directory
   and be labelled with the numbers 0 - MAX_ADVERTS-1 inclusive
   Eg if MAX_ADVERTS is 3 then the files are 0,1,2

   If you do not want to use the adverts feature set it to -1
*/

/* define the directory that will be the root directory of FICS */
#define FICSROOT "./"

#define LIB_DIR           LIBDIR
#define ADVERT_DIR        PKGDATADIR "/data/adverts"
#define MESS_DIR          PKGDATADIR "/data/messages"
#define INDEX_DIR         PKGDATADIR "/data/index"
#define HELP_DIR          PKGDATADIR "/data/help"
#define INFO_DIR          PKGDATADIR "/data/info"
#define AHELP_DIR         PKGDATADIR "/data/admin"
#define USCF_DIR          PKGDATADIR "/data/uscf"
#define STATS_DIR         PKGDATADIR "/data/stats"
#define SPOOL_DIR         PKGDATADIR "/spool/mail.XXXXXX"
#define ADJOURNED_DIR     PKGDATADIR "/games/adjourned"
#define HISTORY_DIR       PKGDATADIR "/games/history"
#define JOURNAL_DIR       PKGDATADIR "/games/journal"
#define BOARD_DIR         PKGDATADIR "/data/boards"
#define LISTS_DIR         PKGDATADIR "/data/lists"
#define BOOK_DIR          PKGDATADIR "/data/book"
#define MESS_FULL         PKGDATADIR "/data/messages/full"
#define MESS_FULL_UNREG   PKGDATADIR "/data/messages/full_unreg"
#define USAGE_DIR         PKGDATADIR "/data/usage"

/* all of these can be overridden using the 'aconfig' command */
#define DEFAULT_MAX_PLAYERS 5000  /* total players */
#define DEFAULT_MAX_PLAYERS_UNREG 4000 /* number of players before denying access to guests */
#define DEFAULT_MAX_USER_LIST_SIZE 100
#define DEFAULT_MAX_ALIASES 20
#define DEFAULT_RATING 0
#define DEFAULT_RD 350
#define DEFAULT_TIME 2
#define DEFAULT_INCREMENT 12
#define DEFAULT_MAX_SOUGHT 1000
#define DEFAULT_IDLE_TIMEOUT 3600
#define DEFAULT_LOGIN_TIMEOUT 300
#define DEFAULT_GUEST_PREFIX_ONLY 0

/* Where the standard ucb mail program is */

#define MAILPROGRAM       "/usr/bin/mail"

/// log file : gabrielsan (2004/09/22)
#define LOG_FILE	PREFIXDIR "/log/chessd_interaction.log"

/* SENDMAILPROG is a faster and more reliable means of sending mail if
   defined.  Make sure your system mailer agent is defined here properly
   for your system with respect to name, location and options.  These may
   differ significatly depending on the type of system and what mailer is
   installed  */

/* The following works fine for SunOS4.1.X with berkeley sendmail  */

/* #define SENDMAILPROG      "/usr/lib/sendmail -t" */

#endif /* _CONFIGURATION_H */
