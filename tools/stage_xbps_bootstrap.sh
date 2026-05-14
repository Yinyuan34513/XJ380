#!/bin/sh
set -eu

# Download and stage Void Linux packages into a prepared root.
# Uses xbps-static on the HOST (not inside QEMU).

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <image-root>" >&2
    exit 2
fi

root=$1
bf_dir=${BF_DIR:-Bf}
repo=${XBPS_REPOSITORY:-https://mirrors.tuna.tsinghua.edu.cn/voidlinux/current}
host_arch=${XBPS_STATIC_ARCH:-$(uname -m)}
case ${XBPS_CACHEDIR:-} in
/*) cache_dir=$XBPS_CACHEDIR ;;
"") cache_dir=$(pwd)/$bf_dir/xbps-cache ;;
*) cache_dir=$(pwd)/$XBPS_CACHEDIR ;;
esac
static_tarball=${XBPS_STATIC_TARBALL:-$bf_dir/xbps-static-latest.$host_arch-musl.tar.xz}
static_uri=${XBPS_STATIC_URI:-http://repo-default.voidlinux.org/static/xbps-static-latest.$host_arch-musl.tar.xz}

default_bootstrap=${XBPS_BOOTSTRAP_BASE:-base-minimal bash dash coreutils findutils sed grep gawk diffutils gzip tar bzip2 util-linux ncurses tzdata which fastfetch gcc binutils make strace inetutils bind-utils curl}
BOOTSTRAP_PKGS="${XBPS_BOOTSTRAP:-$default_bootstrap}"

find_xbps_static()
{
    for path in \
        "$bf_dir/usr/bin/xbps-install" \
        "$bf_dir/usr/bin/xbps-install.static" \
        "$bf_dir/bin/xbps-install" \
        "$bf_dir/xbps-install"
    do
        if [ -x "$path" ]; then
            printf '%s\n' "$path"
            return 0
        fi
    done
    return 1
}

prepare_xbps_static()
{
    if find_xbps_static >/dev/null 2>&1; then
        return 0
    fi

    if [ ! -f "$static_tarball" ]; then
        echo "missing xbps static tarball: $static_tarball" >&2
        echo "download it with:" >&2
        echo "  mkdir -p $bf_dir" >&2
        echo "  curl -fL '$static_uri' -o '$static_tarball'" >&2
        return 1
    fi

    echo "Extracting xbps-static: $static_tarball" >&2
    mkdir -p "$bf_dir"
    tar -xJf "$static_tarball" -C "$bf_dir" ./usr/bin ./var/db/xbps/keys
}

xbins()
{
    xbps_install=$1
    shift
    XBPS_ARCH=$host_arch \
        "$xbps_install" \
        -c "$cache_dir" \
        -r "$root" \
        --repository="$repo" \
        "$@"
}

xbins_noconf()
{
    xbps_install=$1
    shift
    XBPS_ARCH=$host_arch \
        "$xbps_install" \
        -C /dev/null \
        -c "$cache_dir" \
        -r "$root" \
        --repository="$repo" \
        "$@"
}

prepare_xbps_static || true

xbps_install=
if ! xbps_install=$(find_xbps_static); then
    xbps_install=
fi
if [ -z "$xbps_install" ]; then
    echo "xbps-static not found in $bf_dir, trying system xbps-install..." >&2
    if command -v xbps-install >/dev/null 2>&1; then
        xbps_install=$(command -v xbps-install)
    else
        echo "No xbps-install available." >&2
        echo "Download xbps-static from:" >&2
        echo "  http://repo-default.voidlinux.org/static/xbps-static-latest.$host_arch-musl.tar.xz" >&2
        exit 1
    fi
fi

echo "Using xbps-install: $xbps_install" >&2
echo "Repository: $repo" >&2
echo "Target root: $root" >&2

mkdir -p "$cache_dir"
mkdir -p "$root/var/db/xbps/keys"
mkdir -p "$root/etc/xbps.d"
mkdir -p "$root/usr/share/xbps.d"

printf 'repository=%s\n' "$repo" > "$root/usr/share/xbps.d/00-repository-main.conf"
printf 'repository=%s\n' "$repo" > "$root/etc/xbps.d/00-repository-main.conf"

if [ -d "$bf_dir/xbps-triggers" ]; then
    mkdir -p "$root/usr/libexec"
    cp -aL "$bf_dir/xbps-triggers" "$root/usr/libexec/xbps-triggers"
fi

# Copy any available keys from xbps-static or Bf
for keydir in "$bf_dir/var/db/xbps/keys" "$bf_dir/usr/var/db/xbps/keys"; do
    if [ -d "$keydir" ]; then
        cp -a "$keydir/." "$root/var/db/xbps/keys/"
    fi
done

echo "Downloading and installing: $BOOTSTRAP_PKGS" >&2

for pkg in $BOOTSTRAP_PKGS; do
    echo "  -> $pkg" >&2
done

xbins_noconf "$xbps_install" -Sy $BOOTSTRAP_PKGS

# Clean FAT32-incompatible characters from key filenames after xbps has used them.
for key in "$root"/var/db/xbps/keys/*:*; do
    [ -e "$key" ] || continue
    dir=$(dirname "$key")
    base=$(basename "$key")
    safe=$(printf '%s' "$base" | tr ':<>"\\|?*' '_')
    [ "$base" != "$safe" ] && mv -f "$key" "$dir/$safe"
done

echo "Bootstrap complete." >&2
echo "Installed into $root:" >&2
echo "  /usr/bin  - binaries" >&2
echo "  /usr/lib  - libraries" >&2
echo "  /usr/include - headers" >&2
echo "  /usr/share - data files" >&2
