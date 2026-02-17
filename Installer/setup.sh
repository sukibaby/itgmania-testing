#!/bin/bash
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo 'root privileges required' >&2
    exit 1
fi

# Installation prefix and paths
PREFIX="${PREFIX:-/opt}"
INSTALL_DIR="${PREFIX}/itgmania"
THEMES_DIR="${INSTALL_DIR}/Themes"
THEME_DIR="${THEMES_DIR}/Simply Love"
THEME_OLD_DIR="${THEMES_DIR}/Simply Love.old"
DESKTOP_FILE="${INSTALL_DIR}/itgmania.desktop"
APPS_DIR="/usr/share/applications"

copy_sl_config() {
	local src_file="${THEME_OLD_DIR}/$1"
	local dst_file="${THEME_DIR}/$1"
	
	if [ -d "$src_file" ]; then
		# Don't overwrite any existing files.
		# This lets us retain the updated readmes and other files that come with the new release.
		cp -r -n "$src_file" "$dst_file"
		return
	elif [ -f "$src_file" ]; then
		cp "$src_file" "$dst_file"
	fi
}

cd "$(dirname "$0")"

# Move the old SL release out of the way
[ -d "$THEME_DIR" ] && mv "$THEME_DIR" "$THEME_OLD_DIR"

# Install ITGm
[ -d "$PREFIX" ] || install -d -m 755 -o root -g root "$PREFIX"
cp -R --preserve=mode,timestamps itgmania "$PREFIX"
ln -sf "$DESKTOP_FILE" "$APPS_DIR"

# Copy persistent files over from the old SL folder
if [ -d "$THEME_OLD_DIR" ]; then
	copy_sl_config "Other/SongManager PreferredCourses.txt"
	copy_sl_config "Other/SongManager PreferredSongs.txt"
	copy_sl_config "Modules"

	rm -rf "$THEME_OLD_DIR"
fi
