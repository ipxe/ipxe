CPPFLAGS    = 
LDLIBS      = 
CFLAGS      = -pipe -g -O2 -Wall
LDFLAGS     = -pipe
CC          = gcc
LD          = gcc
# Some "black" magic to determine optimal compiler flags for target
# architecture
TARGET_ARCH:= $(shell if [ \! -r .compile-options ] ; then ( \
                cpu=`grep cpu /proc/cpuinfo 2>&1 |head -1| \
                     cut -d : -f 2-| sed -e 's/ //g'`; \
                if [ x"$$cpu" = x"" ] ; then \
                  echo -fno-strength-reduce; \
                else if [ "$$cpu" = "386" ] ; then \
                  echo -m386 -fno-strength-reduce; \
                else if [ "$$cpu" = "486" ] ; then \
                  echo -m486 -fno-strength-reduce; \
                else if [ "$$cpu" = "Alpha" ] ; then \
                  echo -fno-strength-reduce; \
                else echo main\(\)\{\} >.compile-options.c; \
                  if gcc -mpentium -o .compile-options.o -c \
                         .compile-options.c &>/dev/null; then \
                     echo -mpentium -fstrength-reduce; \
                 else if gcc -m486 -malign-functions=2 -malign-jumps=2 \
                             -malign-loops=2 -o .compile-options.o -c \
                             .compile-options.c &>/dev/null; then \
                  echo -n -m486 -malign-functions=2 -malign-jumps=2; \
                  echo ' '-malign-loops=2 -fno-strength-reduce; \
                else echo -m486; \
              fi;fi;fi;fi;fi;fi) > .compile-options; \
              rm -f .compile-options.c .compile-options.o; \
              fi; cat .compile-options)
ASFLAGS     = $(TARGET_ARCH)

OBJS        = rom-scan.o

##############################################################################

ifeq (.depend,$(wildcard .depend))
all: rom-scan
include .depend
else
all: depend
	@$(MAKE) all
endif

##############################################################################

rom-scan: $(OBJS)

##############################################################################

clean:
	$(RM) *~ *.o *.dvi *.log *.aux *yacc.tab.[ch] *yacc.output *lex.[co] \
              *.dat .depend .tmp_depend .compile-options*
	strip rom-scan >&/dev/null || true

##############################################################################

depend:
	for i in *.c;do $(CPP) $(CPPFLAGS) -MM $$i;done >.tmp_depend
	mv .tmp_depend .depend


