SUBDIRS = lemon
bin_PROGRAMS = proctab
proctab_CPPFLAGS = -I./include
proctab_SOURCES = pmtr.c tok.c job.c cfg.c

cfg.c: cfg.y $(SUBDIRS)
	lemon/lemon $<
