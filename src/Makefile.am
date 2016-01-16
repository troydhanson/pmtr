bin_PROGRAMS = proctab
noinst_PROGRAMS = lemon
lemon_SOURCES = lemon.c
proctab_CPPFLAGS = -I./include
proctab_SOURCES = cfg.c pmtr.c tok.c job.c net.c

tok.c pmtr.c job.c net.c: cfg.h

cfg.c cfg.h: cfg.y lemon
	@./lemon $<

