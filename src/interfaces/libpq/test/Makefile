#
# Makefile for example programs
#

#subdir = src/test/examples
subdir = src/interfaces/libpq/test
#top_builddir = ../../..
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)

DIRS = access bootstrap catalog parser commands executor lib libpq \
	metaq nodes optimizer port postmaster provrewrite regex rewrite \
	storage tcop tsearch utils ../timezone
SUBSYSOBJS = $(DIRS:%=$(top_builddir)/src/backend/%/SUBSYS.o)

OBJS = single.o parser.o
PROGS = single parser

all: $(PROGS)

single: single.c
	$(CC) $(CFLAGS) -I../../../../src/interfaces/libpq -I../../../../src/include -D_GNU_SOURCE -I/usr/include/libxml2  -L../../../../src/port  -Wl,-rpath,'./'  single.c  -L../../../../src/port -lpgport -L../../../../src/interfaces/libpq -lpq  -o single
	cp single query
	
parser: parser.o
	$(CC) $(CFLAGS) parser.o $(SUBSYSOBJS) $(LDFLAGS) $(LIBS) -o $@$(X)

clean: 
	rm -f $(PROGS) $(OBJS)

# need at least one process to run with param 100 (MYMAGICN)
run:
	./single dbname=single 100 &
	./single dbname=single 99 &
	sleep 6 && ./query dbname=single -1
