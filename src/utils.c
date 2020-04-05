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
#include <sys/timeb.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>

#include <unistd.h>

#include "globals.h"
#include "playerdb.h"
#include "utils.h"
#include "multicol.h"
#include "command_network.h"
#include "configuration.h"
#include "chessd_locale.h"
#include "malloc.h"
#include "dbdbi.h"
#include "crypt.h"
#include "crypt-md5.h"

void ActiveUser(int operation, int chessduserid, char *username, char *fromip, int where)
{
	dbi_result res = NULL;
	const char* SQL_insert =
		"INSERT INTO ActiveUser (userid, username, fromip, sincewhen, activewhere) "
						"VALUES (%i, '%s', '%s', NOW(), %i)";
	const char* SQL_delete =
		"DELETE FROM ActiveUser WHERE userid = %i AND activewhere = %i";

	switch (operation) {
	case auINSERT:
		res = dbi_conn_queryf(dbiconn, SQL_insert, 
							  chessduserid, username, fromip, where);
		break;
	case auDELETE:
		res = dbi_conn_queryf(dbiconn, SQL_delete, chessduserid, where);
		break;
	}
	if(res) dbi_result_free(res);
}

void DeleteActiveGame(char* white_login, char* black_login)
{
	dbi_result res;
	// FIXME: watch out for case sensitivity of logins in following query?
	const char* sql = "DELETE FROM ActiveGames "
					  "WHERE whiteplayername = '%s' AND blackplayername = '%s'";

	if (white_login != NULL && black_login != NULL)
	{
		d_printf("CHESSD: removing game %s x %s\n", white_login, black_login);
		res = dbi_conn_queryf(dbiconn, sql, white_login, black_login);
		if(res)
			dbi_result_free(res);
		else
			d_printf("CHESSD: failed to remove game %s x %s\n",
					 white_login, black_login);
	}
}

void InsertActiveGame(int type, int white_id, int black_id, 
					  char* white_login, char* black_login)
{
  const char* sql =
  	"INSERT INTO ActiveGames (timeOfStart, gameTypeID, whitePlayerID, "
  							 "blackPlayerID, whitePlayerName, blackPlayerName) "
					 "VALUES (NOW(), %i, %i, %i, '%s', '%s')";
  dbi_result_free( dbi_conn_queryf( dbiconn, sql, type, white_id, 
  									black_id, white_login, black_login ) );
}

void InsertServerEvent( int serverEvent, int int_parameter )
{
	const char* sql =
		"INSERT INTO ServerEvent (eventType, int_parameter, eventTime) "
						 "VALUES (%i, %i, NOW())";
	dbi_result_free(dbi_conn_queryf(dbiconn, sql, serverEvent, int_parameter));
}

void ClearServerActiveUsersAndGames()
{
	dbi_result_free(dbi_conn_queryf(dbiconn, "DELETE FROM ActiveUser "
  											 "WHERE activewhere >= 0"));
	dbi_result_free(dbi_conn_queryf(dbiconn, "DELETE FROM ActiveGames"));
}

char *eatword(char *str)
{
  while (*str && !isspace(*str))
    str++;
  return str;
}

char *eatwhite(char *str)
{
  while (*str && isspace(*str))
    str++;
  return str;
}

char *eattailwhite(char *str)
{
  int len;
  if (str == NULL)
    return NULL;

  len = strlen(str);
  while (len > 0 && isspace(str[len - 1]))
    len--;
  str[len] = '\0';
  return (str);
}

char *nextword(char *str)
{
  return eatwhite(eatword(str));
}

/* Process a command for a user */
int pcommand(int p, const char *comstr, ...)
{
	struct player *pp = &player_globals.parray[p];
	char *tmp;
	int retval;
	int current_socket = pp->socket;
	va_list ap;

	va_start(ap, comstr);
	vasprintf(&tmp, comstr, ap);
	va_end(ap);
	retval = process_input(current_socket, tmp);
	free(tmp);
	
	if (retval == COM_LOGOUT) {
		process_disconnection(current_socket);
		net_close_connection(current_socket);
	}
	return retval;
}

