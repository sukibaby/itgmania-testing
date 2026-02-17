#!/bin/sh
set -eu

# Prompt user for installation location
echo "Where would you like to install ITGmania?"
echo "1) User directory: ~/.local/itgmania"
echo "2) System directory: /opt (requires root privileges)"
echo "Note, if you want to perform a portable install, you must choose the user directory."
printf "Enter choice (1 or 2): "
read choice

PORTABLE_INSTALL=0
need_root=0

case "$choice" in
    1)
        PREFIX="${PREFIX:-${HOME}/.local/itgmania}"
        APPS_DIR="${HOME}/.local/share/applications"
        printf "Create a portable install? (y/n): "
        read portable_choice
        if [ "$portable_choice" = "y" ] || [ "$portable_choice" = "Y" ]; then
            PORTABLE_INSTALL=1
        fi
        ;;
    2)
        need_root=1
        PREFIX="${PREFIX:-/opt}"
        APPS_DIR="/usr/share/applications"
        ;;
    *)
        echo "Invalid choice. Please enter 1 or 2." >&2
        exit 1
        ;;
esac

if [ "$need_root" -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
    echo "Error: root privileges required to install to /opt" >&2
    echo "Try running with sudo:" >&2
    echo "  sudo \"$0\"" >&2
    exit 1
fi

if [ "$need_root" -eq 1 ]; then
    echo "Installing to system directory: $PREFIX"
else
    echo "Installing to user directory: $PREFIX"
fi

INSTALL_DIR="$PREFIX"
THEMES_DIR="$INSTALL_DIR/Themes"
THEME_DIR="$THEMES_DIR/Simply Love"
THEME_OLD_DIR="$THEMES_DIR/Simply Love.old"
DESKTOP_FILE="$INSTALL_DIR/itgmania.desktop"

copy_sl_config() {
	src_file="${THEME_OLD_DIR}/$1"
	dst_file="${THEME_DIR}/$1"

	if [ -d "$src_file" ]; then
		# Don't overwrite any existing files.
		# This lets us retain the updated readmes and other files that come with the new release.
		cp -r -n "$src_file" "$dst_file"
	elif [ -f "$src_file" ]; then
		cp "$src_file" "$dst_file"
	fi
}

cd "$(dirname "$0")"

# Move the old SL release out of the way
[ -d "$THEME_DIR" ] && mv "$THEME_DIR" "$THEME_OLD_DIR"

# Install ITGm
if [ ! -d "$PREFIX" ]; then
    if [ "$need_root" -eq 1 ]; then
        install -d -m 755 -o root -g root "$PREFIX"
    else
        install -d -m 755 "$PREFIX"
    fi
fi
cp -R --preserve=mode,timestamps itgmania "$PREFIX"

# Ensure applications directory exists and install desktop file
if [ ! -d "$APPS_DIR" ] && ! mkdir -p "$APPS_DIR" 2>/dev/null; then
    echo "Warning: Could not create applications directory: $APPS_DIR" >&2
    echo "Desktop file will not be installed." >&2
elif desktop_err=$(ln -sf "$DESKTOP_FILE" "$APPS_DIR" 2>&1); then
    :
else
    echo "Warning: Could not install desktop file to $APPS_DIR" >&2
    [ -n "$desktop_err" ] && printf "  %s\n" "$desktop_err" >&2
fi

# Create portable install marker if requested
if [ "$PORTABLE_INSTALL" = "1" ]; then
    touch "$PREFIX/Portable.ini"
    echo "Portable install marker created: $PREFIX/Portable.ini"
fi

# Copy persistent files over from the old SL folder
if [ -d "$THEME_OLD_DIR" ]; then
    for item in \
        "Other/SongManager PreferredCourses.txt" \
        "Other/SongManager PreferredSongs.txt" \
        "Modules"
    do
        copy_sl_config "$item"
    done

	rm -rf "$THEME_OLD_DIR"
fi
