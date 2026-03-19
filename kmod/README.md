# Kernel Modules

`kmod/` contains loadable kernel modules that are built into `.km` artifacts
and copied into `/lib/modules` in the disk image.

Current layout:

- `kmod/e1000/`: loadable Intel E1000 network driver
- `kmod/netserver/`: networking module sources and third-party stack code

Boundary rule:

- Put code here only if it is intended to be linked as a shared kernel module.
- Keep code in `kernel/` only if it must be part of the core kernel image.
- Keep code in `driver/` only if it is a built-in driver linked into `kernel.elf`.

Compatibility:

- The legacy `mod/Makefile` remains as a thin wrapper so old build commands do
  not fail immediately.
