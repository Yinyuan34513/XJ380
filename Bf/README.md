# Bf binary resources

Put host-independent binary resources used by the image staging scripts here.

For XBPS:

```sh
mkdir -p Bf
XBPS_XZ_URI="http://repo-default.voidlinux.org/static/xbps-static-latest.$(uname -m)-musl.tar.xz"
curl -fL "$XBPS_XZ_URI" -o "Bf/xbps-static-latest.$(uname -m)-musl.tar.xz"
```

Also provide XBPS triggers using one of these forms:

```text
Bf/xbps-triggers/
Bf/xbps-triggers-<version>.<arch>.xbps
```

For ncurses applications such as nano, keep required terminfo entries under:

```text
Bf/terminfo/
```
