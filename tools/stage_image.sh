#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
cd "$repo_root"

BUILD_DIR=${BUILD_DIR:-out}
APP_BUSYBOX=${APP_BUSYBOX:-resources/apps/busybox}
APP_FASTFETCH=${APP_FASTFETCH:-resources/apps/fastfetch}
BF_DIR=${BF_DIR:-Bf}
HOST_GCC=${HOST_GCC:-gcc}
HOST_GCC_TRIPLE=${HOST_GCC_TRIPLE:-$("$HOST_GCC" -dumpmachine 2>/dev/null || true)}
HOST_CLANG=${HOST_CLANG:-clang}
HOST_CLANGXX=${HOST_CLANGXX:-clang++}
HOST_LLVM_AR=${HOST_LLVM_AR:-llvm-ar}
HOST_LLVM_RANLIB=${HOST_LLVM_RANLIB:-llvm-ranlib}
IMAGE_BUSYBOX=${IMAGE_BUSYBOX:-0}
IMAGE_PACKAGE_MANAGER=${IMAGE_PACKAGE_MANAGER:-xbps}
IMAGE_TOOLCHAIN=${IMAGE_TOOLCHAIN:-clang}
SELFHOST_USER_THIRD_PARTY_DEFAULT='third_party/mbedtls-src third_party/litehtml third_party/libvterm'
SELFHOST_USER_THIRD_PARTY=${SELFHOST_USER_THIRD_PARTY:-$SELFHOST_USER_THIRD_PARTY_DEFAULT}
SYSTEM_APPS=${SYSTEM_APPS:-shell.elf fmanager.elf taskmgr.elf ctrlmenu.elf}
BUILTIN_APPS=${BUILTIN_APPS:-calc.elf texter.elf picturer.elf browser.elf nut.elf}
USER_APPS=${USER_APPS:-busyterm.elf bcms-sp.elf sigtest.elf posixdemo.elf libctest.elf dyn-hello}
STARDUSTUI_BUILTIN_APPS=${STARDUSTUI_BUILTIN_APPS:-helloworld.elf layout.elf duckchat.elf}
MUSL_RUNTIME_FILES=${MUSL_RUNTIME_FILES:-ld-musl-x86_64.so.1 libc.so libc.musl-x86_64.so.1 libgcc_s.so.1}
XBPS_PREPARE_ROOT=${XBPS_PREPARE_ROOT:-$BF_DIR/xbps-root}
XBPS_BOOTSTRAP_BASE=${XBPS_BOOTSTRAP_BASE:-base-minimal bash dash coreutils findutils sed grep gawk diffutils gzip tar bzip2 util-linux ncurses tzdata which fastfetch gcc binutils make strace inetutils bind-utils curl}
XBPS_BOOTSTRAP=${XBPS_BOOTSTRAP:-$XBPS_BOOTSTRAP_BASE}

usage()
{
    cat >&2 <<'EOF'
usage: stage_image.sh <command> [args]

commands:
  base <image-root>
  elf-deps <image-root> <elf>...
  xbps-bootstrap <prepared-root>
  prepared-root <prepared-root> <image-root>
  xbps <image-root>
  toolchain <image-root>
  selfhost-user <image-root>
  populate-root <image-root>
EOF
    exit 2
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
    same_src=$1

    [ -e "$same_src" ] || return 0

    copy_to "$same_src" "$root$same_src"
}

copy_list_to_root_dir()
{
    dst_dir=$1
    shift

    mkdir -p "$root/$dst_dir"
    for name do
        [ -e "$BUILD_DIR/$name" ] || continue
        cp -a "$BUILD_DIR/$name" "$root/$dst_dir/"
    done
}

copy_contents_to_root_dir()
{
    src_dir=$1
    dst_dir=$2

    [ -d "$src_dir" ] || return 0

    mkdir -p "$root/$dst_dir"
    cp -a "$src_dir/." "$root/$dst_dir/"
}

