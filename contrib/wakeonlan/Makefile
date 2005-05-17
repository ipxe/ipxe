## Makefile for use with gnu make and MinGW32 gnu gcc

TARGET=wol

CC=gcc
LD=gcc

CPPFLAGS= -Wall -O2
LFLAGS= -s

#LIBFILES= -lwsock
#LIBPATH= -L/usr/lib

ICON=$(TARGET).ico
OBJS=$(TARGET).o
RESF=$(TARGET).rc

#RESNAME=$(TARGET).res
#BINNAME=$(TARGET).exe
BINNAME=$(TARGET)

$(BINNAME): $(OBJS) $(RESNAME)
	$(LD) $(LFLAGS) -o $@ $^ $(LIBPATH) $(LIBFILES)

%.res:%.rc
	windres -I rc -O coff -i $< -o $@

%.rc:Makefile
	@echo 100 ICON "$(ICON)" > $@

dist:$(BINNAME)
	rm -f $(OBJS) $(RESNAME) $(RESF)

clean:
	rm -f $(OBJS) $(RESNAME) $(RESF)
	rm -f $(BINNAME)

