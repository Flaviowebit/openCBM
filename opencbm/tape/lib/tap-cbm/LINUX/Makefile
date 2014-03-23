RELATIVEPATH=../../../
include ${RELATIVEPATH}LINUX/config.make

.PHONY: all clean mrproper install uninstall install-files

CFLAGS += -I${RELATIVEPATH}/include

LIB     = libtapcbm.a
SRCS    = tap-cbm.c

OBJS    = $(SRCS:.c=.lo)

all: $(LIB)

clean:
	rm -f $(OBJS) $(LIB)

mrproper: clean

install-files:

install: install-files

uninstall:

%cap.lo: %.c

.c.o:
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(LIB): $(OBJS)
	$(AR) r $@ $(OBJS)