copy_glob_to_dir()
{
    dst_dir=$1
    shift

    mkdir -p "$dst_dir"
    for pattern do
        for f in $pattern; do
            [ -e "$f" ] || continue
            cp -aL "$f" "$dst_dir/"
        done
    done
}

sanitize_xbps_keys_for_fat()
{
    for key in "$root"/var/db/xbps/keys/*:*; do
        [ -e "$key" ] || continue
        dir=$(dirname "$key")
        base=$(basename "$key")
        safe=$(printf '%s' "$base" | tr ':<>"\\|?*' '_')
        [ "$base" = "$safe" ] || mv -f "$key" "$dir/$safe"
    done
}

flatten_root_links()
{
    find "$root" -xtype l -delete
    find "$root" -type l -exec sh -c '
        for lnk do
            tmp=${lnk}.xj380-copy
            if cp -aL "$lnk" "$tmp" 2>/dev/null; then
                rm -f "$lnk"
                mv "$tmp" "$lnk"
            else
                rm -rf "$tmp"
                rm -f "$lnk"
            fi
        done
    ' sh {} +
}

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
    printf 'nameserver 223.5.5.5\n' > "$root/etc/resolv.conf"

    cat > "$root/etc/os-release" <<'EOF'
NAME="XJ380"
PRETTY_NAME="XINGJI XJ380 Singularity 1.0.0 XSK 2.1.0"
ID=xj380
VERSION_ID="6.6"
HOME_URL="https://github.com/xingji-studio/XJ380"
EOF

    if [ -d resources/etc/fastfetch ]; then
        cp -a resources/etc/fastfetch/. "$root/etc/fastfetch/"
    fi
    if [ -f /etc/ssl/certs/ca-certificates.crt ]; then
        copy_to /etc/ssl/certs/ca-certificates.crt "$root/etc/ssl/certs/ca-certificates.crt"
    fi
    if [ -d "$BF_DIR/terminfo" ]; then
        cp -aL "$BF_DIR/terminfo/." "$root/usr/share/terminfo/"
    fi
}

cmd_base()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}
    stage_base_dirs
    stage_base_files
}

copy_elf_dep()
{
    src=$1

    case "$src" in
        /*) ;;
        *) return 0 ;;
    esac

    [ -e "$src" ] || return 0

    copy_to "$src" "$root$src"
}

cmd_elf_deps()
{
    [ "$#" -ge 2 ] || usage

    root=${1%/}
    shift

    command -v ldd >/dev/null 2>&1 || return 0

    for elf do
        if ! ldd "$elf" >/dev/null 2>&1; then
            continue
        fi

        ldd "$elf" | while IFS= read -r line; do
            set -- $line
            case "$line" in
                *"=>"*)
                    copy_elf_dep "${3:-}"
                    ;;
                /*)
                    copy_elf_dep "$1"
                    ;;
            esac
        done
    done
}

find_xbps_static()
{
    for path in \
        "$BF_DIR/usr/bin/xbps-install" \
        "$BF_DIR/usr/bin/xbps-install.static" \
        "$BF_DIR/bin/xbps-install" \
        "$BF_DIR/xbps-install"
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
        echo "  mkdir -p $BF_DIR" >&2
        echo "  curl -fL '$static_uri' -o '$static_tarball'" >&2
        return 1
    fi

    echo "Extracting xbps-static: $static_tarball" >&2
    mkdir -p "$BF_DIR"
    tar -xJf "$static_tarball" -C "$BF_DIR" ./usr/bin ./var/db/xbps/keys
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

cmd_xbps_bootstrap()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}
    repo=${XBPS_REPOSITORY:-https://mirrors.tuna.tsinghua.edu.cn/voidlinux/current}
    host_arch=${XBPS_STATIC_ARCH:-$(uname -m)}
    static_tarball=${XBPS_STATIC_TARBALL:-$BF_DIR/xbps-static-latest.$host_arch-musl.tar.xz}
    static_uri=${XBPS_STATIC_URI:-http://repo-default.voidlinux.org/static/xbps-static-latest.$host_arch-musl.tar.xz}

    case ${XBPS_CACHEDIR:-} in
        /*) cache_dir=$XBPS_CACHEDIR ;;
        "") cache_dir=$(pwd)/$BF_DIR/xbps-cache ;;
        *) cache_dir=$(pwd)/$XBPS_CACHEDIR ;;
    esac

    prepare_xbps_static || true

    xbps_install=
    if ! xbps_install=$(find_xbps_static); then
        xbps_install=
    fi
    if [ -z "$xbps_install" ]; then
        echo "xbps-static not found in $BF_DIR, trying system xbps-install..." >&2
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

    if [ -d "$BF_DIR/xbps-triggers" ]; then
        mkdir -p "$root/usr/libexec"
        cp -aL "$BF_DIR/xbps-triggers" "$root/usr/libexec/xbps-triggers"
    fi

    for keydir in "$BF_DIR/var/db/xbps/keys" "$BF_DIR/usr/var/db/xbps/keys"; do
        if [ -d "$keydir" ]; then
            cp -a "$keydir/." "$root/var/db/xbps/keys/"
        fi
    done

    echo "Downloading and installing: $XBPS_BOOTSTRAP" >&2
    for pkg in $XBPS_BOOTSTRAP; do
        echo "  -> $pkg" >&2
    done

    xbins_noconf "$xbps_install" -Sy $XBPS_BOOTSTRAP
    sanitize_xbps_keys_for_fat

    echo "Bootstrap complete." >&2
}

stage_prepared_entry()
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

link_bin_tool()
{
    tool=$1
    target=$2
    dst=$dst_root/bin/$tool

    if [ ! -x "$dst_root/$target" ]; then
        return 0
    fi
    if [ -d "$dst" ] && [ ! -L "$dst" ]; then
        return 0
    fi

    rm -f "$dst"
    ln -s "../$target" "$dst"
}

cmd_prepared_root()
{
    [ "$#" -eq 2 ] || usage

    src_root=${1%/}
    dst_root=${2%/}

    if [ ! -d "$src_root" ]; then
        echo "prepared root not found: $src_root" >&2
        exit 1
    fi

    find "$src_root" -mindepth 1 | while IFS= read -r src; do
        stage_prepared_entry "$src"
    done

    mkdir -p "$dst_root/bin" "$dst_root/usr/bin"

    if [ -x "$dst_root/usr/bin/dash" ]; then
        link_bin_tool sh usr/bin/dash
    elif [ -x "$dst_root/usr/bin/bash" ]; then
        link_bin_tool sh usr/bin/bash
    fi

    for tool in \
        awk basename cat chmod chgrp chown cp cut date dd df dirname du echo env false find grep \
        gunzip gzip head install ln ls mkdir mv printf pwd readlink realpath rm rmdir sed sh sleep \
        sort stat sync tail tar test touch tr true uname uniq wc which xargs
    do
        [ "$tool" = "sh" ] && continue
        link_bin_tool "$tool" "usr/bin/$tool"
    done
}

find_tool_in_bf()
{
    tool=$1
    for path in \
        "$BF_DIR/usr/bin/$tool" \
        "$BF_DIR/bin/$tool" \
        "$BF_DIR/$tool"
    do
        if [ -x "$path" ]; then
            printf '%s\n' "$path"
            return 0
        fi
    done
    return 1
}

install_busybox_applet()
{
    dst=$1
    app=$2

    mkdir -p "$(dirname "$root$dst")"
    if [ -e "$root$dst" ]; then
        return 0
    fi
    if [ "$dst" = "/bin/sh" ]; then
        cp -aL "$busybox" "$root$dst"
        return 0
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
        "$root/usr/share/xbps.d" \
        "$root/usr/bin" \
        "$root/usr/libexec" \
        "$root/var/cache/xbps" \
        "$root/var/db/xbps/keys" \
        "$root/var/db/xbps/metadata"
}

stage_xbps_config()
{
    for dir in "$root/usr/share/xbps.d" "$root/etc/xbps.d"; do
        mkdir -p "$dir"
        conf=$dir/00-repository-main.conf
        printf 'repository=%s\n' "$repo" > "$conf"
    done

    for conf in "$root"/usr/share/xbps.d/*-repository-*.conf \
                "$root"/usr/share/xbps.d/*repository*.conf \
                "$root"/etc/xbps.d/*-repository-*.conf \
                "$root"/etc/xbps.d/*repository*.conf; do
        [ -f "$conf" ] || continue
        sed -i \
            -e 's|https://repo-default.voidlinux.org|http://mirrors.tuna.tsinghua.edu.cn/voidlinux|g' \
            -e 's|https://alpha.de.repo.voidlinux.org|http://mirrors.tuna.tsinghua.edu.cn/voidlinux|g' \
            "$conf"
    done
}

stage_xbps_tools()
{
    required='xbps-install xbps-query xbps-remove xbps-uhelper'
    optional='xbps-rindex xbps-reconfigure xbps-alternatives xbps-pkgdb'

    for tool in $required; do
        if ! src=$(find_tool_in_bf "$tool"); then
            echo "missing $tool in $BF_DIR" >&2
            echo "preferred: download $static_uri to $static_tarball" >&2
            exit 1
        fi
        copy_to "$src" "$root/usr/bin/$tool"
    done

    for tool in $optional; do
        if src=$(find_tool_in_bf "$tool"); then
            copy_to "$src" "$root/usr/bin/$tool"
        fi
    done
}

stage_xbps_static_tarball()
{
    if [ ! -f "$static_tarball" ]; then
        echo "missing xbps static tarball: $static_tarball" >&2
        echo "download it with:" >&2
        echo "  mkdir -p $BF_DIR" >&2
        echo "  curl -fL '$static_uri' -o '$static_tarball'" >&2
        return 1
    fi

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

stage_xbps_triggers()
{
    if [ -d "$triggers_dir" ]; then
        mkdir -p "$root/usr/libexec"
        cp -aL "$triggers_dir" "$root/usr/libexec/xbps-triggers"
        return 0
    fi

    if [ -z "$triggers_pkg" ]; then
        for pkg in "$BF_DIR"/xbps-triggers-*.xbps; do
            [ -f "$pkg" ] || continue
            triggers_pkg=$pkg
            break
        done
    fi

    if [ -n "$triggers_pkg" ] && [ -f "$triggers_pkg" ]; then
        tar --zstd -xf "$triggers_pkg" -C "$root" ./usr/libexec/xbps-triggers
        return 0
    fi

    echo "missing xbps triggers" >&2
    echo "place triggers in $triggers_dir or put xbps-triggers-*.xbps under $BF_DIR" >&2
    exit 1
}

stage_busybox_links()
{
    if [ "$IMAGE_BUSYBOX" != "1" ]; then
        echo "skip busybox applet staging (IMAGE_BUSYBOX=$IMAGE_BUSYBOX)"
        return 0
    fi

    if [ -e "$busybox" ] && [ ! -e "$root/apps/busybox" ]; then
        copy_to "$busybox" "$root/apps/busybox"
    fi
    if [ ! -e "$root/apps/busybox" ]; then
        return 0
    fi

    for app in \
        sh ash cat chmod chown chgrp cp date echo env false grep install ln ls mkdir mv printf pwd \
        readlink rm rmdir sed sort stat sync test touch tr true uname uniq xargs awk
    do
        install_busybox_applet "/bin/$app" "$app"
        install_busybox_applet "/usr/bin/$app" "$app"
    done
}

cmd_xbps()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}
    busybox=$APP_BUSYBOX
    repo=${XBPS_REPOSITORY:-http://mirrors.tuna.tsinghua.edu.cn/voidlinux/current}
    host_arch=${XBPS_STATIC_ARCH:-$(uname -m)}
    static_tarball=${XBPS_STATIC_TARBALL:-$BF_DIR/xbps-static-latest.$host_arch-musl.tar.xz}
    static_uri=${XBPS_STATIC_URI:-http://repo-default.voidlinux.org/static/xbps-static-latest.$host_arch-musl.tar.xz}
    triggers_dir=${XBPS_TRIGGERS_DIR:-$BF_DIR/xbps-triggers}
    triggers_pkg=${XBPS_TRIGGERS_PKG:-}

    stage_xbps_dirs
    if ! stage_xbps_static_tarball; then
        stage_xbps_tools
    fi
    stage_xbps_config
    sanitize_xbps_keys_for_fat
    stage_xbps_triggers
    stage_busybox_links
}

find_real()
{
    tool=$1
    path=$(command -v "$tool" 2>/dev/null || true)
    [ -n "$path" ] || return 1
    readlink -f "$path"
}

require_real()
{
    tool=$1
    path=$(find_real "$tool" || true)

    if [ -z "$path" ]; then
        echo "missing required host tool: $tool" >&2
        exit 1
    fi

    printf '%s\n' "$path"
}

copy_ldd_named_deps_to_dir()
{
    dir=$1
    shift

    mkdir -p "$dir"
    for elf do
        [ -e "$elf" ] || continue
        ldd "$elf" 2>/dev/null | while IFS= read -r line; do
            set -- $line
            case "$line" in
                *"=>"*)
                    [ -n "${3:-}" ] || continue
                    case "$1" in
                        libLLVM.so.*|libclang-cpp.so.*)
                            copy_to "$3" "$dir/$1"
                            ;;
                    esac
                    ;;
            esac
        done
    done
}

stage_non_llvm_ldd_deps()
{
    for elf do
        [ -e "$elf" ] || continue
        ldd "$elf" 2>/dev/null | while IFS= read -r line; do
            set -- $line
            case "$line" in
                *"=>"*)
                    case "${1:-}" in
                        libLLVM.so.*|libclang-cpp.so.*)
                            continue
                            ;;
                    esac
                    [ -n "${3:-}" ] || continue
                    copy_same_path "$3"
                    ;;
                /*)
                    copy_same_path "$1"
                    ;;
            esac
        done
    done
}

gcc_triple()
{
    "$HOST_GCC" -dumpmachine 2>/dev/null
}

gcc_version()
{
    "$HOST_GCC" -dumpversion 2>/dev/null
}

gcc_libdir()
{
    libgcc=$("$HOST_GCC" -print-file-name=libgcc.a 2>/dev/null)
    dirname "$libgcc"
}

gcc_libexecdir()
{
    cc1=$("$HOST_GCC" -print-prog-name=cc1 2>/dev/null)
    dirname "$cc1"
}

stage_glibc_devel()
{
    triple=$1

    rm -rf "$root/usr/include"
    mkdir -p "$root/usr/include" "$root/usr/lib/$triple" "$root/lib/$triple"
    cp -a /usr/include/. "$root/usr/include/"

    copy_glob_to_dir "$root/usr/lib/$triple" \
        "/usr/lib/$triple/crt*.o" \
        "/usr/lib/$triple/Scrt*.o" \
        "/usr/lib/$triple/Mcrt*.o" \
        "/usr/lib/$triple/libc.so" \
        "/usr/lib/$triple/libc_nonshared.a" \
        "/usr/lib/$triple/libm.so" \
        "/usr/lib/$triple/libmvec_nonshared.a" \
        "/usr/lib/$triple/libpthread_nonshared.a" \
        "/usr/lib/$triple/libstdc++.so*"

    copy_glob_to_dir "$root/lib/$triple" \
        "/lib/$triple/crt*.o" \
        "/lib/$triple/Scrt*.o" \
        "/lib/$triple/Mcrt*.o" \
        "/lib/$triple/libbfd-*.so" \
        "/lib/$triple/libctf.so*" \
        "/lib/$triple/libjansson.so*" \
        "/lib/$triple/libsframe.so*" \
        "/lib/$triple/libz.so*" \
        "/lib/$triple/libzstd.so*" \
        "/lib/$triple/libc.so*" \
        "/lib/$triple/libm.so*" \
        "/lib/$triple/libgcc_s.so*" \
        "/lib/$triple/libstdc++.so*"
}

stage_minimal_gcc_runtime()
{
    triple=$1
    version=$2
    libdir=$(gcc_libdir)
    dst=$root/usr/lib/gcc/$triple/$version

    mkdir -p "$dst"
    copy_glob_to_dir "$dst" \
        "$libdir/crtbegin*.o" \
        "$libdir/crtend*.o" \
        "$libdir/libgcc.a" \
        "$libdir/libgcc_eh.a" \
        "$libdir/libgcc_s.so*" \
        "$libdir/libstdc++.so*" \
        "$libdir/libsupc++.a"
}

stage_binutils()
{
    triple=$1
    as_path=$(command -v "$triple-as" 2>/dev/null || command -v as 2>/dev/null || true)
    ld_path=$(command -v "$triple-ld.bfd" 2>/dev/null || command -v "$triple-ld" 2>/dev/null || command -v ld.bfd 2>/dev/null || command -v ld 2>/dev/null || true)

    [ -n "$as_path" ] || { echo "missing required host assembler" >&2; exit 1; }
    [ -n "$ld_path" ] || { echo "missing required host linker" >&2; exit 1; }

    as_real=$(readlink -f "$as_path")
    ld_real=$(readlink -f "$ld_path")
    mkdir -p "$root/usr/bin"
    copy_to "$as_real" "$root/usr/bin/$(basename "$as_real")"
    copy_to "$as_real" "$root/usr/bin/as"
    copy_to "$ld_real" "$root/usr/bin/$(basename "$ld_real")"
    copy_to "$ld_real" "$root/usr/bin/$triple-ld"
    copy_to "$ld_real" "$root/usr/bin/ld"
    cmd_elf_deps "$root" "$as_real" "$ld_real"
}

stage_gcc_toolchain()
{
    triple=$(gcc_triple)
    version=$(gcc_version)
    gcc_real=$(require_real "$HOST_GCC")
    gcc_name=$(basename "$gcc_real")
    libdir=$(gcc_libdir)
    libexecdir=$(gcc_libexecdir)

    [ -n "$triple" ] && [ -n "$version" ] || { echo "cannot detect host gcc target/version" >&2; exit 1; }
    [ -d "$libdir" ] || { echo "cannot detect host gcc libdir" >&2; exit 1; }
    [ -d "$libexecdir" ] || { echo "cannot detect host gcc libexecdir" >&2; exit 1; }

    mkdir -p "$root/usr/bin" "$root/usr/lib/gcc/$triple" "$root/usr/libexec/gcc/$triple"
    copy_to "$gcc_real" "$root/usr/bin/$gcc_name"
    copy_to "$gcc_real" "$root/usr/bin/gcc-$version"
    copy_to "$gcc_real" "$root/usr/bin/gcc"
    copy_to "$gcc_real" "$root/usr/bin/cc"
    cp -aL "$libdir" "$root/usr/lib/gcc/$triple/"
    cp -aL "$libexecdir" "$root/usr/libexec/gcc/$triple/"

    stage_binutils "$triple"
    stage_glibc_devel "$triple"
    cmd_elf_deps "$root" "$gcc_real" "$libexecdir/cc1" "$libexecdir/collect2"
}

stage_clang_resource()
{
    clang=$1
    resource_dir=$("$clang" --print-resource-dir 2>/dev/null)
    resource_ver=$(basename "$resource_dir")
    resource_dst=$root/usr/lib/clang/$resource_ver

    [ -d "$resource_dir" ] || { echo "cannot detect clang resource dir" >&2; exit 1; }

    mkdir -p "$resource_dst"
    if [ "${CLANG_STAGE_FULL_RESOURCE:-0}" = 1 ]; then
        cp -aL "$resource_dir/." "$resource_dst/"
        return 0
    fi

    cp -aL "$resource_dir/include" "$resource_dst/include"
    mkdir -p "$resource_dst/lib/linux"
    copy_to "$resource_dir/lib/linux/libclang_rt.builtins-x86_64.a" "$resource_dst/lib/linux/libclang_rt.builtins-x86_64.a"
    copy_to "$resource_dir/lib/linux/clang_rt.crtbegin-x86_64.o" "$resource_dst/lib/linux/clang_rt.crtbegin-x86_64.o"
    copy_to "$resource_dir/lib/linux/clang_rt.crtend-x86_64.o" "$resource_dst/lib/linux/clang_rt.crtend-x86_64.o"
}

stage_clang_toolchain()
{
    triple=$(gcc_triple)
    version=$(gcc_version)
    clang_real=$(require_real "$HOST_CLANG")
    clangxx_real=$(require_real "$HOST_CLANGXX")
    llvm_ar_real=$(find_real "$HOST_LLVM_AR" || true)
    llvm_ranlib_real=$(find_real "$HOST_LLVM_RANLIB" || true)

    [ -n "$triple" ] && [ -n "$version" ] || { echo "cannot detect host gcc target/version" >&2; exit 1; }

    mkdir -p "$root/usr/bin" "$root/usr/lib"
    copy_to "$clang_real" "$root/usr/bin/clang"
    copy_to "$clangxx_real" "$root/usr/bin/clang++"
    copy_to "$clang_real" "$root/usr/bin/cc"
    copy_to "$clangxx_real" "$root/usr/bin/c++"

    if [ -n "$llvm_ar_real" ]; then
        copy_to "$llvm_ar_real" "$root/usr/bin/llvm-ar"
        copy_to "$llvm_ar_real" "$root/usr/bin/ar"
    fi
    if [ -n "$llvm_ranlib_real" ]; then
        copy_to "$llvm_ranlib_real" "$root/usr/bin/llvm-ranlib"
        copy_to "$llvm_ranlib_real" "$root/usr/bin/ranlib"
    fi

    copy_ldd_named_deps_to_dir "$root/usr/lib" "$clang_real" "$llvm_ar_real"

    stage_binutils "$triple"
    stage_glibc_devel "$triple"
    stage_minimal_gcc_runtime "$triple" "$version"
    stage_clang_resource "$clang_real"
    stage_non_llvm_ldd_deps "$clang_real" "$clangxx_real"
    if [ -n "$llvm_ar_real" ]; then
        stage_non_llvm_ldd_deps "$llvm_ar_real"
    fi
    if [ -n "$llvm_ranlib_real" ]; then
        stage_non_llvm_ldd_deps "$llvm_ranlib_real"
    fi
}

cmd_toolchain()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}

    case "$IMAGE_TOOLCHAIN" in
        clang)
            stage_clang_toolchain
            ;;
        gcc)
            stage_gcc_toolchain
            ;;
        none)
            ;;
        *)
            echo "unsupported IMAGE_TOOLCHAIN=$IMAGE_TOOLCHAIN (expected clang, gcc, or none)" >&2
            exit 1
            ;;
    esac
}

cmd_selfhost_user()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}

    rm -rf \
        "$root/apps/user" \
        "$root/apps/include" \
        "$root/apps/lib" \
        "$root/apps/third_party" \
        "$root/apps/liballoc-x86_64.a"
    mkdir -p "$root/apps/third_party"

    cp -a ./user "$root/apps/user"
    cp -a ./include "$root/apps/include"
    cp -a ./lib "$root/apps/lib"
    copy_to ./liballoc-x86_64.a "$root/apps/liballoc-x86_64.a"

    for dir in $SELFHOST_USER_THIRD_PARTY; do
        [ -e "./$dir" ] || continue
        cp -a "./$dir" "$root/apps/third_party/"
    done

    find "$root/apps/user" "$root/apps/third_party" -type d -name .git -prune -exec rm -rf {} +
}

cmd_populate_root()
{
    [ "$#" -eq 1 ] || usage

    root=${1%/}

    stage_base_dirs
    stage_base_files

    copy_to ./user/fmanager/fm_bg.png "$root/system/resources/image/fm_bg.png"
    copy_to "$BUILD_DIR/BOOTX64.efi" "$root/EFI/BOOT/BOOTX64.efi"
    copy_to "$BUILD_DIR/kernel.elf" "$root/system/kernel.elf"

    copy_list_to_root_dir apps/system $SYSTEM_APPS
    copy_list_to_root_dir apps/builtin $BUILTIN_APPS
    copy_list_to_root_dir apps $USER_APPS
    copy_list_to_root_dir apps/builtin $STARDUSTUI_BUILTIN_APPS

    cmd_selfhost_user "$root"

    copy_to /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 "$root/lib64/ld-linux-x86-64.so.2"
    for runtime_file in $MUSL_RUNTIME_FILES; do
        copy_to "./resources/musl/$runtime_file" "$root/lib/$runtime_file"
    done
    if [ -n "$HOST_GCC_TRIPLE" ]; then
        copy_to "/lib/$HOST_GCC_TRIPLE/libc.so.6" "$root/lib64/libc.so.6"
    fi
    if [ "$IMAGE_BUSYBOX" = 1 ]; then
        copy_to "$APP_BUSYBOX" "$root/apps/busybox"
    fi
    if [ -f ./lolcat_100.0.1-3_all.deb ]; then
        copy_to ./lolcat_100.0.1-3_all.deb "$root/apps/lolcat.deb"
    fi
    copy_to "$APP_FASTFETCH" "$root/apps/fastfetch"
    copy_to ./1.c "$root/apps/1.c"

    cmd_toolchain "$root"
    cmd_elf_deps "$root" "$APP_FASTFETCH"

    case "$IMAGE_PACKAGE_MANAGER" in
        xbps)
            cmd_xbps "$root"
            if [ -n "$XBPS_BOOTSTRAP" ]; then
                if [ ! -d "$XBPS_PREPARE_ROOT/var/db/xbps" ]; then
                    echo "missing prepared xbps root: $XBPS_PREPARE_ROOT" >&2
                    echo "run: make prepare" >&2
                    exit 1
                fi
                cmd_prepared_root "$XBPS_PREPARE_ROOT" "$root"
            fi
            ;;
        none)
            ;;
        *)
            echo "unsupported IMAGE_PACKAGE_MANAGER=$IMAGE_PACKAGE_MANAGER (expected xbps or none)" >&2
            exit 1
            ;;
    esac

    copy_to ./font/ttf/XJ380F.ttf "$root/system/font/XJ380F.ttf"
    copy_to ./font/ttf/XJ380C.ttf "$root/system/font/XJ380C.ttf"
    copy_contents_to_root_dir ./graphics/window/theme/skyglass system/theme/skyglass
    copy_to ./graphics/peek.png "$root/system/foxland/peek.png"
    copy_contents_to_root_dir ./graphics/logo system
    copy_contents_to_root_dir ./graphics/icon system/icon
    copy_glob_to_dir "$root/mod" "./$BUILD_DIR/*.km"
    copy_contents_to_root_dir ./resources system/resources

    flatten_root_links
}

[ "$#" -gt 0 ] || usage

command=$1
shift

case "$command" in
    base)
        cmd_base "$@"
        ;;
    elf-deps)
        cmd_elf_deps "$@"
        ;;
    xbps-bootstrap)
        cmd_xbps_bootstrap "$@"
        ;;
    prepared-root)
        cmd_prepared_root "$@"
        ;;
    xbps)
        cmd_xbps "$@"
        ;;
    toolchain)
        cmd_toolchain "$@"
        ;;
    selfhost-user)
        cmd_selfhost_user "$@"
        ;;
    populate-root)
        cmd_populate_root "$@"
        ;;
    *)
        usage
        ;;
esac
