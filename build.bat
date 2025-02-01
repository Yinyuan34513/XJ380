@echo off
chcp 65001
rmdir /s /q out
rmdir /s /q esp
mkdir out
mkdir esp\EFI\BOOT

set ASM_FLAGS=-O0 -Wall -g -nostdlib -m64 -ffreestanding -fno-exceptions -fno-rtti
set C_FLAGS=-O0 -Wall -g -ffreestanding -m64 -fno-exceptions -fno-rtti -std=c11 -fshort-wchar
set CPP_FLAGS=-O0 -Wall -g -ffreestanding -m64 -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar

set ASM_FILES=
for /r kernel %%i in (*.S) do (
  set ASM_FILES=!ASM_FILES! %%i
)
for /r driver %%i in (*.S) do (
  set ASM_FILES=!ASM_FILES! %%i
)
for /r graphics %%i in (*.S) do (
  set ASM_FILES=!ASM_FILES! %%i
)
for /r font %%i in (*.S) do (
  set ASM_FILES=!ASM_FILES! %%i
)
for /r lib %%i in (*.S) do (
  set ASM_FILES=!ASM_FILES! %%i
)

set C_FILES=
for /r kernel %%i in (*.c) do (
  set C_FILES=!C_FILES! %%i
)
for /r driver %%i in (*.c) do (
  set C_FILES=!C_FILES! %%i
)
for /r graphics %%i in (*.c) do (
  set C_FILES=!C_FILES! %%i
)
for /r font %%i in (*.c) do (
  set C_FILES=!C_FILES! %%i
)
for /r lib %%i in (*.c) do (
  set C_FILES=!C_FILES! %%i
)

set CPP_FILES=
for /r kernel %%i in (*.cpp) do (
  set CPP_FILES=!CPP_FILES! %%i
)
for /r driver %%i in (*.cpp) do (
  set CPP_FILES=!CPP_FILES! %%i
)
for /r graphics %%i in (*.cpp) do (
  set CPP_FILES=!CPP_FILES! %%i
)
for /r font %%i in (*.cpp) do (
  set CPP_FILES=!CPP_FILES! %%i
)
for /r lib %%i in (*.cpp) do (
  set CPP_FILES=!CPP_FILES! %%i
)

echo [ >out\compile_commands.json

..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

setlocal enabledelayedexpansion
set counter=1
for %%f in (%ASM_FILES%) do (
  call :add_to_compile_commands %%f "gcc %%f %ASM_FLAGS% -c -o out\!counter!.o"
  ..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-gcc.exe %%f %ASM_FLAGS% -c -o out\!counter!.o
  set /a counter+=1
)
for %%f in (%C_FILES%) do (
  call :add_to_compile_commands %%f "gcc %%f %C_FLAGS% -c -o out\!counter!.o"
  ..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-gcc.exe %%f %C_FLAGS% -c -o out\!counter!.o
  set /a counter+=1
)
for %%f in (%CPP_FILES%) do (
  call :add_to_compile_commands %%f "g++ %%f %CPP_FLAGS% -c -o out\!counter!.o"
  ..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe %%f %CPP_FLAGS% -c -o out\!counter!.o
  set /a counter+=1
)

..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -e KernelMain -T linker.ld --static -o out\kernel.elf out\*.o font\hankaku.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-gcc.exe kallsyms\kallsyms.c -o .\out\kallsyms
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-nm.exe -n .\out\kernel.elf | .\out\kallsyms > .\out\kallsyms.S
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-gcc.exe -O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -nostdinc -c .\out\kallsyms.S -o .\out\kallsyms.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -z muldefs -e KernelMain -T linker.ld --static -o .\out\kernel.elf out\*.o .\font\hankaku.o .\out\kallsyms.o

powershell -Command "(Get-Content out\compile_commands.json) -replace '},$', '}' | Set-Content out\compile_commands.json"
echo ] >>out\compile_commands.json

copy out\BOOTX64.efi esp\EFI\BOOT
copy out\kernel.elf esp

..\tools\qemu\qemu-system-x86_64.exe -bios OVMF.fd -m 4096 -smp 4 -net none -drive file=fat:rw:esp,index=0,format=vvfat -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0

goto :eof

:add_to_compile_commands
set file=%1
set command=%2
set directory=%cd%
echo { >>out\compile_commands.json
echo  "directory": "%directory%", >>out\compile_commands.json
echo  "command": "%command%", >>out\compile_commands.json
echo  "file": "%file%" >>out\compile_commands.json
echo }, >>out\compile_commands.json
goto :eof
