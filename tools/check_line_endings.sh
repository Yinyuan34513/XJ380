#!/bin/sh
set -eu

cr=$(printf '\r')
bad=

check_file()
{
    path=$1
    [ -f "$path" ] || return 0
    if LC_ALL=C grep -Iq "$cr" "$path" && LC_ALL=C grep -Il "$cr" "$path" >/dev/null; then
        printf 'CRLF line endings are not allowed: %s\n' "$path" >&2
        return 1
    fi
    return 0
}

for path in tools/*.sh Bf/xbps-triggers/*; do
    check_file "$path" || bad=1
done

if [ -n "$bad" ]; then
    exit 1
fi
