/* Default linker script, for normal executables */
OUTPUT_FORMAT("coff-e1-big")
MEMORY
	{
        romvec   : ORIGIN = 0x2000000,  LENGTH = 0x0000400
        flash    : ORIGIN = 0xE0000000, LENGTH = 0x20000 
        eflash   : ORIGIN = 0x2200000,  LENGTH = 0x20000
        ram      : ORIGIN = 0x00000000, LENGTH = 0x1000000
        eram16MB : ORIGIN = 0x01000000, LENGTH = 0
        sram     : ORIGIN = 0x40000000, LENGTH = 0x40000
		iram     : ORIGIN = 0xC0000000, LENGTH = 0x4000
	}

SEARCH_DIR("/usr/local/e1-coff/lib");
ENTRY(Main)
MEM0start   = 0x00000000;
MEM0size    = 0x40000000;
MEM1start   = 0x40000000;
MEM1size    = 0x40000000;
MEM2start   = 0x80000000;
MEM2size    = 0x40000000;
IRAMstart   = 0xC0000000;
IRAMsize    = 0x20000000;
MEM3start   = 0xE0000000;
MEM3size    = 0x20000000;
Stack1Reserve = 560;
_Stack1Size = DEFINED(_Stack1Size)? _Stack1Size : 1*1024;
_Stack2Size = DEFINED(_Stack2Size)? _Stack2Size : 16*1024;
_Stack1Base = DEFINED(_Stack1Base)? _Stack1Base : __bss_end__;
_Stack2Base = DEFINED(_Stack2Base)? _Stack2Base : __bss_end__ + _Stack1Size + Stack1Reserve;
_Mem0HeapBase = DEFINED(_Mem0HeapBase)? _Mem0HeapBase : _Stack2Base + _Stack2Size;
_Mem1HeapBase = DEFINED(_Mem1HeapBase)? _Mem1HeapBase : 0;
Priority    = DEFINED(Priority)   ? Priority    : 31;
TextBase    = DEFINED(TextBase)   ? TextBase    : 0xa00000;
SECTIONS
{
  .G6 (DEFINED(G6Base) ? G6Base : 0x9000) : {
    *(.G6)
  }
  .G7 (DEFINED(G7Base) ? G7Base : 0x40001000) : {
    *(.G7)
  }
  .G8 (DEFINED(G8Base) ? G8Base : 0xC0000000) : {
    *(.G8)
  }
  .G9 (DEFINED(G9Base) ? G9Base : 0) : {
    *(.G9)
  }
  .G10 (DEFINED(G10Base) ? G10Base : 0) : {
    *(.G10)
  }
  .G11 (DEFINED(G11Base) ? G11Base : 0) : {
    *(.G11)
  }
  .G12 (DEFINED(G12Base) ? G12Base : 0) : {
    *(.G12)
  }
  .G13 (DEFINED(G13Base) ? G13Base : 0) : {
    *(.G13)
  }

  .text TextBase : {
	__virt_start = .;
	__text = . ;
    *(.text)
    *(.fini)
 		. = ALIGN(16);
		_isa_drivers = . ;
		*(.drivisa);
		_isa_drivers_end = . ;
		. = ALIGN(16);

   *(.init)
  _etext  =  . ;
    /* _init = DEFINED(_init) ? _init : 0; */
    /* _fini = DEFINED(_fini) ? _fini : 0; */
    /* _argc = DEFINED(_argc) ? _argc : 0; */
    /* _argv = DEFINED(_argv) ? _argv : 0; */
    /* _envp = DEFINED(_envp) ? _envp : 0; */
    /* _hwinit = DEFINED(_hwinit) ? _hwinit : 0; */
    /* _atexit = DEFINED(_atexit) ? _atexit : 0; */
    G6Size = SIZEOF(.G6);
    G7Size = SIZEOF(.G7);
    G8Size = SIZEOF(.G8);
    G9Size = SIZEOF(.G9);
    G10Size = SIZEOF(.G10);
    G11Size = SIZEOF(.G11);
    G12Size = SIZEOF(.G12);
    G13Size = SIZEOF(.G13);

}

  .data  SIZEOF(.text) + ADDR(.text) : {
    *(.data)
     _edata  =  . ;
}

  .bss  SIZEOF(.data) + ADDR(.data) : 
  {
     __bss_start__ = ALIGN( 0x10 ) ;
     __bss = . ;
    *(.bss)
    *(COMMON)
     __end = . ;
     __bss_end__ = ALIGN( 0x10 ) ;
     __ebss = . ;
  }

.eram16MB :    
	{
	___ramend = . - 0x7000;
	} > eram16MB

  .stab  0 (NOLOAD) :
  {
    [ .stab ]
  }
  .stabstr  0 (NOLOAD) :
  {
    [ .stabstr ]
  }
  _GOT_  0 (NOLOAD) :
  {
    [ _GOT_ ]
  }
}
