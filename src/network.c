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

// undo: %s/net_globals./net_globals\./g
// redo: %s/net_globals\./net_globals./g

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#ifdef __Linux__
  #include <linux/stddef.h>
#endif
#ifdef HAVE_SYS_FILIO_H
  #include <sys/filio.h>  // Solaris, for FIONBIO symbol
#endif
#ifdef HAVE_STRINGS_H
  #include <strings.h>  // for bcopy(), bzero()
#endif

#include "network.h"
#include "command.h"
#include "globals.h"
#include "malloc.h"
#include "utils.h"
#include "chessd_locale.h"
#include "timeseal.h"
#include "configuration.h"
#include "command_network.h"

extern int errno;

/* Index == fd, for sparse array, quick lookups! wasted memory :( */
static int findConnection(int fd)
{
	if (net_globals.con == NULL || fd < 0 || net_globals.con[fd]->status == NETSTAT_EMPTY)
		return -1;
	return fd;
}


static void set_sndbuf(int fd, int len)
{
	net_globals.con[fd]->sndbufpos = len;
	if (len < net_globals.con[fd]->sndbufsize)
		net_globals.con[fd]->sndbuf[len] = 0;
}

static int net_addConnection(int fd, struct in_addr fromHost)
{
	struct connection_t *con;
	int noblock = 1;

	/* possibly expand the connections array */
	if (fd >= net_globals.no_file) {
		int i;
		net_globals.con = (struct connection_t **)realloc(net_globals.con,
								  (fd+1) * sizeof(struct connection_t *));
		for (i=net_globals.no_file;i<fd+1;i++) {
			net_globals.con[i] = (struct connection_t *)calloc(1,
									   sizeof(struct connection_t));
			net_globals.con[i]->status = NETSTAT_EMPTY;
		}
		net_globals.no_file = fd+1;
	}

	if (findConnection(fd) >= 0) {
		d_printf( _("CHESSD: FD already in connection table!\n"));
		return -1;
	}
	if (ioctl(fd, FIONBIO, &noblock) == -1)
		d_printf( _("Error setting nonblocking mode errno=%d\n"), errno);

	con = net_globals.con[fd];
	// this "locks" this specific entry in the net_globals
	con->status = NETSTAT_EMPTY;

	con->fd = fd;
	con->outFd = fd != 0 ? fd : 1;
	con->fromHost = fromHost;
	con->timeseal = 0;
	con->timeseal_init = 1;
	con->time = 0;
	con->numPending = 0;
	con->inBuf[0] = 0;
	con->processed = 0;
	con->outPos = 0;
	if (con->sndbuf == NULL) {
#ifdef DEBUG
		d_printf( _("CHESSD: nac(%d) allocating sndbuf.\n"), fd);
#endif
		con->sndbufpos = 0;
		con->sndbufsize = MAX_STRING_LENGTH;
		con->sndbuf = malloc(MAX_STRING_LENGTH);
	} else {
#ifdef DEBUG
		d_printf( _("CHESSD: nac(%d) reusing old sndbuf size %d pos %d.\n"), 
				  fd, con->sndbufsize, con->sndbufpos);
#endif
	}
	con->state = 0;
	net_globals.numConnections++;

#ifdef DEBUG
	d_printf( _("CHESSD: fd: %d connections: %d  descriptors: %d \n"), 
			  fd, net_globals.numConnections, getdtablesize());	/* sparky 3/13/95 */
#endif

	// now that everything is set for this user, "release" the net_globals
	// lock
	con->status = NETSTAT_CONNECTED;
	return 0;
}

static int remConnection(int fd)
{
	int which, i;

	if ((which = findConnection(fd)) < 0) {
		d_printf( _("remConnection: Couldn't find fd to close.\n"));
		return -1;
	}
	net_globals.numConnections--;
	net_globals.con[fd]->status = NETSTAT_EMPTY;
	if (net_globals.con[fd]->sndbuf == NULL) {
		d_printf( _("CHESSD: remConnection(%d) SNAFU, this shouldn't happen.\n"), fd);
	} else {
		if (net_globals.con[fd]->sndbufsize > MAX_STRING_LENGTH) {
			net_globals.con[fd]->sndbufsize = MAX_STRING_LENGTH;
			net_globals.con[fd]->sndbuf =
				realloc(net_globals.con[fd]->sndbuf, MAX_STRING_LENGTH);
		}
		if (net_globals.con[fd]->sndbufpos) /* didn't send everything, bummer */
			set_sndbuf(fd, 0);
	}

	/* see if we can shrink the con array */
	for (i = fd; i < net_globals.no_file; i++) {
		if (net_globals.con[i]->status != NETSTAT_EMPTY)
			return 0;
	}

	/* yep! shrink it */
	for (i = fd; i < net_globals.no_file; i++) {
		FREE(net_globals.con[i]->sndbuf);
		FREE(net_globals.con[i]);
	}
	net_globals.no_file = fd;
	net_globals.con = (struct connection_t **)realloc(net_globals.con,
							  net_globals.no_file * sizeof(struct connection_t *));

	return 0;
}

