#!/usr/bin/env zsh
set -euo pipefail

PREFDIR="$HOME/Library/Preferences/ITGmania"
LOGDIR="$HOME/Library/Logs/ITGmania"
CACHEDIR="$HOME/Library/Caches/ITGmania"
SCREENSHOTDIR="$HOME/Pictures/ITGmania Screenshots"
DEST_ROOT="$HOME/Library/Application Support/ITGmania"
DEST_SAVE="$DEST_ROOT/Save"
DEST_LOGS="$DEST_ROOT/Logs"
DEST_SCREENSHOTS="$DEST_ROOT/Screenshots"

has_content=false
for src in "$PREFDIR" "$LOGDIR" "$CACHEDIR" "$SCREENSHOTDIR"; do
  if [[ -d "$src" ]] && find "$src" -mindepth 1 -print -quit >/dev/null 2>&1; then
    has_content=true
  fi
done

if ! $has_content; then
  echo "No ITGmania data found to migrate."
  exit 0
fi

echo "ITGmania data found in one or more of:"
echo "  $PREFDIR"
echo "  $LOGDIR"
echo "  $CACHEDIR"
echo "  $SCREENSHOTDIR"
echo ""
echo "Choose action: [m]ove (recommended), [c]opy, [x] cancel:"
read -r choice
case "$choice" in
  m|M) action="move" ;;
  c|C) action="copy" ;;
  x|X) echo "Cancelled."; exit 0 ;;
  *) echo "Invalid choice. Cancelled."; exit 1 ;;
esac

mkdir -p "$DEST_SAVE" "$DEST_LOGS" "$DEST_SCREENSHOTS"

copy_or_move() {
  local src=$1 dest=$2
  local op=$3
  [[ -d "$src" ]] || return 0
  if ! find "$src" -mindepth 1 -print -quit >/dev/null 2>&1; then
    return 0
  fi
  if [[ "$op" == "move" ]]; then
    echo "Moving from $src to $dest"
    mv "$src"/* "$dest"/ 2>/dev/null || true
    mv "$src"/.* "$dest"/ 2>/dev/null || true
  else
    echo "Copying from $src to $dest"
    rsync -a "$src"/ "$dest"/
  fi
}

copy_or_move "$PREFDIR" "$DEST_SAVE" "$action"
echo "Save data now located at $DEST_SAVE"

copy_or_move "$LOGDIR" "$DEST_LOGS" "$action"
echo "Old logs now located at $DEST_LOGS"

copy_or_move "$SCREENSHOTDIR" "$DEST_SCREENSHOTS" "$action"
echo "Old screenshots now located at $DEST_SCREENSHOTS"

# No need to bring over old cache, just remove it.
if [[ -d "$CACHEDIR" ]]; then
  echo "Removing old cache at $CACHEDIR"
  rm -rf "$CACHEDIR"
fi

echo "Done."