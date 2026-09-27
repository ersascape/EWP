#!/usr/bin/env bash
set -euo pipefail

watch_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
watch_python="$watch_root/.tools/platformio/bin/python"
if [[ ! -x "$watch_python" ]]; then
    python3 -m venv "$watch_root/.tools/platformio"
fi
if ! "$watch_python" -c 'import platformio' >/dev/null 2>&1; then
    "$watch_python" -m pip install 'platformio==6.2.0'
fi
export PLATFORMIO_CORE_DIR="$watch_root/.pio-core"
cd -- "$watch_root"
exec "$watch_python" -m platformio "$@"
