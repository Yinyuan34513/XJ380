#!/bin/sh

export HOME="/root"
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
mkdir -p $HOME/.config/dconf
mkdir -p $HOME/.config/labwc
mkdir -p /run/dbus
mkdir -p /var/lib/dbus
mkdir -p $XDG_RUNTIME_DIR

chmod 01777 /tmp/.X11-unix
chmod 0777 /tmp/.ICE-unix
chmod 0700 $XDG_RUNTIME_DIR

setup_xcursor_aliases() {
    cursor_dir="/usr/share/icons/Adwaita/cursors"
    if [ ! -d "$cursor_dir" ]; then
        return 0
    fi

    if [ ! -e "$cursor_dir/dnd-copy" ] && [ ! -L "$cursor_dir/dnd-copy" ]; then
        ln -s copy "$cursor_dir/dnd-copy" 2>/dev/null || true
    fi
    if [ ! -e "$cursor_dir/dnd-none" ] && [ ! -L "$cursor_dir/dnd-none" ]; then
        ln -s no-drop "$cursor_dir/dnd-none" 2>/dev/null || \
            ln -s not-allowed "$cursor_dir/dnd-none" 2>/dev/null || true
    fi
    if [ ! -e "$cursor_dir/dnd-move" ] && [ ! -L "$cursor_dir/dnd-move" ]; then
        ln -s default "$cursor_dir/dnd-move" 2>/dev/null || true
    fi
}

setup_xcursor_aliases

cat > $HOME/.config/labwc/autostart <<'EOF'
#!/bin/sh

background="/system/resources/image/background2.png"
if [ -f "$background" ]; then
    swaybg -i "$background" -m fill &
fi
EOF
chmod +x $HOME/.config/labwc/autostart

dbus-daemon --system --fork
dbus-uuidgen --ensure

udevd --daemon

while [ ! -e "/run/udev/control" ]; do
    sleep 0.5
done

udevadm settle

exec dbus-run-session -- \
    seatd-launch -- \
    labwc