static void net_flushme(int which)
{
	struct connection_t *con = net_globals.con[which];
	int sent = send(con->outFd, con->sndbuf, con->sndbufpos, 0);
	if (sent == -1) {
		if (errno != EPIPE)		/* EPIPE = they've disconnected */
			d_printf( _("CHESSD: net_flushme(%d) couldn't send, errno=%d.\n"), which, errno);
		set_sndbuf(which, 0);
	} else {
		con->sndbufpos -= sent;
		if (con->sndbufpos)
			memmove(con->sndbuf, con->sndbuf + sent, con->sndbufpos);
	}
	if (con->sndbufsize > MAX_STRING_LENGTH && con->sndbufpos < MAX_STRING_LENGTH) {
		/* time to shrink the buffer */
		con->sndbuf = realloc(con->sndbuf, MAX_STRING_LENGTH);
		con->sndbufsize = MAX_STRING_LENGTH;
	}
	set_sndbuf(which, con->sndbufpos);
}

static void net_flush_all_connections(void)
{
	int which;
	fd_set writefds;
	struct timeval to;

	FD_ZERO(&writefds);
	for (which = 0; which < net_globals.no_file; which++) {
		if (net_globals.con[which]->status == NETSTAT_CONNECTED &&
		    net_globals.con[which]->sndbufpos)
			FD_SET(net_globals.con[which]->outFd, &writefds);
	}

	to.tv_usec = 0;
	to.tv_sec = 0;
	select(net_globals.no_file, NULL, &writefds, NULL, &to);
	for (which = 0; which < net_globals.no_file; which++) {
		if (FD_ISSET(net_globals.con[which]->outFd, &writefds))
			net_flushme(which);
	}
}

static void net_flush_connection(int fd)
{
	int which;
	fd_set writefds;
	struct timeval to;
	
	if ((which = findConnection(fd)) >= 0 && net_globals.con[which]->sndbufpos) {
		FD_ZERO(&writefds);
		FD_SET(net_globals.con[which]->outFd, &writefds);
		to.tv_usec = 0;
		to.tv_sec = 0;
		select(net_globals.no_file, NULL, &writefds, NULL, &to);
		if (FD_ISSET(net_globals.con[which]->outFd, &writefds))
			net_flushme(which);
	}
}

static int sendme(int which, char *str, int len)
{
	int i, count = len;
	fd_set writefds;
	struct timeval to;
	struct connection_t *con = net_globals.con[which];

	// take smaller of len and remaining buffer space
	while ((i = ((con->sndbufsize - con->sndbufpos) < len) 
		   ? (con->sndbufsize - con->sndbufpos) : len) > 0) {
		memmove(con->sndbuf + con->sndbufpos, str, i);
		con->sndbufpos += i;
		str += i;
		len -= i;
		if (con->sndbufpos == con->sndbufsize) { // filled send buffer?
			FD_ZERO(&writefds);
			FD_SET(con->outFd, &writefds);
			to.tv_usec = 0;
			to.tv_sec = 0;
			select(net_globals.no_file, NULL, &writefds, NULL, &to);
			if (FD_ISSET(con->outFd, &writefds)) {
				net_flushme(which);
			} else {
				/* time to grow the buffer */
				con->sndbufsize += MAX_STRING_LENGTH;
				con->sndbuf = realloc(con->sndbuf, con->sndbufsize);
			}
		}
	}
	set_sndbuf(which, con->sndbufpos);
	return count;
}

/*
 * -1 for an error other than EWOULDBLOCK.
 * Put <lf> after every <cr> and put \ at the end of overlength lines.
 * Doesn't send anything unless the buffer fills, output waits until
 * flushed
 */