static int vpprintf(int p, int do_formatting, const char *format, va_list ap)
{
	struct player *pp = &player_globals.parray[p];
	char *tmp = NULL;
	int retval;
	retval = vasprintf(&tmp, format, ap);

	// never format when using the extended protocol
	if (pp->ivariables.ext_protocol)
		do_formatting = 0;

	if (retval != 0) {
		if (pp->d_width == 0) {
			pp->d_width = 79;
			pp->d_height = 27;
		}

		net_send_string(pp->socket, tmp, do_formatting, pp->d_width + 1);
	}
	FREE(tmp);

	return retval;
}

int pprintf(int p, const char *format,...)
{
	int retval;
	va_list ap;
	
	va_start(ap, format);
	retval = vpprintf(p, 1, format, ap);
	va_end(ap);
	return retval;
}

static void pprintf_dohightlight(int p)
{
	struct player *pp = &player_globals.parray[p];
	if (pp->highlight & 0x01)
		pprintf(p, "\033[7m");
	if (pp->highlight & 0x02)
		pprintf(p, "\033[1m");
	if (pp->highlight & 0x04)
		pprintf(p, "\033[4m");
	if (pp->highlight & 0x08)
		pprintf(p, "\033[2m");
}

int pprintf_highlight(int p, const char *format, ...)
{
	struct player *pp = &player_globals.parray[p];
	int retval;
	va_list ap;

	pprintf_dohightlight(p);
	va_start(ap, format);
	retval = vpprintf(p, 1, format, ap);
	va_end(ap);
	if (pp->highlight)
		pprintf(p, "\033[0m");
	return retval;
}

static void sprintf_dohightlight(int p, char *s)
{
	struct player *pp = &player_globals.parray[p];
	if (pp->highlight & 0x01)
		strcat(s, "\033[7m");
	if (pp->highlight & 0x02)
		strcat(s, "\033[1m");
	if (pp->highlight & 0x04)
		strcat(s, "\033[4m");
	if (pp->highlight & 0x08)
		strcat(s, "\033[2m");
}

/*
  like pprintf() but with paging for long messages
*/
int pprintf_more(int p, const char *format, ...)
{
	struct player *pp = &player_globals.parray[p];
	char *s = NULL;
	va_list ap;
	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	FREE(pp->more_text);
	pp->more_text = s;

	return pmore_text(p);
}

int psprintf_highlight(int p, char *s, const char *format, ...)
{
	struct player *pp = &player_globals.parray[p];
	int retval;
	va_list ap;

	va_start(ap, format);
	if (pp->highlight) {
		sprintf_dohightlight(p, s);
		retval = vsprintf(s + strlen(s), format, ap);
		strcat(s, "\033[0m");
	} else
		retval = vsprintf(s, format, ap);
	va_end(ap);
	return retval;
}

int pprintf_prompt(int p, const char *format, ...)
{
	int retval;
	va_list ap;
	
	va_start(ap, format);
	retval = vpprintf(p, 1, format, ap);
	va_end(ap);
	send_prompt(p);
	return retval;
}

/* send a prompt to p */
void send_prompt(int p)
{
	struct player *pp = &player_globals.parray[p];
	const char *prompt = pp->prompt;
	pprintf(p, "%s%s", prompt, isspace(prompt[strlen(prompt)-1]) ? "" : " ");
}

int pprintf_noformat(int p, const char *format, ...)
{
	int retval;
	va_list ap;
	
	va_start(ap, format);
	retval = vpprintf(p, 0, format, ap);
	va_end(ap);
	return retval;
}

void Bell (int p)
{
	if (CheckPFlag(p, PFLAG_BELL))
		pprintf (p, "\a");
	return;
}

int psend_raw_file(int p, char *dir, char *file)
{
	struct player *pp = &player_globals.parray[p];
	FILE *fp;
	char tmp[MAX_LINE_SIZE];
	int num;

	fp = fopen_p("%s/%s", "r", dir, file);

	if (!fp)
		return -1;
	while ((num = fread(tmp, sizeof(char), MAX_LINE_SIZE - 1, fp)) > 0) {
		tmp[num] = '\0';
		net_send_string(pp->socket, tmp, 1, pp->d_width + 1);
	}
	fclose(fp);
	return 0;
}

/*
  load a file into memory
*/
char *file_load(const char *fname, size_t *size)
{
    int fd;
    char *p;

    if (!fname || !*fname) return NULL;

    fd = open(fname,O_RDONLY);
    if (fd == -1) return NULL;

    p = fd_load(fd, size);
    close(fd);

    return p;
}

