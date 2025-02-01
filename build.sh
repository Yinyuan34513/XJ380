#!/usr/bin/sh
rm -rf ./out ./esp
mkdir -p ./out
mkdir -p ./esp/EFI/BOOT

ASM_FLAGS="-O0 -Wall -g -nostdlib -m64 -ffreestanding -fno-exceptions"
C_FLAGS="-O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -nostdinc"
CPP_FLAGS="-O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit"

ASM_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.S")
C_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.c")
CPP_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.cpp")

echo "[" >out/compile_commands.json

add_to_compile_commands() {
  local file=$1
  local command=$2
  local directory=$(pwd)
  echo "{" >>out/compile_commands.json
  echo "  \"directory\": \"$directory\"," >>out/compile_commands.json
  echo "  \"command\": \"$command\"," >>out/compile_commands.json
  echo "  \"file\": \"$file\"" >>out/compile_commands.json
  echo "}," >>out/compile_commands.json
}

x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

counter=1
for file in $ASM_FILES; do
  add_to_compile_commands "$file" "gcc $file $ASM_FLAGS -c -o ./out/$counter.o"
  x86_64-linux-gnu-gcc $file $ASM_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done
for file in $C_FILES; do
  add_to_compile_commands "$file" "gcc $file $C_FLAGS -c -o ./out/$counter.o"
  x86_64-linux-gnu-gcc $file $C_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done
for file in $CPP_FILES; do
  add_to_compile_commands "$file" "g++ $file $CPP_FLAGS -c -o ./out/$counter.o"
  x86_64-linux-gnu-g++-14 $file $CPP_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done

x86_64-linux-gnu-ld -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(find ./out -name "*.o") ./font/hankaku.o
gcc kallsyms/kallsyms.c -o ./out/kallsyms
x86_64-linux-gnu-nm -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
x86_64-linux-gnu-gcc -O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -nostdinc -c ./out/kallsyms.S -o ./out/kallsyms.o
x86_64-linux-gnu-ld -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o

truncate -s-2 out/compile_commands.json
echo "]" >>out/compile_commands.json

cp ./out/BOOTX64.efi ./esp/EFI/BOOT
cp ./out/kernel.elf ./esp

qemu-system-x86_64 -bios OVMF.fd -m 4096 -smp 4 -net none -drive if=none,id=harddisk,file=fat:rw:esp,index=0,format=vvfat -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0
