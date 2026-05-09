# AGENTS.md

## IMPORTANT TIPS!

- If you want to patch codes, just do it, don't end your response proactively.

  ## Repo at a glance
- XJ380 is a freestanding x86_64 OS project built by the root `Makefile` (UEFI bootloader + kernel + user apps + loadable kernel modules).
- Boot flow:
  - `boot/bootx64.c` `efi_main` loads `\system\kernel.elf`
  - kernel entry is `KernelMain` in `kernel/main.cpp` (`linker.ld` sets `ENTRY(KernelMain)`).

## High-value boundaries
- `boot/`: UEFI bootloader (`out/BOOTX64.efi`).
- `kernel/`: core kernel (`out/kernel.elf`).
- `driver/`, `graphics/`, `font/`, `lib/`: linked into core kernel image.
- `kmod/`: loadable kernel modules; produces `out/*.km` copied into image `/system`.
- `user/`: user-space ELF apps; depends on `user/xapi` runtime objects.
- `frameworks/StardustUI/`: git submodule; example apps are copied into image apps during root build.

## Build prerequisites and gotchas
- Initialize submodule first: `git submodule update --init --recursive`.
- `make all` downloads `liballoc-x86_64.a` if missing (network required).
- Linux/WSL toolchain expected; key tools include `clang`, `nasm`, `gcc-mingw-w64-x86-64`, `mtools`, `dosfstools`, `qemu-system-x86_64`.
- Image creation uses privileged tools (`sgdisk`, `mkfs.vfat`, `mcopy`), so `make vdisk` may require sudo.

## Canonical commands
- Full build: `make all -j"$(nproc)"`.
- Build image: `make vdisk`.
- Build + run QEMU: `make run`.
- Run existing image only: `make justrun`.
- Format code: `make format`.
- Static checks: `make check`.

## Build switches that matter
- Defaults: `DEBUG=1`, `SMP=4`, `SUDO=1`, `KVM=1`.
- `SUDO=0`: disables sudo wrapper for QEMU invocation.
- `KVM=0`: falls back to `-cpu max` instead of KVM host CPU.
- `DISPLAY_BACKEND` is QEMU backend (`gtk` by default), not X11 `$DISPLAY`.
- If browser/httpget/nut linking cannot find clang builtins, pass `BROWSER_BUILTINS=/path/to/libclang_rt.builtins-x86_64.a`.

## Build ordering constraints
- Root `make all` intentionally runs in this order: kernel/boot artifacts -> `user build_xapi` -> user apps -> `kmod` -> StardustUI example copy.
- Many user app link steps depend on `out/xapi/*.o` and `out/xapi/liballoc-x86_64.a` produced by `make -C user build_xapi`.

## Generated files and cleanup
- Main artifacts: `out/`, `XJ380.img` (optional `XJ380.vmdk`), `serial.log`.
- Clean build artifacts: `make clean`.
- `.clangd` files are generated from `.clangd_template` files with `make gen.clangd` (remove with `make clean.clangd`).

## Module placement rule (from repo docs)
- Put code in `kmod/` only if it is intended to be a loadable module.
- Keep mandatory core-kernel code in `kernel/`.
- Keep built-in (non-loadable) drivers in `driver/`.