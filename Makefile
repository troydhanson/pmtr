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
	touch /etc/pmtr.conf
	if [ -f /sbin/chkconfig ];                    \
	then                                          \
	  cp initscript-centos /etc/rc.d/init.d/pmtr; \
	  /sbin/chkconfig --add pmtr;                 \
	  /etc/init.d/pmtr start;                     \
	elif [ -d /etc/init ];                        \
	then                                          \
	  cp initscript-ubuntu /etc/init/pmtr.conf;   \
	  /sbin/start pmtr;                           \
	fi
