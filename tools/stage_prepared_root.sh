#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <prepared-root> <image-root>" >&2
    exit 2
fi

src_root=${1%/}
dst_root=${2%/}

if [ ! -d "$src_root" ]; then
    echo "prepared root not found: $src_root" >&2
    exit 1
fi

stage_entry()
{
    src=$1
    rel=${src#"$src_root"/}
    dst=$dst_root/$rel

    case $rel in
    Bf|Bf/*|var/cache/xbps|var/cache/xbps/*)
        return 0
        ;;
    esac

    if [ -L "$src" ]; then
        if [ -d "$dst" ] && [ ! -L "$dst" ]; then
            return 0
        fi
        rm -rf "$dst"
        mkdir -p "$(dirname "$dst")"
        cp -a "$src" "$dst"
        return 0
    fi

    if [ -d "$src" ]; then
        if [ -L "$dst" ] || { [ -e "$dst" ] && [ ! -d "$dst" ]; }; then
            rm -rf "$dst"
        fi
        mkdir -p "$dst"
        return 0
    fi

    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
}

find "$src_root" -mindepth 1 | while IFS= read -r src; do
    stage_entry "$src"
done