/* width here is terminal width = width var + 1 at present) */
int net_send_string(int fd, char *str, int format, int width)
{
	struct connection_t *con;
	int which, i, j;

	if ((which = findConnection(fd)) < 0)
		return -1;

	con = net_globals.con[which];
	while (*str) {
		for (i = 0; str[i] >= ' '; i++)
			;
		if (i) {
			if (format && (i >= (j = width - con->outPos))) { /* word wrap */
				i = j-1;
				while (i > 0 && str[i-1] != ' ')
					i--;
/*
				while (i > 0 && str[i - 1] == ' ')
					i--;
*/
				if (i == 0)
					i = j - 1;
				sendme(which, str, i);
				sendme(which, "\n\r\\   ", 6);
				con->outPos = 4;
				while (str[i] == ' ')	/* eat the leading spaces after we wrap */
					i++;
			} else {
				sendme(which, str, i);
				con->outPos += i;
			}
			str += i;
		} else {			/* non-printable stuff handled here */
			switch (*str) {
			case '\t':
				sendme(which, "        ", 8 - (con->outPos & 7));
				con->outPos &= ~7;
				if (con->outPos += 8 >= width)
					con->outPos = 0;
				break;
			case '\n':
				sendme(which, "\n\r", 2);
				con->outPos = 0;
				break;
			case '\033':
				con->outPos -= 3;
			default:
				sendme(which, str, 1);
			}
			str++;
		}
	}
	return 0;
}

/* if we get a complete line (something terminated by \n), copy it to com
   and return 1.
   if we don't get a complete line, but there is no error, return 0.
   if some error, return -1.
 */
static int readline2(char *com, int who)
{
	static const unsigned char will_tm[]  = {IAC, WILL, TELOPT_TM, '\0'};
	static const unsigned char will_sga[] = {IAC, WILL, TELOPT_SGA, '\0'};
	static const unsigned char ayt[] = N_("[Responding to AYT: Yes, I'm here.]\n");
	unsigned char *start, *s, *d;
	int howmany, state, fd, pending;

	// let's make sure there is no crap remaining from the previous user
	memset(com, 0, MAX_STRING_LENGTH * sizeof(char));

	state = net_globals.con[who]->state;
	if (state == 2 || state > 4) {
		d_printf( _("CHESSD: bad state %d for net_globals.con[%d], this is a bug.\n"),
				  state, who);
		state = 0;
	}
	s = start = net_globals.con[who]->inBuf;
	pending = net_globals.con[who]->numPending;
	fd = net_globals.con[who]->fd;

	// read the data sent by this user
	howmany = recv(fd, start + pending, MAX_STRING_LENGTH - 1 - pending, 0);
	if (howmany == 0)		/* error: they've disconnected */
		return -1;
	else if (howmany == -1) {
		if (errno != EWOULDBLOCK) {	/* some other error */
			return -1;
		} else if (net_globals.con[who]->processed) {
			/* nothing new and nothing old */
			// in the past, this is a place where one user could exploit the
			// server and have another user execute a command
			return 0;
		} else {
			/* nothing new, but some unprocessed old */
			howmany = 0;
		}
	}
	if (net_globals.con[who]->processed)
		s += pending;
	else
		howmany += pending;
	d = s;

	// just make sure that whatever goes over the limit is thrown away
	if (howmany >= (MAX_STRING_LENGTH - 1 - pending))
		howmany = MAX_STRING_LENGTH - 1 - pending;

	for (; howmany-- > 0; s++) {
		switch (state) {
		case 0: /* Haven't skipped over any control chars or telnet commands */
			if (*s == IAC) {
				d = s;
				state = 1;
			} else if (*s == '\n') {
				*s = '\0';
				strcpy(com, (char*) start);
				if (howmany)
					bcopy(s + 1, start, howmany);
				net_globals.con[who]->state = 0;
				net_globals.con[who]->numPending = howmany;
				net_globals.con[who]->inBuf[howmany] = 0;
				net_globals.con[who]->processed = 0;
				net_globals.con[who]->outPos = 0;
				return 1;
				/* Cannot strip ^? yet otherwise timeseal probs occur */
			} else if (*s == '\010') { /* ^H lets delete */
				if (s > start)
					d = s-1;
				else
					d = s;
				state = 2;
			} else if (*s > (0xff-0x20) || *s < 0x20) {
				d = s;
				state = 2;
			}
			break;
		case 1:			/* got telnet IAC */
			if (*s == IP)
				return -1;		/* ^C = logout */
			else if (*s == DO)
				state = 4;
			else if (*s == WILL || *s == DONT || *s == WONT)
				state = 3;		/* this is cheesy, but we aren't using em */
			else if (*s == AYT) {
				send(fd, _((char*) ayt), strlen(_((char*) ayt)), 0);
				state = 2;
			} else if (*s == EL) {	/* erase line */
				d = start;
				state = 2;
			} else			/* dunno what it is, so ignore it */
				state = 2;
			break;
		case 2:			/* we've skipped over something, need to
							   shuffle processed chars down */
			if (*s == IAC)
				state = 1;
			else if (*s == '\n') {
				*d = '\0';
				strcpy(com, (char*) start);
				if (howmany)
					memmove(start, s + 1, howmany);
				net_globals.con[who]->state = 0;
				net_globals.con[who]->numPending = howmany;
				net_globals.con[who]->inBuf[howmany] = 0;
				net_globals.con[who]->processed = 0;
				net_globals.con[who]->outPos = 0;
				return 1;
				/* Cannot strip ^? yet otherwise timeseal probs occur */
			} else if (*s == '\010') { /* ^H lets delete */
				if (d > start)
					d = d-1;
			} else if (*s >= 0x20)
				*(d++) = *s;
			break;
		case 3:			/* some telnet junk we're ignoring */
			state = 2;
			break;
		case 4:			/* got IAC DO */
			if (*s == TELOPT_TM)
				send(fd, will_tm, strlen((char*) will_tm), 0);
			else if (*s == TELOPT_SGA)
				send(fd, will_sga, strlen((char*) will_sga), 0);
			state = 2;
			break;
		}
	}
	if (state == 0)
		d = s;
	else if (state == 2)
		state = 0;

	net_globals.con[who]->state = state;
	net_globals.con[who]->numPending = d - start;
	net_globals.con[who]->inBuf[d-start] = 0;
	net_globals.con[who]->processed = 1;

	if (net_globals.con[who]->numPending == MAX_STRING_LENGTH-1) {
		/* buffer full */
		*d = '\0';
		strcpy(com, (char*) start);
		net_globals.con[who]->state = 0;
		net_globals.con[who]->numPending = 0;
		net_globals.con[who]->inBuf[0] = 0;
		net_globals.con[who]->processed = 0;
		return 1;
	}
	return 0;
}

