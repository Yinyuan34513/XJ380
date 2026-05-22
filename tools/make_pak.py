#!/usr/bin/env python3
import os
import stat
import struct
import sys
from pathlib import Path


MAGIC = b"XJPAK10\x00"
ENTRY_FILE = 1
ENTRY_DIR = 2


def usage() -> int:
    print("usage: make_pak.py <root-dir> <output.pak>", file=sys.stderr)
    return 2


def rel_path(root: Path, path: Path) -> str:
    rel = path.relative_to(root).as_posix()
    if rel == ".":
        return "/"
    return "/" + rel


def collect(root: Path):
    entries = []
    for cur, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if d not in {".git", "__pycache__"})
        files = sorted(files)
        cur_path = Path(cur)
        if cur_path != root:
            st = cur_path.stat()
            entries.append((ENTRY_DIR, rel_path(root, cur_path), stat.S_IMODE(st.st_mode), b""))
        for name in files:
            p = cur_path / name
            if p.is_symlink():
                continue
            st = p.stat()
            data = p.read_bytes()
            entries.append((ENTRY_FILE, rel_path(root, p), stat.S_IMODE(st.st_mode), data))
    return entries


def main() -> int:
    if len(sys.argv) != 3:
        return usage()
    root = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    if not root.is_dir():
        print(f"make_pak.py: root is not a directory: {root}", file=sys.stderr)
        return 1

    entries = collect(root)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as f:
        f.write(struct.pack("<8sQ", MAGIC, len(entries)))
        for typ, path, mode, data in entries:
            path_bytes = path.encode("utf-8")
            if len(path_bytes) > 0xffff:
                print(f"make_pak.py: path too long: {path}", file=sys.stderr)
                return 1
            f.write(struct.pack("<HHIQ", len(path_bytes), typ, mode, len(data)))
            f.write(path_bytes)
            if typ == ENTRY_FILE:
                f.write(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
