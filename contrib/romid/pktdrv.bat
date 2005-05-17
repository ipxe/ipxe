@echo off
if (%PKTDRV%)==(P) goto end
A:\PKT\ROMID
if (%ROMID%)==() goto select
if (%ROMID%)==(NE) goto NE
if (%ROMID%)==(WD) goto WD
if (%ROMID%)==(3C509) goto 3C509
if (%ROMID%)==(3C905B) goto 3C905B
if (%ROMID%)==(EEPRO100) goto EEPRO100
if (%PCIID%)==(10EC:8029) goto RTL8029
if (%PCIID%)==(10EC:8139) goto RTL8139
if (%PCIID%)==(11AD:0002) goto LITEON
if (%PCIID%)==(1011:0009) goto DEC
echo No Driver installed!
goto end
:select
cls
echo *** Select your network card.... ***
echo.
echo  1 -- Intel EtherExpress 100B PCI Adapter
echo  2 -- 3Com 3C90X Etherlink III PCI Adapter
echo  3 -- 3Com 3C5X9 Etherlink III ISA Adapter
echo  4 -- NE2000 Novell ISA Adapter
echo  5 -- Realtek 8029 PCI Adapter
echo  6 -- Realtek 8139 PCI Adapter
echo  7 -- SMC EliteUltra 8216 ISA Adapter
echo  8 -- DEC21x4 Busmaster PCI Adapter
echo  9 -- LiteOn PNIC Busmaster PCI Adapter
echo.
echo *** **************************** ***
echo.
choice /c123456789 /n Select:
if errorlevel 9 goto LITEON
if errorlevel 8 goto DEC
if errorlevel 7 goto WD
if errorlevel 6 goto RTL8139
if errorlevel 5 goto RTL8029
if errorlevel 4 goto NE
if errorlevel 3 goto 3C509
if errorlevel 2 goto 3C905B
if errorlevel 1 goto EEPRO100
goto end

:EEPRO100
A:\PKT\E100BPKT 0x60
if errorlevel 0 goto ok
goto end
:NE
A:\PKT\NE2000 0x60 11 0x0300
if errorlevel 0 goto ok
goto end
:RTL8029
A:\PKT\PCIPKT 0x60
if errorlevel 0 goto ok
goto end
:RTL8139
A:\PKT\RTSPKT 0x60
if ERRORLEVEL 0 goto OK
goto end
:WD
A:\PKT\SMC_WD 0x60
if errorlevel 0 goto ok
goto end
:3C509
A:\PKT\3C5X9PD 0x60
if errorlevel 0 goto ok
goto end
:3C905B
A:\PKT\3C90XPD 0x60
if errorlevel 0 goto ok
goto end
:LITEON
A:\PKT\FEPD 0x60
if ERRORLEVEL 0 goto ok
goto end
:DEC
A:\PKT\ETHPCI 0x60
if ERRORLEVEL 0 goto ok
goto end

:ok
SET PKTDRV=P
:end

