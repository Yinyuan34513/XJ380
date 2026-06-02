# KMOD KNOWLEDGE BASE

## OVERVIEW
Loadable kernel module subtree. Modules build as shared `.sys` artifacts, load through the kernel dynamic linker, and are copied into the disk image `/mod`.

## STRUCTURE
```
kmod/
├── Makefile         # Dispatches e1000, netserver, xhci
├── e1000/           # Intel E1000 network driver module
├── netserver/       # lwIP-backed networking module
├── xhci/            # USB xHCI module
└── dlinker_test/    # Module dependency/linker test, not built by default
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Module policy | `README.md` | Boundary rule for `kmod/` vs `kernel/` vs `driver/`. |
| Module loader | `kernel/dlinker.cpp`, `include/dlinker.h` | Loads `.sys`, resolves symbols/deps. |
| Top-level build | `Makefile` | Builds e1000 -> netserver -> xhci. |
| Module entry | `*/Makefile`, `*/*.cpp` | Linker entry symbol is `dlmain`. |
| Image copy | root `Makefile` | Copies `out/*.sys` into image `/mod`. |

## CONVENTIONS
- Put code here only if it is intended to be linked as a shared loadable kernel module.
- Module C/C++ flags are kernel-like but add `-fPIC` and `-fvisibility=hidden`.
- Module link commands use `-shared` and `-Wl,-e,dlmain`; some modules also use `-zmuldefs` or `-Bsymbolic`.
- Export/import shared kernel symbols through the existing dlinker/kernel symbol mechanism.
- Normal root `kmod/Makefile` does not build `dlinker_test`.
- Keep vendored code inside module subtrees documented as integration only; avoid local edits to upstream internals.

## ANTI-PATTERNS
- Do not put mandatory boot drivers in `kmod/`; safe mode/boot flags can disable module loading.
- Do not create Linux-style `init_module` expectations; this repo uses `dlmain`.
- Do not assume `.sys` files land under `/system`; root image copy places `out/*.sys` into `/mod`.
- Do not edit module object order casually where a Makefile hardcodes link order.

## VERIFY
`make -C kmod` for module-only builds; `make all -j"$(nproc)"` and `make vdisk` to verify image staging.
