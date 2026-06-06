#!/bin/sh

set -eu

export HOME="/users/GuoqiFish"
export SHELL="/bin/bash"
export XDG_SESSION_TYPE="wayland"
export XDG_CONFIG_DIRS="/etc/xdg"
export XDG_RUNTIME_DIR="/run/user/0"
export NO_AT_BRIDGE=1
export LANG=en_US.UTF-8
export PATH="/bin:/usr/bin:/usr/sbin:/sbin:/bin:/usr/local/bin:/usr/local/sbin"
export XCURSOR_THEME="Adwaita"
export XCURSOR_SIZE="24"
export XCURSOR_PATH="/usr/share/icons:/usr/share/pixmaps"
export SDL_VIDEODRIVER=wayland
export SDL_AUDIODRIVER=pipewire
export LIBGL_ALWAYS_SOFTWARE=1
export WLR_RENDERER_ALLOW_SOFTWARE=1
export WLR_RENDERER=pixman
export XKB_DEFAULT_LAYOUT=us
export XKB_DEFAULT_MODEL=pc104

mkdir -p /tmp/.X11-unix
mkdir -p /tmp/.ICE-unix
mkdir -p "$HOME/.config/dconf"
mkdir -p /run/dbus
mkdir -p /run/udev
mkdir -p /run/udev/data
mkdir -p /var/lib/dbus
mkdir -p "$XDG_RUNTIME_DIR"
mkdir -p /run/user/0/libinput

chmod 01777 /tmp/.X11-unix
chmod 0777 /tmp/.ICE-unix
chmod 0700 "$XDG_RUNTIME_DIR"

if [ ! -S /run/dbus/system_bus_socket ]; then
dbus-daemon --system --fork
    dbus-uuidgen --ensure
    
fi

if [ ! -e /run/udev/control ]; then
    udevd --daemon &

    tries=0
    while [ ! -e /run/udev/control ]; do
        tries=$((tries + 1))
        if [ "$tries" -ge 60 ]; then
            echo "start-wayland: timed out waiting for /run/udev/control" >&2
            exit 1
        fi
        sleep 0.5
    done
fi

udevadm settle

dump_tree() {
    path="$1"
    echo "start-wayland: recursive listing for $path"
    if [ ! -e "$path" ]; then
        echo "start-wayland: $path does not exist"
        return 0
    fi

    if command -v find >/dev/null 2>&1; then
        find "$path" -maxdepth 8 -exec ls -ld {} \; 2>&1 || true
    else
        ls -laR "$path" 2>&1 || true
    fi
}

exec dbus-run-session -- \
    seatd-launch -- \
    startxfce4 --wayland