/*
  this is like fgets() but operates on a file descriptor
*/
char *fd_gets(char *line, size_t maxsize, int fd)
{
	struct pollfd polling;
    char c;
    unsigned n = 0, res;

    while (n < (maxsize-1) && (read(fd, &c, 1) == 1)) {
        line[n++] = c;
        if (c == '\n') break;
    }

    line[n] = 0;

	// Are there any characters remaining? DAMN! Throw them away.
	// Otherwise, the next player will execute it (BAD)
	polling.fd = fd;
	polling.events = 1;

	for(;;) {
		res = poll(&polling, 1, 100);
		if (res > 0 && polling.revents == POLLIN)
		{
			while (read(fd, &c, 1) >= 1 && c != '\n')
				;
		}
		else
			break;
	}

    return n ? line : NULL;
}

/*
  load a file into memory from a fd.
*/
char *fd_load(int fd, size_t *size)
{
    struct stat sbuf;
    char *p;

    if (fstat(fd, &sbuf) != 0) return NULL;

    p = (char *)malloc(sbuf.st_size+1);
    if (!p) return NULL;
        if (pread(fd, p, sbuf.st_size, 0) != sbuf.st_size) {
        free(p);
        return NULL;
    }
    p[sbuf.st_size] = 0;

    if (size) *size = sbuf.st_size;

    return p;
}

/*
  send a file a page at a time
*/
int psend_file(int p, const char *dir, const char *file)
{
	struct player *pp = &player_globals.parray[p];
	char *fname;

	if (strstr(file, "..")) {
		pprintf(p,_("Trying to be tricky, are we?\n"));
		return 0;
	}

	if (dir)
		asprintf(&fname,"%s/%s",dir,file);
	else
		fname = strdup(file);

	FREE(pp->more_text);
	pp->more_text = file_load(fname, NULL);
	if (!pp->more_text)
		return -1;

	free(fname);

	return pmore_text(p);
}

/*
 * Marsalis added on 8/27/95 so that [next] does not
 * appear in the logout process for those that have
 * a short screen height.  (Fixed due to complaint
 * in Bug's messages).
 */
int psend_logoutfile(int p, char *dir, char *file)
{
	return psend_file(p, dir, file);
}

/*
   continue with text from a previous command
*/
int pmore_text(int p)
{
	struct player *pp = &player_globals.parray[p];
	int lcount = pp->d_height - 1;

	if (!pp->more_text) {
		pprintf(p, _("There is no more.\n"));
		return -1;
	}

	while (pp->more_text && lcount--) {
		char *s = strndup(pp->more_text, strcspn(pp->more_text, "\n")+1);
		int len = strlen(s);
		net_send_string(pp->socket, s, 1, pp->d_width + 1);
		s = strdup(pp->more_text+len);
		FREE(pp->more_text);
		if (s[0])
			pp->more_text = s;
		else
			free(s);
	}

	if (pp->more_text)
		pprintf(p, _("Type [next] to see next page.\n"));

	return 0;
}

char *stolower(char *str)
{
	int i;

	if (!str)
		return NULL;
	for (i = 0; str[i]; i++)
		str[i] = tolower(str[i]);
	return str;
}

/*static int safechar(int c)
{
	return (isprint(c) && !strchr(">!&*?/<|`$;", c));
}
*/
/*
static int safestring(char *str)
{
	int i;

	if (!str)
		return 1;
	for (i = 0; str[i]; i++) {
		if (!safechar(str[i]))
			return 0;
	}
	return 1;
}
*/

int alphastring(char *str)
{
	if (!str || !str[0])
		return 1; // NULL pointer, or empty string, is ok
	if(!isalpha(str[0]))
		return 0; // doesn't begin with a letter
	while(*++str) {
		if (!(isalpha(*str) || isdigit(*str) || *str == '_'))
			return 0; // neither letter, digit, nor _
	}
	return 1;
}

int numberstring(char *str)
{
	int i;

	for (i=0; str[i]; i++)
		if (!isdigit(str[i]))
			return 0;
	return 1;
}

int printablestring(const char *str)
{
	int i;
 	return 1; // BUG?
	if (!str)
		return 1;
	for (i = 0; str[i]; i++) {
		if ((!isprint(str[i])) && (str[i] != '\t') && (str[i] != '\n'))
			return 0;
	}
	return 1;
}

