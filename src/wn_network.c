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

#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __Linux__
  #include <linux/stddef.h>
#endif
#ifdef HAVE_SYS_FILIO_H
  #include <sys/filio.h>  // Solaris, for FIONBIO symbol
#endif
#ifdef HAVE_STRINGS_H
  #include <strings.h>  // for bcopy(), bzero()
#endif

#include "wn_network.h"
#include "globals.h"
#include "malloc.h"
#include "wn_command_network.h"
#include "chessd_locale.h"

#define WN_END_CHAR 	((unsigned char)0)
#define WN_ESCAP_CHAR 	((unsigned char)'\\')
#define WN_STUFF_CHAR 	((unsigned char)'#')

extern int errno;

//1. stuff the data
int stuff_stream(unsigned char* input, unsigned char** stuffed, int len)
{
	unsigned char *buf;
	int i,s;

	// temporary, because we still do not know the final string size
	buf = (unsigned char*) malloc(len * 2 * sizeof(unsigned char) +1);

	if (buf == NULL)
		return -1;

	for (i=0, s=0; i < len; i++) {
		switch (input[i]) {
		case WN_END_CHAR:
			buf[s++] = WN_STUFF_CHAR;
			break;
		case WN_STUFF_CHAR:
		case WN_ESCAP_CHAR:
			buf[s++] = WN_ESCAP_CHAR;
		default:
			buf[s++] = input[i];
		}
	}
	// make sure the buffer has an end character
	buf[s++] = WN_END_CHAR;

	*stuffed = malloc(sizeof(unsigned char) * s);
	if (*stuffed == NULL)
		return -1;

	strncpy((char*) *stuffed, (char*) buf, s);
	FREE(buf);

	return s; // ok, stuffed
}

//2. destuff the data
int destuff(unsigned char* stuffed)
{
	int i, paste_pos, osize, state = 0;

	// up to osize-1, because we do not care
	osize = strlen((char*) stuffed);
	paste_pos = 0;
	for (i=0; i < osize; i++) {
		switch (state) {
		case 0:
			switch(stuffed[i]) {
			case '\\':
				state = 1;
				break;
			case '#':
				stuffed[i] = '\0';
			default:
				stuffed[paste_pos++] = stuffed[i];
			}
			break;
		case 1:
			stuffed[paste_pos++] = stuffed[i];
			state = 0;
		break;
		}
	}
	// null char to crop the string...
	stuffed[paste_pos++] = 0;

	return paste_pos;
}

/* Index == fd, for sparse array, quick lookups! wasted memory :( */
static int wn_findConnection(int fd)
{
	if ( fd<0 || webnet_globals.con[fd]->status == NETSTAT_EMPTY)
		return -1;
	else
		return fd;
}

static void wn_set_sndbuf(int fd, int len)
{
	webnet_globals.con[fd]->sndbufpos = len;
	if (len < webnet_globals.con[fd]->sndbufsize)
		webnet_globals.con[fd]->sndbuf[len] = 0;
}

