SUBDIRS = po intl
ACLOCAL_AMFLAGS = -I m4

# some generic make variables
builddir = @builddir@
top_srcdir = @top_srcdir@
abs_top_srcdir = @abs_top_srcdir@

EXTRA_DIST = timeseal
# empty definitions allow us to use "+=" on included files
MOSTLYCLEANFILES =
CLEANFILES =

# include etc/Makefile.am first for chessdconf variables
include $(top_srcdir)/etc/Makefile.am
include $(top_srcdir)/src/Makefile.am
include $(top_srcdir)/data/Makefile.am
include $(top_srcdir)/sql/Makefile.am