char *hms_desc(int t)
{
static char tstr[80];
int days, hours, mins, secs;

    days  = (t / (60*60*24));
    hours = ((t % (60*60*24)) / (60*60));
    mins  = (((t % (60*60*24)) % (60*60)) / 60);
    secs  = (((t % (60*60*24)) % (60*60)) % 60);
    if ((days==0) && (hours==0) && (mins==0)) {
      sprintf(tstr, ngettext("%d sec", "%d secs", secs), secs);
    } else if ((days==0) && (hours==0)) {
      sprintf(tstr, "%d %s, %d %s",
              mins, ngettext("min", "mins", mins),
              secs, ngettext("sec", "secs", secs)
              );

    } else if (days==0) {
      sprintf(tstr, "%d %s, %d %s, %d %s",
              hours, ngettext("hour", "hours", hours),
              mins, ngettext("min", "mins", mins),
              secs, ngettext("sec", "secs", secs)
              );
    } else {
            sprintf(tstr, _("%d %s, %d %s, %d %s and %d %s"),
              days, ngettext("day", "days", days),
              hours, ngettext("hour", "hours", hours),
              mins, ngettext("min", "mins", mins),
              secs, ngettext("sec", "secs", secs)
                    );
    }
    return tstr;
}

char *hms(int t, int showhour, int showseconds, int spaces)
{
  static char tstr[20];
  char tmp[10];
  int h, m, s;

  h = t / 3600;
  t = t % 3600;
  m = t / 60;
  s = t % 60;
  if (h || showhour) {
    if (spaces)
      sprintf(tstr, "%d : %02d", h, m);
    else
      sprintf(tstr, "%d:%02d", h, m);
  } else {
    sprintf(tstr, "%d", m);
  }
  if (showseconds) {
    if (spaces)
      sprintf(tmp, " : %02d", s);
    else
      sprintf(tmp, ":%02d", s);
    strcat(tstr, tmp);
  }
  return tstr;
}

/* This is used only for relative timeing since it reports seconds since
 * about 5:00pm on Feb 16, 1994
 *
 */
unsigned tenth_secs(void)
{
  struct timeval tp;
  struct timezone tzp;

  gettimeofday(&tp, &tzp);
/* .1 seconds since 1970 almost fills a 32 bit int! So lets subtract off
 * the time right now */

  // changed my mind: internally, use the crap representation; it will
  // be fixed when stored in the db

  // TODO : turning it back to
  return ((tp.tv_sec - 331939277) * 10L) + (tp.tv_usec / 100000);
}

unsigned tenths(int regular_time)
{
	return (regular_time - 331939277) * 10;
}

/* This is to translate tenths-secs time back into 1/1/70 time in full
 * seconds, because vek didn't read utils.c when he programmed new ratings.
   1 sec since 1970 fits into a 32 bit int OK.
*/
int untenths(unsigned tenths)
{
	return (tenths / 10 + 331939277 + 0xffffffff / 10 + 1);
}

char *tenth_str(unsigned t, int spaces)
{
//	return hms(t/10, 0, 1, spaces);
	return hms((t + 5) / 10, 0, 1, spaces);	/* Round it */
}

int file_has_pname(char *fname, char *plogin)
{
	return !strcmp(file_wplayer(fname), plogin)
		|| !strcmp(file_bplayer(fname), plogin);
}

char *file_wplayer(char *fname)
{
  static char tmp[MAX_FILENAME_SIZE];
  char *ptr;

  strcpy(tmp, fname);
  ptr = strrchr(tmp, '-');
  if (!ptr)
    return "";
  *ptr = '\0';
  return tmp;
}

char *file_bplayer(char *fname)
{
  char *ptr;

  ptr = strrchr(fname, '-');
  if (!ptr)
    return "";
  return ptr + 1;
}

/*
  make a human readable IP
*/
char *dotQuad(struct in_addr a)
{
	return inet_ntoa(a);
}

int file_exists(char *fname)
{
  FILE *fp;

  fp = fopen_s(fname, "r");
  if (!fp)
    return 0;
  fclose(fp);
  return 1;
}

/*
 *   like fopen() but doesn't allow opening of filenames containing '..'
 */
FILE *fopen_s(const char *fname, const char *mode)
{
	return fopen_p("%s", mode, fname);
}

