#!/bin/sh
xpmtoppm                                <linux-logo.xpm >linux-logo.ppm
../ppmtoansi -b 0/0/255 -y 10 -t 0/0/0:4 linux-logo.ppm >linux-logo.ansi
xpmtoppm                                <etherboot.xpm  >etherboot.ppm
../ppmtoansi -b 0/0/0                    etherboot.ppm  >etherboot.ansi
xpmtoppm                                <text.xpm       >text.ppm
../ppmtoansi -b 0/0/0   -x 10            text.ppm       >text.ansi
xpmtoppm                                <x.xpm          >x.ppm
../ppmtoansi -b 0/0/0   -x  8            x.ppm          >x.ansi
xpmtoppm                                <dos.xpm        >dos.ppm
../ppmtoansi -b 0/0/0   -x  8            dos.ppm        >dos.ansi
xpmtoppm                                <hd.xpm         >hd.ppm
../ppmtoansi -b 0/0/0   -x  8            hd.ppm         >hd.ansi
xpmtoppm                                <floppy.xpm     >floppy.ppm
../ppmtoansi -b 0/0/0   -x  8            floppy.ppm     >floppy.ansi
xpmtoppm                                <flash.xpm      >flash.ppm
../ppmtoansi -b 0/0/0   -x 11            flash.ppm      >flash.ansi
