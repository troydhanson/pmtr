SUBDIRS = lemon
bin_PROGRAMS = proctab
proctab_CPPFLAGS = -I./include
proctab_SOURCES = cfg.c pmtr.c tok.c job.c net.c

tok.c pmtr.c job.c net.c: cfg.h

cfg.c cfg.h: cfg.y $(SUBDIRS)
	lemon/lemon $<
