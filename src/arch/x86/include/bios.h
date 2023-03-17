#pragma once

#ifndef BIOS_H
    #define BIOS_H

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

    #define BDA_SEG 0x0040
    #define BDA_EBDA 0x000e
    #define BDA_EQUIPMENT_WORD 0x0010
    #define BDA_KB0 0x0017
    #define BDA_KB0_RSHIFT 0x01
    #define BDA_KB0_LSHIFT 0x02
    #define BDA_KB0_CTRL 0x04
    #define BDA_KB0_CAPSLOCK 0x040
    #define BDA_FBMS 0x0013
    #define BDA_TICKS 0x006c
    #define BDA_MIDNIGHT 0x0070
    #define BDA_REBOOT 0x0072
    #define BDA_REBOOT_WARM 0x1234
    #define BDA_NUM_DRIVES 0x0075
    #define BDA_CHAR_HEIGHT 0x0085
    #define BDA_KB2 0x0096
    #define BDA_KB2_RALT 0x08

#endif /* BIOS_H */