static int wn_net_addConnection(int fd, struct in_addr fromHost)
{
	int noblock = 1;

	/* possibly expand the connections array */
	if (fd >= webnet_globals.no_file) {
		int i;
		webnet_globals.con = (struct connection_t **)realloc(
				webnet_globals.con, (fd+1) * sizeof(struct connection_t *));

		for (i=webnet_globals.no_file;i<fd+1;i++) {
			webnet_globals.con[i] = (struct connection_t *)calloc(1,
									   sizeof(struct connection_t));
			webnet_globals.con[i]->status = NETSTAT_EMPTY;
		}
		webnet_globals.no_file = fd+1;
	}

	if (wn_findConnection(fd) >= 0) {
		d_printf( _("CHESSD: FD already in connection table!\n"));
		return -1;
	}
	if (ioctl(fd, FIONBIO, &noblock) == -1) {
		d_printf( _("Error setting nonblocking mode errno=%d\n"), errno);
	}
	webnet_globals.con[fd]->fd = fd;
	if (fd != 0)
		webnet_globals.con[fd]->outFd = fd;
	else
		webnet_globals.con[fd]->outFd = 1;
	webnet_globals.con[fd]->fromHost = fromHost;
	webnet_globals.con[fd]->status = NETSTAT_CONNECTED;
	webnet_globals.con[fd]->timeseal = 0;
	webnet_globals.con[fd]->timeseal_init = 1;
	webnet_globals.con[fd]->time = 0;
	webnet_globals.con[fd]->numPending = 0;
	webnet_globals.con[fd]->inBuf[0] = 0;
	webnet_globals.con[fd]->processed = 0;
	webnet_globals.con[fd]->outPos = 0;
	if (webnet_globals.con[fd]->sndbuf == NULL) {
#ifdef DEBUG
		d_printf( _("CHESSD: nac(%d) allocating sndbuf.\n"), fd);
#endif
		webnet_globals.con[fd]->sndbufpos = 0;
		webnet_globals.con[fd]->sndbufsize = MAX_STRING_LENGTH;
		webnet_globals.con[fd]->sndbuf = malloc(MAX_STRING_LENGTH);
	} else {
#ifdef DEBUG
		d_printf( _("CHESSD: nac(%d) reusing old sndbuf size %d pos %d.\n"), fd, webnet_globals.con[fd]->sndbufsize, webnet_globals.con[fd]->sndbufpos);
#endif
	}
	webnet_globals.con[fd]->state = 0;
	webnet_globals.numConnections++;

#ifdef DEBUG
	d_printf( _("CHESSD: fd: %d connections: %d  descriptors: %d \n"),
			fd, webnet_globals.numConnections, getdtablesize());
	/* sparky 3/13/95 */
#endif

	/// debug
//	d_printf("got a connection to 5051 (%d)\n", fd);
//	fflush(stderr);

	return 0;
}

static int wn_remConnection(int fd)
{
	int which, i;
	if ((which = wn_findConnection(fd)) < 0) {
		d_printf( _("wn_remConnection: Couldn't find fd to close.\n"));
		return -1;
	}
	webnet_globals.numConnections--;
	webnet_globals.con[fd]->status = NETSTAT_EMPTY;
	if (webnet_globals.con[fd]->sndbuf == NULL) {
		d_printf( _("CHESSD: remcon(%d) SNAFU, this shouldn't happen.\n"), fd);
	} else {
		if (webnet_globals.con[fd]->sndbufsize > MAX_STRING_LENGTH) {
			webnet_globals.con[fd]->sndbufsize = MAX_STRING_LENGTH;
			webnet_globals.con[fd]->sndbuf =
				realloc(webnet_globals.con[fd]->sndbuf, MAX_STRING_LENGTH);
		}
		if (webnet_globals.con[fd]->sndbufpos) /* didn't send everything, bummer */
			wn_set_sndbuf(fd, 0);
	}

	/* see if we can shrink the con array */
	for (i=fd;i<webnet_globals.no_file;i++) {
		if (webnet_globals.con[i]->status != NETSTAT_EMPTY)
			return 0;
	}

	/* yep! shrink it */
	for (i=fd;i<webnet_globals.no_file;i++) {
		FREE(webnet_globals.con[i]->sndbuf);
		FREE(webnet_globals.con[i]);
	}
	webnet_globals.no_file = fd;
	webnet_globals.con = (struct connection_t **)realloc(
		webnet_globals.con, webnet_globals.no_file *
							sizeof(struct connection_t *));

	return 0;
}

static void wn_net_flushme(int which)
{
	struct connection_t *wg = webnet_globals.con[which];
  int sent = send(wg->outFd, wg->sndbuf, wg->sndbufpos, 0);

  if (sent == -1) {
    if (errno != EPIPE)		/* EPIPE = they've disconnected */
      d_printf( _("CHESSD: wn_net_flushme(%d) couldn't send, errno=%d.\n"),
			  	which, errno);

    wn_set_sndbuf(which, 0);
  } else {
    wg->sndbufpos -= sent;
    if (wg->sndbufpos)
      memmove(wg->sndbuf, wg->sndbuf + sent, wg->sndbufpos);
  }
  if (wg->sndbufsize > MAX_STRING_LENGTH && wg->sndbufpos < MAX_STRING_LENGTH) {
    /* time to shrink the buffer */
    wg->sndbuf = realloc(wg->sndbuf, MAX_STRING_LENGTH);
    wg->sndbufsize = MAX_STRING_LENGTH;
  }
  wn_set_sndbuf(which, wg->sndbufpos);
}

