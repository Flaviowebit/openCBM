RELATIVEPATH=../
include ${RELATIVEPATH}LINUX/config.make

.PHONY: all mrproper clean install uninstall install-files

PROG	= tapcontrol
SRCS	= tapcontrol.c
OBJS	= $(SRCS:.c=.o)

CFLAGS += -I../lib/cap -I../lib/misc -I../lib/tap-cbm -I../../include -I../common
LINK_FLAGS += -L../lib/cap -L../lib/tap-cbm -L../lib/misc -ltapcap -ltapcbm -ltapmisc

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG)

mrproper: clean

install-files:

install: install-files

uninstall:

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROG): $(OBJS)
	$(CC) $(LINK_FLAGS) $(OBJS) -o $@
