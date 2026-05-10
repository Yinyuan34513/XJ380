#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <image-root>" >&2
    exit 2
fi

root=$1
bf_dir=${BF_DIR:-Bf}

stage_base_dirs()
{
    mkdir -p \
        "$root/EFI/BOOT" \
        "$root/apps/builtin" \
        "$root/apps/system" \
        "$root/apps/thirdparty" \
        "$root/bin" \
        "$root/boot" \
        "$root/dev" \
        "$root/etc/apt/apt.conf.d" \
        "$root/etc/dpkg/dpkg.cfg.d" \
        "$root/etc/fastfetch" \
        "$root/etc/ssl/certs" \
        "$root/etc/xbps.d" \
        "$root/home" \
        "$root/lib" \
        "$root/lib64" \
        "$root/media" \
        "$root/mnt" \
        "$root/mod" \
        "$root/opt" \
        "$root/proc" \
        "$root/root" \
        "$root/run/lock" \
        "$root/sbin" \
        "$root/sys" \
        "$root/system/config" \
        "$root/system/font" \
        "$root/system/foxland" \
        "$root/system/icon" \
        "$root/system/resources/image" \
        "$root/system/theme/skyglass" \
        "$root/tmp" \
        "$root/usr/bin" \
        "$root/usr/include" \
        "$root/usr/lib" \
        "$root/usr/libexec" \
        "$root/usr/local/bin" \
        "$root/usr/local/lib" \
        "$root/usr/local/sbin" \
        "$root/usr/sbin" \
        "$root/usr/share/doc" \
        "$root/usr/share/info" \
        "$root/usr/share/lintian/overrides" \
        "$root/usr/share/man/man1" \
        "$root/usr/share/man/man2" \
        "$root/usr/share/man/man3" \
        "$root/usr/share/man/man4" \
        "$root/usr/share/man/man5" \
        "$root/usr/share/man/man6" \
        "$root/usr/share/man/man7" \
        "$root/usr/share/man/man8" \
        "$root/usr/share/terminfo" \
        "$root/users" \
        "$root/users/Another User" \
        "$root/users/GuoqiFish" \
        "$root/var/backups" \
        "$root/var/cache/apt/archives/partial" \
        "$root/var/cache/debconf" \
        "$root/var/cache/xjapt/cache" \
        "$root/var/lib/apt/lists/partial" \
        "$root/var/lib/dpkg/alternatives" \
        "$root/var/lib/dpkg/info" \
        "$root/var/lib/dpkg/parts" \
        "$root/var/lib/dpkg/triggers" \
        "$root/var/lib/dpkg/updates" \
        "$root/var/cache/xbps" \
        "$root/var/db/xbps/keys" \
        "$root/var/db/xbps/metadata" \
        "$root/var/local" \
        "$root/var/lock" \
        "$root/var/log" \
        "$root/var/mail" \
        "$root/var/opt" \
        "$root/var/spool" \
        "$root/var/tmp"

    chmod 1777 "$root/tmp" "$root/var/tmp"
}

stage_base_files()
{
    printf "nameserver 223.5.5.5\n" > "$root/etc/resolv.conf"

    if [ -f resources/etc/os-release ]; then
        cp -a resources/etc/os-release "$root/etc/os-release"
    fi
    if [ -d resources/etc/fastfetch ]; then
        cp -a resources/etc/fastfetch/. "$root/etc/fastfetch/"
    fi
    if [ -f /etc/ssl/certs/ca-certificates.crt ]; then
        cp -a /etc/ssl/certs/ca-certificates.crt "$root/etc/ssl/certs/ca-certificates.crt"
    fi
    if [ -d "$bf_dir/terminfo" ]; then
        cp -aL "$bf_dir/terminfo/." "$root/usr/share/terminfo/"
    fi
}

stage_base_dirs
stage_base_files
