#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <image-root>" >&2
    exit 2
fi

root=$1
stage_deps=${STAGE_ELF_DEPS:-tools/stage_elf_deps.sh}
busybox=${APP_BUSYBOX:-resources/apps/busybox}

find_real()
{
    tool=$1
    path=$(command -v "$tool" 2>/dev/null || true)
    if [ -z "$path" ]; then
        return 1
    fi
    readlink -f "$path"
}

copy_to()
{
    src=$1
    dst=$2
    [ -e "$src" ] || return 0
    mkdir -p "$(dirname "$dst")"
    cp -aL "$src" "$dst"
}

copy_same_path()
{
    src=$1
    [ -e "$src" ] || return 0
    copy_to "$src" "$root$src"
}

stage_ldd_deps()
{
    if [ -x "$stage_deps" ]; then
        "$stage_deps" "$root" "$@"
    elif [ -f "$stage_deps" ]; then
        sh "$stage_deps" "$root" "$@"
    fi
}

require_host_file()
{
    path=$1
    if [ ! -e "$path" ]; then
        echo "missing required host file: $path" >&2
        exit 1
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

stage_dpkg_bins()
{
    bins="
        /usr/bin/dpkg
        /usr/bin/dpkg-deb
        /usr/bin/dpkg-query
        /usr/bin/dpkg-divert
        /usr/bin/dpkg-maintscript-helper
        /usr/bin/dpkg-realpath
        /usr/bin/dpkg-split
        /usr/bin/dpkg-statoverride
        /usr/bin/dpkg-trigger
        /usr/bin/update-alternatives
        /usr/bin/diff
        /usr/sbin/start-stop-daemon
    "

    for bin in $bins; do
        require_host_file "$bin"
        copy_same_path "$bin"
    done
    stage_ldd_deps $bins

    if tar_real=$(find_real tar); then
        copy_to "$tar_real" "$root/usr/bin/tar"
        copy_to "$tar_real" "$root/bin/tar"
        copy_to "$tar_real" "$root/apps/tar"
        stage_ldd_deps "$tar_real"
    fi

    copy_to /usr/sbin/start-stop-daemon "$root/sbin/start-stop-daemon"
    copy_to /usr/sbin/start-stop-daemon "$root/usr/bin/start-stop-daemon"
    mkdir -p "$root/sbin" "$root/usr/sbin" "$root/usr/bin"
    for ldconfig_dst in "$root/sbin/ldconfig" "$root/usr/sbin/ldconfig" "$root/usr/bin/ldconfig"; do
        {
            printf '%s\n' '#!/bin/sh'
            printf '%s\n' 'exit 0'
        } > "$ldconfig_dst"
        chmod 755 "$ldconfig_dst"
    done
}

stage_dpkg_data()
{
    if [ -d /etc/dpkg ]; then
        cp -aL /etc/dpkg "$root/etc/"
    fi
    if [ -d /usr/share/dpkg ]; then
        cp -aL /usr/share/dpkg "$root/usr/share/"
    fi
    if [ -d /usr/libexec/dpkg ]; then
        cp -aL /usr/libexec/dpkg "$root/usr/libexec/"
    fi

    : > "$root/var/lib/dpkg/status"
    : > "$root/var/lib/dpkg/available"
    : > "$root/var/lib/dpkg/diversions"
    : > "$root/var/lib/dpkg/statoverride"
    : > "$root/var/lib/dpkg/triggers/Unincorp"
}

stage_busybox_links()
{
    require_host_file "$busybox"
    copy_to "$busybox" "$root/apps/busybox"

    for app in \
        sh ash bash cat chmod chown chgrp cp date echo env false grep ln ls mkdir mv printf pwd \
        readlink rm rmdir sed sort stat sync test touch tr true uname uniq unxz xz \
        xargs awk gzip gunzip bzip2 bunzip2 md5sum sha256sum which diff start-stop-daemon
    do
        install_busybox_applet "/bin/$app" "$app"
        install_busybox_applet "/usr/bin/$app" "$app"
    done
}

stage_dpkg_bins
stage_dpkg_data
stage_busybox_links
