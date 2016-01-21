srcdir = @srcdir@

bin_PROGRAMS = proctab
noinst_PROGRAMS = lemon

lemon_SOURCES = lemon.c
EXTRA_DIST = cfg.lt cfg.y

proctab_CPPFLAGS = -I$(srcdir)/include
proctab_SOURCES = job.c job.h net.c net.h tok.c pmtr.c pmtr.h
noinst_HEADERS = include

# generated build products
nodist_proctab_SOURCES = cfg.c cfg.h

tok.c pmtr.c job.c net.c: cfg.h

cfg.c cfg.h: cfg.y cfg.lt lemon
	./lemon $(srcdir)/cfg.y