void clean_user_buffer(int who)
{
	net_globals.con[who]->state = 0;
	net_globals.con[who]->numPending = 0;
	net_globals.con[who]->inBuf[0] = 0;
	net_globals.con[who]->processed = 0;
}

int check_valid_buffer(char* buf)
{
	int i;

	// Return 0 if a non-printable character found in string
	// (stop looking at first space or tab). Otherwise return 1.
	for (i = 0; buf[i] && buf[i] != ' ' && buf[i] != '\t'; i++) {
		if (!isprint(buf[i]))
			return 0;
	}
	return 1;
}

int net_init(int port)
{
	int opt;
	struct sockaddr_in serv_addr;
	struct linger lingeropt;

	net_globals.no_file = 0;
	net_globals.con = NULL;

	/* Open a TCP socket (an Internet stream socket). */
	if ((net_globals.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		d_printf( _("CHESSD: can't open stream socket\n"));
		return -1;
	}
	/* Bind our local address so that the client can send to us */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	/** added in an attempt to allow rebinding to the port **/

	opt = 1;
	setsockopt(net_globals.sockfd, SOL_SOCKET, SO_REUSEADDR,
			   (char *) &opt, sizeof(opt));
	opt = 1;
	setsockopt(net_globals.sockfd, SOL_SOCKET, SO_KEEPALIVE,
			   (char *) &opt, sizeof(opt));
	lingeropt.l_onoff = 0;
	lingeropt.l_linger = 0;
	setsockopt(net_globals.sockfd, SOL_SOCKET, SO_LINGER,
			   (char *) &lingeropt, sizeof(lingeropt));
/*
#ifdef DEBUG
	opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_DEBUG, (char *)&opt, sizeof(opt));
#endif
*/
	if (bind(net_globals.sockfd, (struct sockaddr *) & serv_addr, sizeof(serv_addr)) < 0) {
		d_printf( _("CHESSD: can't bind local address.  errno=%d\n"), errno);
		return -1;
	}
	opt = 1;
	ioctl(net_globals.sockfd, FIONBIO, &opt);
	fcntl(net_globals.sockfd, F_SETFD, 1);
	listen(net_globals.sockfd, 5);
	return 0;
}

void net_close(void)
{
	int i;

	for (i = 0; i < net_globals.no_file; i++) {
		if (net_globals.con[i]->status != NETSTAT_EMPTY)
			net_close_connection(net_globals.con[i]->fd);
	}
}

void net_close_connection(int fd)
{
	if (net_globals.con[fd]->status == NETSTAT_CONNECTED)
		net_flush_connection(fd);
	else
		d_printf( _("Trying to close an unconnected socket?!?!\n"));

	if (!remConnection(fd)) {
		if (fd > 2 && close(fd) < 0)
			d_printf( _("Couldn't close socket %d - errno %d\n"), fd, errno);
	} else
		d_printf( _("Failed to remove connection (Socket %d)\n"), fd);
}

void turn_echo_on(int fd)
{
	static const unsigned char wont_echo[] = {IAC, WONT, TELOPT_ECHO, '\0'};

	send(fd, wont_echo, strlen((char*)wont_echo), 0);
}

void turn_echo_off(int fd)
{
	static const unsigned char will_echo[] = {IAC, WILL, TELOPT_ECHO, '\0'};

	send(fd, will_echo, strlen((char*)will_echo), 0);
}

static struct in_addr net_connected_host(int fd)
{
	int which;

	if ((which = findConnection(fd)) < 0) {
		static struct in_addr ip_zero;
		d_printf( _("CHESSD: FD not in connection table!\n"));
		return ip_zero;
	}
	return net_globals.con[which]->fromHost;
}

void select_loop(void )
{
	char com[MAX_STRING_LENGTH];
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof(struct sockaddr_in);
	int fd, loop, nfound, lineComplete, current_socket, timeout = 0;
	fd_set readfds;
	struct timeval to;

	m_check_all();

	/* we only want to get signals here. This tries to
	   ensure a clean shutdown on 'kill' */
	unblock_signal(SIGTERM);
	block_signal(SIGTERM);

	while ((fd = accept(net_globals.sockfd, (struct sockaddr *) & cli_addr, &cli_len)) != -1) {
		if (net_addConnection(fd, cli_addr.sin_addr)) {
			d_printf( _("FICS is full.  fd = %d.\n"), fd);
			psend_raw_file(fd, MESS_DIR, MESS_FULL);
			close(fd);
		} else {
			if (fd >= net_globals.no_file)
				d_printf(_("FICS (ngc2): Out of range fd!\n"));
			else {
				fcntl(fd, F_SETFD, 1);
				process_new_connection(fd, net_connected_host(fd));
			}
		}
	}

	if (errno != EWOULDBLOCK)
		d_printf( _("CHESSD: Problem with accept().  errno=%d\n"), errno);

	net_flush_all_connections();

	if (net_globals.numConnections == 0)
		sleep(1); /* prevent the server spinning */

	FD_ZERO(&readfds);
	for (loop = 0; loop < net_globals.no_file; loop++)
		if (net_globals.con[loop]->status != NETSTAT_EMPTY)
			FD_SET(net_globals.con[loop]->fd, &readfds);

	to.tv_usec = 0;
	to.tv_sec = timeout;
	nfound = select(net_globals.no_file, &readfds, NULL, NULL, &to);
	for (loop = 0; loop < net_globals.no_file; loop++) {
		if (net_globals.con[loop]->status != NETSTAT_EMPTY) {
			fd = net_globals.con[loop]->fd;
		more_commands:
			lineComplete = readline2(com, fd);
			if (lineComplete == 0)	/* partial line: do nothing with it */
				continue;
			if (lineComplete > 0) {	/* complete line: process it */
				if (!timeseal_parse(com, net_globals.con[loop]))
					continue;

				if (process_input(fd, com) != COM_LOGOUT) {
					net_flush_connection(fd);
					goto more_commands;
				}
			}
			/* Disconnect anyone who gets here */
			process_disconnection(fd);
			net_close_connection(fd);
		}
	}

	if (process_heartbeat(&current_socket) == COM_LOGOUT) {
		process_disconnection(current_socket);
		net_close_connection(current_socket);
	}
}
