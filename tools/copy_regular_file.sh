#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <source> <destination> [mode]" >&2
    exit 2
fi

src=$1
dst=$2
mode=${3:-755}

if [ ! -f "$src" ]; then
    echo "source file not found: $src" >&2
    exit 1
fi

echo "COPY $src -> $dst"

if ! rm -f "$dst"; then
    echo "cannot remove destination: $dst" >&2
    exit 1
fi

if ! mkdir -p "$(dirname "$dst")"; then
    echo "cannot create destination directory: $(dirname "$dst")" >&2
    exit 1
fi

if ! cp --reflink=never --sparse=never "$src" "$dst"; then
    echo "copy failed: $src -> $dst" >&2
    exit 1
fi

if ! chmod "$mode" "$dst"; then
    echo "chmod $mode failed: $dst" >&2
    exit 1
fi

if ! cmp -s "$src" "$dst"; then
    echo "copy verification failed: $src -> $dst" >&2
    wc -c "$src" "$dst" >&2 || true
    exit 1
fi
