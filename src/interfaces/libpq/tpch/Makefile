#
# Makefile for example programs
#

#subdir = src/test/examples
subdir = src/interfaces/libpq/test
#top_builddir = ../../..
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -pg -I$(libpq_srcdir) $(CPPFLAGS)
override LDLIBS := -pg $(libpq_pgport) $(LDLIBS)

DIRS = access bootstrap catalog parser commands executor lib libpq \
	metaq nodes optimizer port postmaster provrewrite regex rewrite \
	storage tcop tsearch utils ../timezone
SUBSYSOBJS = $(DIRS:%=$(top_builddir)/src/backend/%/SUBSYS.o)

OBJS = single.o
PROGS = single

all: $(PROGS)

single: single.c ../libpq.a
	$(CC) $(CFLAGS) -pg -I../../../../src/interfaces/libpq -I../../../../src/include -D_GNU_SOURCE -I/usr/include/libxml2  -L../../../../src/port  -Wl,-rpath,'./'  single.c  -L../../../../src/port -lpgport -L../../../../src/interfaces/libpq ../libpq.a -lcrypt  -o single
	cp single query
	
parser: parser.o strfuncs.o $(SUBSYSOBJS)
	$(CC) $(CFLAGS) parser.o strfuncs.o $(SUBSYSOBJS) $(LDFLAGS) $(LIBS) -o $@$(X)

clean: 
	rm -f $(PROGS) $(OBJS)

# need at least one process to run with param 100 (MYMAGICN)
run:
	./single dbname=single 100 &
	./single dbname=single 99 &
	sleep 6 && ./query dbname=single -1
