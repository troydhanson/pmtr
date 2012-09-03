SUBDIRS=lemon 
all: $(SUBDIRS) pmtr 
CFLAGS=-I /usr/local/include
CFLAGS+=-g


.PHONY: clean $(SUBDIRS)

$(SUBDIRS) doc:
	$(MAKE) -C $@

cfg.c: cfg.y
	lemon/lemon $<

pmtr: pmtr.c cfg.c job.c tok.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o *.out pmtr cfg.c cfg.h
	for f in $(SUBDIRS); do make -C $$f $@; done

install: pmtr
	./install-pmtr.sh
