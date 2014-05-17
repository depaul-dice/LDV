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

clean: 
	rm -f $(PROGS)

# need at least one process to run with param 100 (MYMAGICN)
run:
	./single dbname=single 100 &
	./single dbname=single 99 &
