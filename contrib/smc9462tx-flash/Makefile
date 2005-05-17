GCC = gcc
KERNELDIR = ../linux-2.4.20
KERNELSTYLE=-D__KERNEL__ -DCPU=__i386__ -DMODULE
INCLUDE_DIR=-I$(KERNELDIR)/include -I../include -I$(ROOTDIR)/include

all: dp83820flash.o dp83820_write

CFLAGS+=-O2 -Wall -fomit-frame-pointer -fno-strength-reduce
CFLAGS+=$(KERNELSTYLE) $(CDEBUG) $(INCLUDE_DIR)

install:

dp83820flash.o: dp83820flash.c
	$(GCC) dp83820flash.c -o dp83820flash.o -c $(CFLAGS)

dp83820_write: dp83820_write.c
	$(GCC) $< -o $@ -Wall -O2

clean: 
	$(RM) *.o dp83820_write
