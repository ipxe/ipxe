# Override ARCH here or on the command line
# ARCH=i386
# Additionally you can supply extra compilation arguments, e.g. for x86_64
# EXTRA_CFLAGS=-m32
# EXTRA_ASFLAGS=--32
# EXTRA_LDFLAGS=-m elf_i386
ifndef ARCH
ARCH:=$(shell uname -m | sed -e s,i[3456789]86,i386,)
endif
MAKEDEPS:=
SUFFIXES:=

include Config
include arch/$(ARCH)/Config
include Makefile.main