static void wn_net_flush_all_connections(void)
{
	int which;
	fd_set writefds;
	struct timeval to;

	FD_ZERO(&writefds);
	for (which = 0; which < webnet_globals.no_file; which++) {
		if (webnet_globals.con[which]->status == NETSTAT_CONNECTED &&
		    webnet_globals.con[which]->sndbufpos){
			FD_SET(webnet_globals.con[which]->outFd, &writefds);
		}
	}

	to.tv_usec = 0;
	to.tv_sec = 0;
	select(webnet_globals.no_file, NULL, &writefds, NULL, &to);
	for (which = 0; which < webnet_globals.no_file; which++) {
		if (FD_ISSET(webnet_globals.con[which]->outFd, &writefds)) {
			wn_net_flushme(which);
		}
	}
}

static void wn_net_flush_connection(int fd)
{
  int which;
  fd_set writefds;
  struct timeval to;

  if (((which = wn_findConnection(fd)) >= 0) &&
	  (webnet_globals.con[which]->sndbufpos)) {
    FD_ZERO(&writefds);
    FD_SET(webnet_globals.con[which]->outFd, &writefds);
    to.tv_usec = 0;
    to.tv_sec = 0;
    select(webnet_globals.no_file, NULL, &writefds, NULL, &to);
    if (FD_ISSET(webnet_globals.con[which]->outFd, &writefds)) {
      wn_net_flushme(which);
    }
  }
}

static int wn_sendme(int which, char *str, int len)
{
	int i, count;
	fd_set writefds;
	struct timeval to;
	unsigned char* stuffed_data = NULL;
	unsigned char* start;

//gabrielsan : debug
#if 0
	d_printf("I'll send this: \n");
	for (i=0; i < len; i++) {
		d_printf("%d ", str[i]);
	}
#endif
	// before sending, stuff the string
	if ((len = stuff_stream((unsigned char*) str, &stuffed_data, len)) < 0) {
		FREE(stuffed_data);

		// ops, string not stuffed!
		return 0;
	}

	count = len;
	start = stuffed_data;

	while ((i = (
		(webnet_globals.con[which]->sndbufsize -
				 webnet_globals.con[which]->sndbufpos) < len)
			? (webnet_globals.con[which]->sndbufsize -
				webnet_globals.con[which]->sndbufpos)
			:  len) > 0) {

		memmove(webnet_globals.con[which]->sndbuf +
				webnet_globals.con[which]->sndbufpos,
				stuffed_data, i);

		webnet_globals.con[which]->sndbufpos += i;
		if (webnet_globals.con[which]->sndbufpos ==
				webnet_globals.con[which]->sndbufsize) {

			FD_ZERO(&writefds);
			FD_SET(webnet_globals.con[which]->outFd, &writefds);
			to.tv_usec = 0;
			to.tv_sec = 0;
			select(webnet_globals.no_file, NULL, &writefds, NULL, &to);
			if (FD_ISSET(webnet_globals.con[which]->outFd, &writefds)) {
				wn_net_flushme(which);
			} else {
				/* time to grow the buffer */
				webnet_globals.con[which]->sndbufsize += MAX_STRING_LENGTH;
				webnet_globals.con[which]->sndbuf =
					realloc(webnet_globals.con[which]->sndbuf,
							webnet_globals.con[which]->sndbufsize);
			}
		}
		stuffed_data += i;
		len -= i;
	}

	FREE(start);

	wn_set_sndbuf(which, webnet_globals.con[which]->sndbufpos);

	return count;
}

/*
 * -1 for an error other than EWOULDBLOCK.
 * Doesn't send anything unless the buffer fills, output waits until
 * flushed
*/
/* width here is terminal width = width var + 1 at presnt) */
int wn_net_send_string(int fd, char *str, int len)
{
	int which;

	if ((which = wn_findConnection(fd)) < 0) {
		return -1;
	}

// just send as it is; no format is done now
	wn_sendme(which, str, len);

	return 0;
}