/*
  like fopen() but uses printf style arguments for the file name
*/
FILE *fopen_p(const char *fmt, const char *mode, ...)
{
    char *s = NULL;
    FILE *f;
    va_list ap;
    va_start(ap, mode);
    vasprintf(&s, fmt, ap);
    va_end(ap);
    if (!s) return NULL;
    if (strstr(s, "..")) {
        free(s);
        return NULL;
    }
    f = fopen(s, mode);
    free(s);
    return f;
}


char *ratstr(int rat)
{
  static int on = 0;
  static char tmp[20][10];

  if (on == 20)
    on = 0;
  if (rat)
    sprintf(tmp[on], "%4d", rat);
  else
    strcpy(tmp[on], "----");

  on++;
  return tmp[on - 1];
}

char *ratstrii(int rat, int p)
{
  static int on = 0;
  static char tmp[20][10];

  if (on == 20)
    on = 0;
  if (rat)
    sprintf(tmp[on], "%4d", rat);
  else
    strcpy(tmp[on], CheckPFlag(p, PFLAG_REG) ? "----" : "++++");

  on++;
  return tmp[on - 1];
}

struct t_tree {
  struct t_tree *left, *right;
  char name;			/* Not just 1 char - space for whole name */
};				/* is allocated.  Maybe a little cheesy? */

struct t_dirs {
  struct t_dirs *left, *right;
  time_t mtime;			/* dir's modification time */
  struct t_tree *files;
  char name;			/* ditto */
};

static char **t_buffer = NULL; /* pointer to next space in return buffer */
static int t_buffersize = 0;	/* size of return buffer */

/* fill t_buffer with anything matching "want*" in file tree t_tree */
static void t_sft(const char *want, struct t_tree *t)
{
  if (t) {
    int cmp = strncmp(want, &t->name, strlen(want));
    if (cmp <= 0)		/* if want <= this one, look left */
      t_sft(want, t->left);
    if (t_buffersize && (cmp == 0)) {	/* if a match, add it to buffer */
      t_buffersize--;
      *t_buffer++ = &(t->name);
    }
    if (cmp >= 0)		/* if want >= this one, look right */
      t_sft(want, t->right);
  }
}

/* delete file tree t_tree */
static void t_cft(struct t_tree **t)
{
  if (*t) {
    t_cft(&(*t)->left);
    t_cft(&(*t)->right);
    free(*t);
    *t = NULL;
  }
}

/* make file tree for dir d */
static void t_mft(struct t_dirs *d)
{
  DIR *dirp;
  struct dirent *dp;
  struct t_tree **t;

  if ((dirp = opendir(&(d->name))) == NULL) {
    d_printf( _("CHESSD:mft() couldn't opendir.\n"));
  } else {
    while ((dp = readdir(dirp))) {
      t = &d->files;
      if (dp->d_name[0] != '.') {	/* skip anything starting with . */
	while (*t) {
	  if (strcmp(dp->d_name, &(*t)->name) < 0) {
	    t = &(*t)->left;
	  } else {
	    t = &(*t)->right;
	  }
	}
	*t = malloc(sizeof(struct t_tree) + strlen(dp->d_name)+1);
	(*t)->right = (*t)->left = NULL;
	strcpy(&(*t)->name, dp->d_name);
      }
    }
    closedir(dirp);
  }
}

int search_directory(const char *dir, const char *filter, char **buffer, int buffersize)
{
/* dir = directory to search
   filter = what to search for
   buffer = where to store pointers to matches
   buffersize = how many pointers will fit inside buffer */

  static struct t_dirs *ramdirs = NULL;
  struct t_dirs **i;
  int cmp;
  static char nullify = '\0';
  struct stat statbuf;

  t_buffer = buffer;
  t_buffersize = buffersize;

  if (!stat(dir, &statbuf)) {
    if (filter == NULL)		/* NULL becomes pointer to null string */
      filter = &nullify;
    i = &ramdirs;
    while (*i) {			/* find dir in dir tree */
      cmp = strcmp(dir, &(*i)->name);
      if (cmp == 0)
		break;
      else
		i = cmp < 0 ? &(*i)->left : &(*i)->right;
    }
    if (!*i) {				/* if dir isn't in dir tree, add him */
      *i = malloc(sizeof(struct t_dirs) + strlen(dir)+1);
      (*i)->left = (*i)->right = NULL;
      (*i)->files = NULL;
      strcpy(&(*i)->name, dir);
    }
    if ((*i)->files && (*i)->mtime != statbuf.st_mtime) /* delete any obsolete file tree */
		t_cft(&(*i)->files);

    if ((*i)->files == NULL) {		/* if no file tree for him, make one */
      (*i)->mtime = statbuf.st_mtime;
      t_mft(*i);
    }
    t_sft(filter, (*i)->files);		/* finally, search for matches */
  }
  return (buffersize - t_buffersize);
}

