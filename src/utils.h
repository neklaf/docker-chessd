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

#ifndef _UTILS_H
#define _UTILS_H

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>

#define MAX_WORD_SIZE 1024

/* Maximum length of an output line */
#define MAX_LINE_SIZE 1024

/* Maximum size of a filename */
#ifdef FILENAME_MAX
#  define MAX_FILENAME_SIZE FILENAME_MAX
#else
#  define MAX_FILENAME_SIZE 1024
#endif

#define STRINGIFY(arg) #arg  // FIXME: is there a standard macro for this?

#define FlagON(VAR, FLAG)			((VAR) |= (FLAG))
#define FlagOFF(VAR, FLAG)			((VAR) &= ~(FLAG))
#define CheckFlag(VAR, FLAG)		((VAR) & (FLAG))
#define BoolCheckFlag(VAR, FLAG)	(CheckFlag((VAR), (FLAG)) != 0)
#define ToggleFlag(VAR, FLAG)		((VAR) ^= (FLAG))
#define SetFlag(VAR, FLAG, VALUE)	((VALUE) ? FlagON(VAR, FLAG) : FlagOFF(VAR, FLAG))

#define PFlagON(WHO, FLAG)			FlagON(player_globals.parray[WHO].Flags, FLAG)
#define PFlagOFF(WHO, FLAG)			FlagOFF(player_globals.parray[WHO].Flags, FLAG)
#define CheckPFlag(WHO, FLAG)		CheckFlag(player_globals.parray[WHO].Flags, FLAG)
#define BoolCheckPFlag(WHO, FLAG)	BoolCheckFlag(player_globals.parray[WHO].Flags, FLAG)
#define TogglePFlag(WHO, FLAG)		ToggleFlag(player_globals.parray[WHO].Flags, FLAG)
#define SetPFlag(WHO, FLAG, VALUE)	((VALUE) ? PFlagON(WHO, FLAG) : PFlagOFF(WHO, FLAG))

/* se == server event */
#define seSERVER_START      0
#define seSERVER_SHUTDOWN   1
#define seSERVER_RELOAD     2
#define seUSER_COUNT_CHANGE 3
#define seGAME_COUNT_CHANGE 4

/* au == active user */
#define auINSERT 	0
#define auDELETE 	1

void DeleteActiveGame(char* white_login, char* black_login);
void InsertServerEvent( int serverEvent, int int_parameter );
void ClearServerActiveUsers();
void ActiveUser(int operation, int chessduserid, char* username, char *fromip, int where);
void InsertActiveGame(int type, int white_id, int black_id, char* white_login, char* black_login);

char *eatword(char *str);
char *eatwhite(char *str);
char *eattailwhite(char *str);
char *nextword(char *str);

int mail_string_to_address(char *addr, char *subj, char *str);
int mail_string_to_user(int p, char *subj, char *str);

int pcommand(int p, const char *comstr, ...);
int pprintf(int p, const char *format, ...);
int pprintf_highlight(int p, const char *format, ...);
int pprintf_more(int p, const char *format, ...);
int psprintf_highlight(int p, char *s, const char *format, ...);
int pprintf_prompt(int p, const char *format, ...);
void send_prompt(int p) ;
int pprintf_noformat(int p, const char *format, ...);
void Bell(int p);
int psend_raw_file(int p, char *dir, char *file);
int psend_file(int p, const char *dir, const char *file);
int psend_logoutfile(int p, char *dir, char *file);
int pmore_text(int p);

char *stolower(char *str);
int alphastring(char *str);
int numberstring(char *str);
int printablestring(const char *str);

char *hms_desc(int t);
char *hms(int t, int showhour, int showseconds, int spaces);
unsigned tenth_secs(void);
int untenths(unsigned tenths);
unsigned tenths(int regular_time);
char *tenth_str(unsigned t, int spaces);

int truncate_file(char *file, int lines);
int lines_file(char *file);
int file_has_pname(char *fname, char *plogin);
char *file_wplayer(char *fname);
char *file_bplayer(char *fname);
char *dotQuad(struct in_addr a);
int file_exists(char *fname);
char *ratstr(int rat);
char *ratstrii(int rat, int p);
int search_directory(const char *dir, const char *filter, char **buffer, int buffersize);
int display_directory(int p, char **buffer, int count);
void CenterText(char *target, const char *text, int width, int pad);
void block_signal(int signum);
void unblock_signal(int signum);
int file_copy(const char *src, const char *dest);

size_t strnlen(const char *s, size_t n);
const char *strday(time_t *t);
const char *strltime(time_t *clock);
const char *strgtime(time_t *clock);

void d_printf(const char *fmt, ...);
int dprintf(int fd, const char *format, ...);
void log_printf(const char *fmt, ...);

int file_save(const char *fname, void *data, size_t length);
char *fd_load(int fd, size_t *size);
char *file_load(const char *fname, size_t *size);
char *fd_gets(char *line, size_t maxsize, int fd);
FILE *fopen_p(const char *fmt, const char *mode, ...);
FILE *fopen_s(const char *fname, const char *mode);

size_t strlcpy(char *d, const char *s, size_t bufsize);
#ifndef HAVE_STRNDUP
  char *strndup(const char *s, size_t n);
#endif
#ifndef HAVE_ASPRINTF
  int asprintf(char **strp, const char *fmt, ...);
#endif
#ifndef HAVE_VASPRINTF
  int vasprintf(char **strp, const char *fmt, va_list ap);
#endif


void ClearServerActiveUsersAndGames();
double toggle_clock();
int plsdir(int p, char *dname);
int matchpass(const char *stored, char *given);

#endif /* _UTILS_H */