/*
 *	read it up to the "WN_END_CHAR" character; return 0 if found, -1 if
 *	not (not complete)
 */
static int wn_read_data(char *com, int who)
{
	struct connection_t *wg = webnet_globals.con[who];
	unsigned char *start, *s;
	int hw, howmany, state, fd, pending, i;

	state = wg->state;
	if ((state == 2) || (state > 4)) {
		d_printf( _("CHESSD: state screwed for webnet_globals.con[%d], this is a bug.\n"), who);
		state = 0;
	}
	s = start = wg->inBuf;
	pending = wg->numPending;
	fd = wg->fd;

	// receive the data; notice how it is concatenated AFTER the data
	// already in the buffer
	howmany = recv(fd, start + pending, MAX_STRING_LENGTH - 1 - pending, 0);

// gabrielsan: debug
#if 0
	d_printf("\nDATA RECEIVED: %s\n", start + pending);
	d_printf("I got this: \n");
	for (i=pending; i < howmany; i++) {
		d_printf("%d ", (start + pending)[i]);
	}
	d_printf("\n");
#endif

	if (howmany == 0)		/* error: they've disconnected */
		return -1;
	else if (howmany == -1) {
		if (errno != EWOULDBLOCK)	/* some other error */
			return -1;
		else if (wg->processed)	/* nothing new and nothing old */
			return 0;
		else			/* nothing new, but some unprocessed old */
			howmany = 0;
	}

	if (wg->processed)
		s += pending;
	else
		howmany += pending;

	// so far, no errors found; check whether data is complete
	for (i = pending; (i < howmany) && (*s != WN_END_CHAR); s++, i++)
		;

	// if it stopped at an end character, then this is the end
	if (howmany > 0 && *s == WN_END_CHAR) {
		// data is complete;
		*s = '\0';
		hw = destuff(start + pending);

#if 0
		d_printf("Destuffed data: \n");
		for (i=pending; i < hw; i++) {
			d_printf("%d ", (start + pending)[i]);
		}
		d_printf("\n");
#endif

		memcpy(com, start + pending, hw);
		com[hw] = 0;
		if (hw)
			bcopy(s + 1, start + pending, hw);

		// the state is reseted because the command was ready to
		// process
		wg->state = 0;
		wg->numPending = 0;
		wg->inBuf[0] = 0;
		wg->processed = 0;
		wg->outPos = 0;
		return 1;
	}

	// not complete
	*s = '\0';

	// whatever was before the pending, have already been processed
	destuff(start + pending);
	howmany = strlen((char*) start + pending);

	wg->state = state;
	wg->numPending = howmany;
	wg->inBuf[howmany] = 0;
	wg->processed = 1;
	if (wg->numPending == (MAX_STRING_LENGTH - 1)) {
		/* buffer full */
		strcpy(com, (char*) start);
		wg->state = 0;
		wg->numPending = 0;
		wg->inBuf[0] = 0;
		wg->processed = 0;
		return 1;
	}
	return 0;
}

