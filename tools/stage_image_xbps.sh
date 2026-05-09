#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <image-root>" >&2
    exit 2
fi

root=$1
stage_deps=${STAGE_ELF_DEPS:-tools/stage_elf_deps.sh}
busybox=${APP_BUSYBOX:-resources/apps/busybox}
repo=${XBPS_REPOSITORY:-https://repo-default.voidlinux.org/current}
static_tarball=${XBPS_STATIC_TARBALL:-/home/lu/naos/user/cache/xbps-static-latest.tar.xz}

find_tool()
{
    tool=$1
    if [ -x "resources/apps/$tool" ]; then
        printf '%s\n' "resources/apps/$tool"
        return 0
    fi
    command -v "$tool" 2>/dev/null || return 1
}

copy_to()
{
    src=$1
    dst=$2
    [ -e "$src" ] || return 0
    mkdir -p "$(dirname "$dst")"
    cp -aL "$src" "$dst"
}

stage_ldd_deps()
{
    if [ -x "$stage_deps" ]; then
        "$stage_deps" "$root" "$@"
    elif [ -f "$stage_deps" ]; then
        sh "$stage_deps" "$root" "$@"
    fi
}

install_busybox_applet()
{
    dst=$1
    app=$2
    mkdir -p "$(dirname "$root$dst")"
    if [ -e "$root$dst" ]; then
        return
    fi
    if [ "$dst" = "/bin/sh" ]; then
        cp -aL "$busybox" "$root$dst"
        return
    fi

    {
        printf '%s\n' '#!/bin/sh'
        printf 'exec /apps/busybox %s "$@"\n' "$app"
    } > "$root$dst"
    chmod 755 "$root$dst"
}

stage_xbps_dirs()
{
    mkdir -p \
        "$root/etc/xbps.d" \
        "$root/usr/bin" \
        "$root/var/cache/xbps" \
        "$root/var/db/xbps/keys" \
        "$root/var/db/xbps/metadata"
}

stage_xbps_config()
{
    conf="$root/etc/xbps.d/00-repository-main.conf"
    if [ ! -s "$conf" ]; then
        printf 'repository=%s\n' "$repo" > "$conf"
    fi
}

stage_xbps_tools()
{
    staged=""
    required="xbps-install xbps-query xbps-remove xbps-uhelper"
    optional="xbps-rindex xbps-reconfigure xbps-alternatives xbps-pkgdb"

    for tool in $required; do
        if ! src=$(find_tool "$tool"); then
            echo "missing $tool; install xbps on host or place resources/apps/$tool" >&2
            exit 1
        fi
        copy_to "$src" "$root/usr/bin/$tool"
        staged="$staged $src"
    done

    for tool in $optional; do
        if src=$(find_tool "$tool"); then
            copy_to "$src" "$root/usr/bin/$tool"
            staged="$staged $src"
        fi
    done

    stage_ldd_deps $staged
}

stage_xbps_static_tarball()
{
    [ -f "$static_tarball" ] || return 1
    tar -xJf "$static_tarball" -C "$root" ./usr/bin ./var/db/xbps/keys

    for tool in xbps-install xbps-query xbps-remove xbps-uhelper \
        xbps-rindex xbps-reconfigure xbps-alternatives xbps-pkgdb \
        xbps-fetch xbps-digest; do
        if [ -x "$root/usr/bin/$tool.static" ]; then
            rm -f "$root/usr/bin/$tool"
            cp -aL "$root/usr/bin/$tool.static" "$root/usr/bin/$tool"
        fi
    done
    return 0
}

stage_busybox_links()
{
    if [ -e "$busybox" ]; then
        copy_to "$busybox" "$root/apps/busybox"
        for app in \
            sh ash cat chmod chown chgrp cp date echo env false grep ln ls mkdir mv printf pwd \
            readlink rm rmdir sed sort stat sync test touch tr true uname uniq xargs awk
        do
            install_busybox_applet "/bin/$app" "$app"
            install_busybox_applet "/usr/bin/$app" "$app"
        done
    fi
}

stage_xbps_dirs
stage_xbps_config
if ! stage_xbps_static_tarball; then
    stage_xbps_tools
fi
stage_busybox_links
