#!/usr/bin/gdb -x
# Test suite for GDB remote debugging
# Run:
#   make bin/gpxe.hd.tmp
#   make
#   gdb
#   (gdb) target remote :TCPPORT
#   OR
#   (gdb) target remote udp:IP:UDPPORT
#   (gdb) source tests/gdbstub_test.gdb

define gpxe_load_symbols
	file bin/gpxe.hd.tmp
end

define gpxe_assert
	if $arg0 != $arg1
		echo FAIL $arg2\n
	else
		echo PASS $arg2\n
	end
end

define gpxe_start_tests
	jump gdbstub_test
end

define gpxe_test_regs_read
	gpxe_assert $eax 0xea010203 "gpxe_test_regs_read eax"
	gpxe_assert $ebx 0xeb040506 "gpxe_test_regs_read ebx"
	gpxe_assert $ecx 0xec070809 "gpxe_test_regs_read ecx"
	gpxe_assert $edx 0xed0a0b0c "gpxe_test_regs_read edx"
	gpxe_assert $esi 0x510d0e0f "gpxe_test_regs_read esi"
	gpxe_assert $edi 0xd1102030 "gpxe_test_regs_read edi"
end

define gpxe_test_regs_write
	set $eax = 0xea112233
	set $ebx = 0xeb445566
	set $ecx = 0xec778899
	set $edx = 0xedaabbcc
	set $esi = 0x51ddeeff
	set $edi = 0xd1010203
	c
	gpxe_assert $eax 0xea112233 "gpxe_test_regs_write eax"
	gpxe_assert $ebx 0xeb445566 "gpxe_test_regs_write ebx"
	gpxe_assert $ecx 0xec778899 "gpxe_test_regs_write ecx"
	gpxe_assert $edx 0xedaabbcc "gpxe_test_regs_write edx"
	gpxe_assert $esi 0x51ddeeff "gpxe_test_regs_write esi"
	gpxe_assert $edi 0xd1010203 "gpxe_test_regs_write edi"

	# This assumes segment selectors are always 0x10 or 0x8 (for code).
	gpxe_assert $cs 0x08 "gpxe_test_regs_write cs"
	gpxe_assert $ds 0x10 "gpxe_test_regs_write ds"
end

define gpxe_test_mem_read
	c
	gpxe_assert ({int}($esp+4)) 0x11223344 "gpxe_test_mem_read int"
	gpxe_assert ({short}($esp+2)) 0x5566 "gpxe_test_mem_read short"
	gpxe_assert ({char}($esp)) 0x77 "gpxe_test_mem_read char"
end

define gpxe_test_mem_write
	set ({int}($esp+4)) = 0xaabbccdd
	set ({short}($esp+2)) = 0xeeff
	set ({char}($esp)) = 0x99
	c
	gpxe_assert ({int}($esp+4)) 0xaabbccdd "gpxe_test_mem_write int"
	gpxe_assert ({short}($esp+2)) (short)0xeeff "gpxe_test_mem_write short"
	gpxe_assert ({char}($esp)) (char)0x99 "gpxe_test_mem_write char"
end

define gpxe_test_step
	c
	si
	gpxe_assert ({char}($eip-1)) (char)0x90 "gpxe_test_step" # nop = 0x90
end

define gpxe_test_awatch
	awatch watch_me

	c
	gpxe_assert $ecx 0x600d0000 "gpxe_test_awatch read"
	if $ecx == 0x600d0000
		c
	end

	c
	gpxe_assert $ecx 0x600d0001 "gpxe_test_awatch write"
	if $ecx == 0x600d0001
		c
	end

	delete
end

define gpxe_test_watch
	watch watch_me
	c
	gpxe_assert $ecx 0x600d0002 "gpxe_test_watch"
	if $ecx == 0x600d0002
		c
	end
	delete
end

gpxe_load_symbols
gpxe_start_tests
gpxe_test_regs_read
gpxe_test_regs_write
gpxe_test_mem_read
gpxe_test_mem_write
gpxe_test_step
gpxe_test_awatch
gpxe_test_watch