int wn_net_init(int port)
{
  int opt;
  struct sockaddr_in serv_addr;
  struct linger lingeropt;

  webnet_globals.no_file = 0;
  webnet_globals.con = NULL;

  /* Open a TCP socket (an Internet stream socket). */
  if ((webnet_globals.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    d_printf( _("CHESSD: can't open stream socket\n"));
    return -1;
  }
  /* Bind our local address so that the client can send to us */
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  /** added in an attempt to allow rebinding to the port **/

  opt = 1;
  setsockopt(webnet_globals.sockfd, SOL_SOCKET, SO_REUSEADDR,
		  	 (char *) &opt, sizeof(opt));
  opt = 1;
  setsockopt(webnet_globals.sockfd, SOL_SOCKET, SO_KEEPALIVE,
		  	 (char *) &opt, sizeof(opt));
  lingeropt.l_onoff = 0;
  lingeropt.l_linger = 0;
  setsockopt(webnet_globals.sockfd, SOL_SOCKET, SO_LINGER,
		  	 (char *) &lingeropt, sizeof(lingeropt));

/*
#ifdef DEBUG
  opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_DEBUG, (char *)&opt, sizeof(opt));
#endif
*/

  if (bind(webnet_globals.sockfd, (struct sockaddr*) &serv_addr,
		   sizeof(serv_addr)) < 0) {
    d_printf( _("CHESSD: can't bind local address.  errno=%d\n"), errno);
    return -1;
  }
  opt = 1;
  ioctl(webnet_globals.sockfd, FIONBIO, &opt);
  fcntl(webnet_globals.sockfd, F_SETFD, 1);
  fcntl(webnet_globals.sockfd, O_NONBLOCK, 1);
  listen(webnet_globals.sockfd, 5);
  return 0;
}

void wn_net_close(void)
{
	int i;

	for (i = 0; i < webnet_globals.no_file; i++) {
		if (webnet_globals.con[i]->status != NETSTAT_EMPTY)
		wn_net_close_connection(webnet_globals.con[i]->fd);
	}
}

void wn_net_close_connection(int fd)
{
  if (webnet_globals.con[fd]->status == NETSTAT_CONNECTED)
    wn_net_flush_connection(fd);
  else
    d_printf(_("Trying to close an unconnected socket?!?!\n"));

  if (!wn_remConnection(fd)) {
    if (fd > 2 && close(fd) < 0)
		d_printf( _("Couldn't close socket %d - errno %d\n"), fd, errno);
  } else
    d_printf(_("Failed to remove connection (Socket %d)\n"), fd);

  // gabrielsan: debug
  // d_printf("disconnected %d\n", fd);
}

static struct in_addr wn_net_connected_host(int fd)
{
	static struct in_addr ip_zero;
	int which;

	if ((which = wn_findConnection(fd)) < 0) {
		d_printf( _("CHESSD: FD not in connection table!\n"));
		return ip_zero;
	}
	return webnet_globals.con[which]->fromHost;
}


void wn_select_loop(void )
{
	char com[MAX_STRING_LENGTH];
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof(struct sockaddr_in);
	int fd, loop, nfound, lineComplete;
	fd_set readfds;
	struct timeval to;
	int timeout = 0;

	/* we only want to get signals here. This tries to
	   ensure a clean shutdown on 'kill' */
	unblock_signal(SIGTERM);
	block_signal(SIGTERM);

	while ((fd = accept(webnet_globals.sockfd,
					    (struct sockaddr *) & cli_addr, &cli_len)) != -1)
	{
		if (wn_net_addConnection(fd, cli_addr.sin_addr)) {
			d_printf( _("CHESSD is full.  fd = %d.\n"), fd);
			close(fd);
		} else {
			if (fd >= webnet_globals.no_file)
				d_printf(_("CHESSD (ngc2): Out of range fd!\n"));
			else {
				fcntl(fd, F_SETFD, 1);
				wn_process_new_connection(fd, wn_net_connected_host(fd));
			}
		}
	}
	if (errno != EWOULDBLOCK)
		d_printf( _("CHESSD: Problem with accept().  errno=%d\n"), errno);

	wn_net_flush_all_connections();

	FD_ZERO(&readfds);
	for (loop = 0; loop < webnet_globals.no_file; loop++)
		if (webnet_globals.con[loop]->status != NETSTAT_EMPTY)
			FD_SET(webnet_globals.con[loop]->fd, &readfds);

	to.tv_usec = 0;
	to.tv_sec = timeout;
	nfound = select(webnet_globals.no_file, &readfds, NULL, NULL, &to);
	for (loop = 0; loop < webnet_globals.no_file; loop++) {
		if (webnet_globals.con[loop]->status != NETSTAT_EMPTY) {
			fd = webnet_globals.con[loop]->fd;
		more_commands:
			lineComplete = wn_read_data(com, fd);

			if (lineComplete == 0)	/* partial data: do nothing with it */
				continue;

			if (lineComplete > 0) {	/* complete data: process it */
				if (wn_process_input(fd, com) != COM_LOGOUT) {
					wn_net_flush_connection(fd);
					goto more_commands;
				}
			}
			/* Disconnect anyone who gets here */
			wn_process_disconnection(fd);
			wn_net_close_connection(fd);
		}
	}
}
