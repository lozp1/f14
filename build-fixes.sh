#!/bin/sh
# Build the payload, then the launcher, each with its own log and .exit file.
# The .exit file is written by THIS script, so a failing build cannot be hidden
# by the wrapper's own exit code (a lesson this port already learned once).
cd "$(dirname "$0")" || exit 1
{
  echo "=== payload: ./build.sh  $(date)"
  ./build.sh
  echo "payload rc=$?"
} > build-fixes-payload.log 2>&1
tail -1 build-fixes-payload.log | sed 's/.*rc=//' > build-fixes-payload.exit.txt
{
  echo "=== launcher: ./launcher/build.sh  $(date)"
  ./launcher/build.sh
  echo "launcher rc=$?"
} > build-fixes-launcher.log 2>&1
tail -1 build-fixes-launcher.log | sed 's/.*rc=//' > build-fixes-launcher.exit.txt
echo "done"
