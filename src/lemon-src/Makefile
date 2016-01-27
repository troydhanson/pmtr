# This Makefile is intended to be invoked manually by the author if cfg.y changes.
# It is not part of the standard build process, since the output generated here 
# (cfg.c and cfg.h) are checked in, as if they are static sources; see README.md

ALL: lemon cfg.c

lemon: lemon.c
	$(CC) -o $@ $<

cfg.c: lemon cfg.y
	./lemon -q cfg.y

.PHONY: clean

clean: 
	rm -f lemon
