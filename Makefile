SUBDIRS=lemon doc
all: $(SUBDIRS) pmtr 
CFLAGS=-I /usr/local/include
CFLAGS+=-g


.PHONY: clean $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

cfg.c: cfg.y
	lemon/lemon $<

pmtr: pmtr.c cfg.c job.c tok.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o *.out pmtr cfg.c cfg.h
	for f in $(SUBDIRS); do make -C $$f $@; done

install: pmtr
	cp $^ /usr/local/bin
	#cp upstart/pmtr.conf /etc/init/pmtr.conf
	touch /etc/pmtr.conf
