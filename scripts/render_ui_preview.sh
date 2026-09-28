#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
gfx_root="$repo_root/.pio/libdeps/ErsaWearable/Adafruit GFX Library"
output="$repo_root/docs/images/notification_call_media_simulation.png"

if [[ ! -f "$gfx_root/Adafruit_GFX.cpp" ]]; then
  echo "Adafruit GFX is missing. Run 'make firmware' first." >&2
  exit 1
fi
if ! command -v magick >/dev/null 2>&1; then
  echo "ImageMagick's 'magick' command is required." >&2
  exit 1
fi

preview_tmp="$(mktemp -d)"
trap 'rm -rf -- "$preview_tmp"' EXIT
# The upstream GFX header checks ARDUINO for Print signatures. Use the host
# signature in temporary copies, without changing the installed library.
for source in Adafruit_GFX.cpp Adafruit_GFX.h gfxfont.h glcdfont.c; do
  sed 's/#if ARDUINO >= 100/#if 1/g' "$gfx_root/$source" > "$preview_tmp/$source"
done

g++ -std=c++17 -I "$preview_tmp" -I "$repo_root/tests/ui_preview" \
  -I "$repo_root/include" -I "$repo_root/src" -I "$repo_root/tests" \
  "$repo_root/tests/ui_preview/render.cpp" "$preview_tmp/Adafruit_GFX.cpp" \
  "$repo_root/src/apps/app_notifications.cpp" "$repo_root/src/apps/app_call.cpp" \
  "$repo_root/src/apps/app_now_playing.cpp" \
  "$repo_root/src/ersa/services/bluetooth_manager.cpp" \
  "$repo_root/src/ersa/events/event_bus.cpp" \
  "$repo_root/src/ersa/app/application_manager.cpp" \
  -o "$preview_tmp/render"
"$preview_tmp/render" "$preview_tmp"
magick "$preview_tmp/notification.pgm" "$preview_tmp/call.pgm" \
  "$preview_tmp/now_playing.pgm" +append -filter point -resize 200% "$output"
echo "Wrote $output"
