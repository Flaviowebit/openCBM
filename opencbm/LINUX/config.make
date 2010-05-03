# $Id: config.make,v 1.7.4.9 2010-05-03 00:47:58 fbriere Exp $
#


# NEVER CHANGE THIS!
# we rely on bash features, thus, we need bash to compile successfully!
export SHELL=/bin/bash

# choose your crossassembler (if you have one).
# mandatory if you want to hack any of the 6502 sources.
#
#XASS        = xa
XASS        = cl65


#
# destination directories
#
PREFIX      = /usr/local
BINDIR      = $(PREFIX)/bin
LIBDIR      = $(PREFIX)/lib
MANDIR      = $(PREFIX)/man/man1
INFODIR     = $(PREFIX)/info
INCDIR      = $(PREFIX)/include
MODDIR      = `for d in /lib/modules/\`uname -r\`/{kernel/drivers/char,extra,misc}; do test -d $$d && echo $$d; done | head -n 1`
UDEV_RULES  = /etc/udev/rules.d/


#
# compiler/linker flags. Should be ok.
#
ARCH	     = linux

CFLAGS       = -O2 -Wall
ALL_CFLAGS   = -I../include -I../include/LINUX -DOPENCBM_VERSION=$(VERSION) $(CFLAGS)
LIB_CFLAGS   = $(ALL_CFLAGS) -D_REENTRANT
SHLIB_CFLAGS = $(LIB_CFLAGS) -fPIC
LINK_FLAGS   = -L../lib -L../arch/$(ARCH) -lopencbm -larch $(LDFLAGS)
SONAME       = -Wl,-soname -Wl,
CC           = gcc
AR           = ar
LDCONFIG     = /sbin/ldconfig

#
# location of the kernel source directory
# (removed, use the later implementation instead. I left them in in case the
#  later implementation does not work on a particular machine.)
#KERNEL_SOURCE = /usr/src/linux                      # for kernel 2.4
#KERNEL_SOURCE = /lib/modules/`uname -r`/build       # for kernel 2.6

# from patch #1189489 on SourceForge, with fix from #1189492):
KERNEL_SOURCE = ${shell for d in {/lib/modules/`uname -r`/build,/usr/src/linux}; do test -d $$d && echo $$d; done | head -n 1}

#
# Find out if we should include linux/autoconf.h or linux/conf.h in the kernel module
#
KERNEL_INCLUDE_CONFIG = ${shell for c in ${KERNEL_SOURCE}/include/linux/autoconf.h ${KERNEL_SOURCE}/include/linux/config.h; do test -f $$c && echo $$c; done | head -n 1}

#
# kernel driver compile flags.
#
# add `-DDIRECT_PORT_ACCESS' to avoid usage of the generic parport module
#   (pre-0.3.0 behaviour, definitely needed for kernel 2.0.x)
#
# add `-DOLD_C4L_CABLE' if you want to use your old (cbm4linux <= 0.2.0)
#   XE1541-like cable. Don't to it. Upgrade to XM1541 instead.
#
#KERNEL_FLAGS = -DDIRECT_PORT_ACCESS
KERNEL_FLAGS = ${KERNEL_DEFINE}


#
# common compile flags
#
.SUFFIXES: .a65 .o65 .inc .lo

.c.lo:
	$(CC) $(SHLIB_CFLAGS) -c -o $@ $<

.o65.inc:
	test -s $< && od -w8 -txC -v -An $< | \
	sed 's/\([0-9a-f]\{2\}\) */0x\1,/g; $$s/,$$//' > $@


#
# package version (major.minor.release). Don't touch.
#
MAJ = 0
MIN = 4
REL = 2

VERSION = '"$(MAJ).$(MIN).$(REL)"'

#
# cross assembler definitions.
# patches for other assemblers are welcome.
#

# xa defs
XA          = xa

# cl65 defs, contributed by Ullrich von Bassewitz <uz(at)musoftware(dot)de>
# (cc65 >= 2.6 required)
CL65        = cl65
LD65        = ld65
CA65_FLAGS  = --feature labels_without_colons --feature pc_assignment --feature loose_char_term --asm-include-dir ..


#
# suffix rules
#
.a65.o65:
ifeq ($(XASS),xa)
	$(XA) $< -o $@
else
ifeq ($(XASS),cl65)
	$(CL65) -c $(CA65_FLAGS) -o $*.tmp $<
	$(LD65) --target none -o $@ $*.tmp && rm -f $*.tmp
else
	@echo "*** Error: No crossassembler defined. Check config.make" 2>&1
	exit 1
endif
endif
