#!/bin/sh
set -eu

if [ "$#" -ne 5 ]; then
    echo "usage: $0 <iso-root> <bootx64.efi> <installer-kernel.elf> <installer-root.pak> <system-payload.pak>" >&2
    exit 2
fi

iso_root=$1
boot_efi=$2
kernel=$3
installer_root=$4
system_payload=$5
out_iso=XJ380-installer.iso
esp_img="$iso_root/EFI/BOOT/efiboot.img"

for f in "$boot_efi" "$kernel" "$installer_root" "$system_payload"; do
    if [ ! -f "$f" ]; then
        echo "missing installer input: $f" >&2
        exit 1
    fi
done

if ! command -v xorriso >/dev/null 2>&1; then
    echo "xorriso is required to build the UEFI installer ISO" >&2
    exit 1
fi
if ! command -v mcopy >/dev/null 2>&1 || ! command -v mmd >/dev/null 2>&1; then
    echo "mtools is required to build the UEFI installer ISO" >&2
    exit 1
fi
if ! command -v mkfs.vfat >/dev/null 2>&1; then
    echo "dosfstools mkfs.vfat is required to build the UEFI installer ISO" >&2
    exit 1
fi

rm -rf "$iso_root"
mkdir -p "$iso_root/EFI/BOOT"
cp -a "$boot_efi" "$iso_root/EFI/BOOT/BOOTX64.EFI"

payload_kib=$(du -sk "$boot_efi" "$kernel" "$installer_root" "$system_payload" | awk '{s += $1} END {print s + 8192}')
esp_mib=$(( (payload_kib + 1023) / 1024 ))
if [ "$esp_mib" -lt 32 ]; then
    esp_mib=32
fi

dd if=/dev/zero of="$esp_img" bs=1M count="$esp_mib" status=none
mkfs.vfat -F 32 "$esp_img" >/dev/null
mmd -i "$esp_img" ::EFI ::EFI/BOOT ::installer
mcopy -i "$esp_img" "$iso_root/EFI/BOOT/BOOTX64.EFI" ::EFI/BOOT/BOOTX64.EFI
mcopy -i "$esp_img" "$kernel" ::installer/installer-kernel.elf
mcopy -i "$esp_img" "$installer_root" ::installer/installer-root.pak
mcopy -i "$esp_img" "$system_payload" ::installer/system-payload.pak

xorriso -as mkisofs \
    -iso-level 3 \
    -V XJ380_INSTALLER \
    -eltorito-alt-boot \
    -e EFI/BOOT/efiboot.img \
    -no-emul-boot \
    -o "$out_iso" \
    "$iso_root" >/dev/null

echo "$out_iso"
