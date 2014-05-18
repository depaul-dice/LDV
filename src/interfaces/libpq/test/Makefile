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


PROGS = single

all: $(PROGS)
	/usr/bin/gcc-4.7 -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Winline -Wdeclaration-after-statement -Wendif-labels -fno-strict-aliasing -I../../../../src/interfaces/libpq -I../../../../src/include -D_GNU_SOURCE -I/usr/include/libxml2  -L../../../../src/port  -Wl,-rpath,'./'  single.c  -L../../../../src/port -lpgport -L../../../../src/interfaces/libpq -lpq  -o single
	cp single query

clean: 
	rm -f $(PROGS)

# need at least one process to run with param 100 (MYMAGICN)
run:
	./single dbname=single 100 &
	./single dbname=single 99 &
	sleep 6 && ./query dbname=single -1