/* print sorted directory listing
 * Toby Thain, http://sourceforge.net/users/qu1j0t3/
 */

int comparestr(char **a, char **b)
{
  return strcmp(*a,*b);
}

int plsdir(int p, char *dname)
{
  DIR *dirp;
  struct dirent *dp;
  char **ents;
  int i,n;
  
  dirp = opendir(dname);
  if(!dirp) return 0;
  
  /* count entries */
  n = 0;
  while( readdir(dirp) )
    ++n;

  if( (ents = malloc(n * sizeof(char*))) ){
    /* get entry pointers */
    rewinddir(dirp);
    for ( n=0 ; (dp = readdir(dirp)) ; )
      if (dp->d_name[0] != '.')
        ents[n++] = dp->d_name;

    qsort(ents, n, sizeof(char*), (void*)comparestr);
  
    for( i=0 ; i<n ; ++i )
      pprintf(p, "%s\n", ents[i]);
	
    free(ents);
  } /* else silently fail on out of memory (FIXME) */

  closedir(dirp);
  return n;
}

int display_directory(int p, char **buffer, int count)
/* buffer contains 'count' string pointers */
{
	struct player *pp = &player_globals.parray[p];
	int i;
	multicol *m = multicol_start(count);

	for (i = 0; (i < count); i++)
		multicol_store(m, *buffer++);
	multicol_pprint(m, p, pp->d_width, 1);
	multicol_end(m);
	return (i);
}

void CenterText (char *target, const char *text, int width, int pad)
{
  int left, len;
  char *format;

  len = strlen(text);
  left = (width + 1 - len) / 2;    /* leading blank space. */

  if (pad)
    asprintf (&format, "%%%ds%%-%ds", left, width-left);  /* e.g. "%5s%-10s" */
  else
    asprintf (&format, "%%%ds%%s", left);    /* e.g. "%5s%s" */
  sprintf (target, format, "", text);

  FREE(format);

  return;
}

/* block a signal */
void block_signal(int signum)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,signum);
	sigprocmask(SIG_BLOCK,&set,NULL);
}

/* unblock a signal */
void unblock_signal(int signum)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,signum);
	sigprocmask(SIG_UNBLOCK,&set,NULL);
}


int file_copy(const char *src, const char *dest)
{
  int fd1, fd2, n;
  char buf[1024];

  fd1 = open(src, O_RDONLY);
  if (fd1 == -1) return -1;

  unlink(dest);
  fd2 = open(dest, O_WRONLY|O_CREAT|O_EXCL, 0644);
  if (fd2 == -1) {
    close(fd1);
    return -1;
  }

  while ((n = read(fd1, buf, sizeof(buf))) > 0) {
    if (write(fd2, buf, n) != n) {
      close(fd2);
      close(fd1);
      unlink(dest);
      return -1;
    }
  }

  close(fd1);
  close(fd2);
  return 0;
}

#ifndef HAVE_DPRINTF
int dprintf(int fd, const char *format, ...)
{
	va_list ap;
	char *ptr = NULL;
	int ret = 0;

	va_start(ap, format);
	vasprintf(&ptr, format, ap);
	va_end(ap);

	if (ptr) {
		ret = write(fd, ptr, strlen(ptr));

		free(ptr);
	}

	return ret;
}
#endif

/* day as a string */
const char *strday(time_t *t)
{
	struct tm *stm = localtime(t);
	static char tstr[100];

	strftime(tstr, sizeof(tstr), "%a %b %e", stm);
	return tstr;
}

static const char *strtime(struct tm * stm, short gmt)
{
	static char tstr[100];

	strftime(tstr, sizeof(tstr), gmt ? "%a %b %e, %H:%M GMT %Y" 
									 : "%a %b %e, %H:%M %Z %Y", stm);
	return tstr;
}

