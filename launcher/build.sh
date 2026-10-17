#!/bin/sh
# Build the launcher NRO (64-bit). The runtime's launcher/build.sh runs make in
# devkitPro's 64-bit container with the payload next door. Build the wrapper
# first (../build.sh): the NRO carries ../fifa14_nx.nsp and ../fifa14_nx.build.
HERE="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_DIR="$HERE" PAYLOAD=fifa14_nx exec "$HERE/../runtime/launcher/build.sh" "$@"
