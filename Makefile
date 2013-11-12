SUBDIRS=lemon 
all: $(SUBDIRS) cfg.c pmtr 
CFLAGS=-Iinclude 
#CFLAGS+=-Wall
CFLAGS+=-g

$(SUBDIRS) doc:
	$(MAKE) -C $@

cfg.c cfg.h: cfg.y
	lemon/lemon $<

# TODO force rebuild of c files if any h files change

objs := cfg.o $(patsubst %.c,%.o,$(wildcard *.c)) 

pmtr : $(objs)

clean:
	rm -f *.o *.out pmtr cfg.c cfg.h
	for f in $(SUBDIRS); do make -C $$f $@; done

export BINDIR=/usr/bin
UTILS=pmtr-rptserver pmtr-ctl
install: pmtr
	./install-pmtr.sh
	(cd util; cp ${UTILS} ${BINDIR})
	(cd ${BINDIR}; chmod a+rx pmtr ${UTILS})

.PHONY: clean $(SUBDIRS)