const char *strltime(time_t *clock)
{
	struct tm *stm = localtime(clock);
	return strtime(stm, 0);
}

const char *strgtime(time_t *clock)
{
	struct tm *stm = gmtime(clock);
	return strtime(stm, 1);
}

/* useful debug utility */
void d_printf(const char *fmt, ...)
{
	va_list ap;
	time_t t = time(NULL);
	fprintf(stderr,"%s ", strltime(&t));
	va_start(ap, fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
}

void log_printf(const char *fmt, ...)
{
	va_list ap;
	time_t t = time(NULL);
	static FILE *log_file = NULL;

	if (log_file == NULL) {
		// open the log file
		log_file = fopen(LOG_FILE, "a");

		// if no log file can be opened, use the stderr
		if (log_file == NULL) {
			log_file = stderr;
			d_printf(_("CHESSD: log file \"" LOG_FILE "\" could not be created\n"
					   "CHESSD: using the standard error output\n"));
		}
	}

	fprintf(log_file ,"%s ", strltime(&t));

	fprintf(stderr, "LOG: %s ", strltime(&t));
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
        va_end(ap);
        va_start(ap, fmt);
	vfprintf(log_file, fmt, ap);
	va_end(ap);
	fflush(log_file);
	fflush(stderr);
}

/*
 * function used to measure time
 *
 */
double toggle_clock()
{
	static struct timeb start, end;
	double spent;
	static int dostart = 1;

	if (dostart) {
		ftime(&start);

		dostart = 0;
		return 0;
	}
	else {
		ftime(&end);

		dostart = 1;
		spent = (end.time + end.millitm/1000.0) -
				(start.time + start.millitm/1000.0);
	}
	return spent;
}

#ifndef HAVE_STRLCPY
/**
 * Like strncpy but does not 0 fill the buffer and always null
 * terminates.
 *
 * @param bufsize is the size of the destination buffer.
 *
 * @return index of the terminating byte.
 **/
 size_t strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (bufsize <= 0) return 0;
	if (len >= bufsize) len = bufsize-1;
	memcpy(d, s, len);
	d[len] = 0;
	return ret;
}
#endif

#ifndef HAVE_STRLCAT
/**
 * Like strncat() but does not 0 fill the buffer and always null
 * terminates.
 *
 * @param bufsize length of the buffer, which should be one more than
 * the maximum resulting string length.
 **/
 size_t strlcat(char *d, const char *s, size_t bufsize)
{
	size_t len1 = strlen(d);
	size_t len2 = strlen(s);
	size_t ret = len1 + len2;

	if (len1+len2 >= bufsize) {
		len2 = bufsize - (len1+1);
	}
	if (len2 > 0) {
		memcpy(d+len1, s, len2);
		d[len1+len2] = 0;
	}
	return ret;
}
#endif

#ifndef HAVE_STRNDUP
// e.g. not found on Solaris
char *strndup(const char *s, size_t n){
	char *p;
	size_t len = strlen(s);
	if(len>n) len = n;
	if( (p = malloc(len+1)) ){
		memcpy(p, s, len);
		p[len] = 0;
	}
	return p;
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...){
	va_list ap;
	int n;
	
	va_start(fmt, ap);
	n = vasprintf(strp, fmt, ap);
	va_end(ap);
	return n;
}
#endif

#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap){
	va_list aq;
	int n;
	char *dst;

	va_copy(aq, ap);
	// call once, printing nothing, to get a character count
	n = vsnprintf(NULL, 0, fmt, ap);
	// don't call va_end() here (caller must - see man vsprintf)
	// now allocate buffer of right size, and print into it
	if( (dst = malloc(n+1)) )
		vsnprintf(dst, n, fmt, aq);
	va_end(aq);
	*strp = dst;
	return n;
}
#endif

// this routine knows about two password formats: a 32-char hex MD5 string
// (for initial database passwords)
// and the original 'salted' chessd_crypt() format (28 chars),
// which is also used when password is changed.

int matchpass(const char *stored, char *given){
	char salt[3];

	if (strlen(stored) > 4) {
		salt[0] = stored[3];
		salt[1] = stored[4];
		salt[2] = '\0';
	}
	return (strlen(stored) == 32 && match_md5(given, stored))
		|| !strcmp(chessd_crypt(given,salt), stored);
}

