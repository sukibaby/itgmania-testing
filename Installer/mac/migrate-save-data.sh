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

echo "ITGmania data for version 1.1.0 or below was found."
for src in "$DEST_SAVE" "$DEST_LOGS" "$DEST_SCREENSHOTS"; do
  if [[ -d "$src" ]] && find "$src" -mindepth 1 -print -quit >/dev/null 2>&1; then
    echo ""
    echo " --- Note ---"
    echo "ITGmania data for version 1.2.0 or above seems to"
    echo "exist already!"
    echo ""
    echo "Migration may overwrite newer data, so please double"
    echo "check before moving or copying anything if you're"
    echo "concerned about possibly losing any data!"
    echo ""
  fi
done

echo ""
echo "Choose action: [m]ove (recommended), [c]opy, [x] cancel:"
read -r choice
case "$choice" in
  m|M) action="move" ;;
  c|C) action="copy" ;;
  x|X) echo "Cancelled."; exit 0 ;;
  *) echo "Invalid choice - please enter m, c, or x."; exit 1 ;;
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
    # macOS comes with rsync so this is safe
    rsync -a --remove-source-files --prune-empty-dirs "$src"/ "$dest"/
    find "$src" -mindepth 1 -maxdepth 1 -exec rm -rf -- {} + >/dev/null 2>&1 || true
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

echo
echo "Done!"