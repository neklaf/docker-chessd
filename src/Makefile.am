#   Copyright (c) 2003 Federal University of Parana
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# The kitchen sink.  TODO: split this monster
csourcedir = $(top_srcdir)/src

SHARED_SRC = \
	$(csourcedir)/adminproc.c $(csourcedir)/adminproc.h	\
	$(csourcedir)/adump.h \
	$(csourcedir)/algcheck.c $(csourcedir)/algcheck.h \
	$(csourcedir)/board.c $(csourcedir)/board.h	\
	$(csourcedir)/chessd_locale.h \
	$(csourcedir)/chessd_main.h		\
	$(csourcedir)/command.c $(csourcedir)/command.h				\
	$(csourcedir)/command_network.h \
	$(csourcedir)/common.h			\
	$(csourcedir)/comproc.c $(csourcedir)/comproc.h				\
	$(csourcedir)/config_genstrc.h \
	$(csourcedir)/config_read.c $(csourcedir)/config_read.h \
	$(csourcedir)/configuration.h		\
	$(csourcedir)/crypt-md5.c $(csourcedir)/crypt-md5.h			\
	$(csourcedir)/crypt-md5c.c $(csourcedir)/crypt-md5c.h			\
	$(csourcedir)/crypt-misc.c $(csourcedir)/crypt-misc.h			\
	$(csourcedir)/crypt.c $(csourcedir)/crypt.h \
	$(csourcedir)/dbdbi.c $(csourcedir)/dbdbi.h \
	$(csourcedir)/debug_com.c $(csourcedir)/debug_com.h \
	$(csourcedir)/eco.c $(csourcedir)/eco.h	\
	$(csourcedir)/follow.c $(csourcedir)/follow.h \
	$(csourcedir)/formula.c $(csourcedir)/formula.h \
	$(csourcedir)/gamedb.c $(csourcedir)/gamedb.h	\
	$(csourcedir)/gameproc.c $(csourcedir)/gameproc.h \
	$(csourcedir)/gics.c $(csourcedir)/gics.h \
	$(csourcedir)/globals.h \
	$(csourcedir)/help.c $(csourcedir)/help.h \
	$(csourcedir)/iset.c $(csourcedir)/iset.h		\
	$(csourcedir)/lists.c $(csourcedir)/lists.h \
	$(csourcedir)/logkeeper.c $(csourcedir)/logkeeper.h \
	$(csourcedir)/malloc.c $(csourcedir)/malloc.h \
	$(csourcedir)/matchproc.c $(csourcedir)/matchproc.h \
	$(csourcedir)/md5.h				\
	$(csourcedir)/movecheck.c $(csourcedir)/movecheck.h			\
	$(csourcedir)/multicol.c $(csourcedir)/multicol.h			\
	$(csourcedir)/network.c $(csourcedir)/network.h				\
	$(csourcedir)/obsproc.c $(csourcedir)/obsproc.h				\
	$(csourcedir)/pending.c $(csourcedir)/pending.h				\
	$(csourcedir)/playerdb.c $(csourcedir)/playerdb.h			\
	$(csourcedir)/protocol.c $(csourcedir)/protocol.h			\
	$(csourcedir)/ratings.c $(csourcedir)/ratings.h \
	$(csourcedir)/reload.c $(csourcedir)/reload.h \
	$(csourcedir)/seekproc.c $(csourcedir)/seekproc.h \
	$(csourcedir)/setup.c $(csourcedir)/setup.h	\
	$(csourcedir)/shutdown.c $(csourcedir)/shutdown.h			\
	$(csourcedir)/talkproc.c $(csourcedir)/talkproc.h			\
	$(csourcedir)/timeseal.c $(csourcedir)/timeseal.h			\
	$(csourcedir)/utils.c $(csourcedir)/utils.h \
	$(csourcedir)/variable.c $(csourcedir)/variable.h \
	$(csourcedir)/wn_com_proc.c $(csourcedir)/wn_com_proc.h \
	$(csourcedir)/wn_command.c			\
	$(csourcedir)/wn_command_network.h \
	$(csourcedir)/wn_network.c $(csourcedir)/wn_network.h \
	$(csourcedir)/wn_utils.c $(csourcedir)/wn_utils.h


localedir=$(datadir)/locale

AM_CFLAGS = -fPIC -W -Wall -Wno-unused-parameter -fno-strict-aliasing $(LIBXML_CFLAGS) $(CFLAGS)
AM_CPPFLAGS =						\
  -DPKGDATADIR=\"$(pkgdatadir)\" -DLIBDIR=\"$(libdir)\"	\
  -DPREFIXDIR=\"$(prefix)\"				\
  -DSYSCONFDIR=\"$(sysconfdir)\"			\
  -DLOCALEDIR=\"$(localedir)\" $(CPPFLAGS)		\
  -I..

bin_PROGRAMS = chessd
lib_LTLIBRARIES = chessd.la

chessd_SOURCES =  $(csourcedir)/chessd_main.c

chessd_la_SOURCES = $(SHARED_SRC)

chessd_CPPFLAGS = $(AM_CPPFLAGS)
chessd_LDFLAGS = -export-dynamic
chessd_LDADD = $(LIBXML_LIBS)
chessd_la_LDFLAGS = -module -avoid-version
chessd_la_CFLAGS = $(AM_CFLAGS) # needed to avoid .o conflicts

command.o: $(csourcedir)/command_list.h
