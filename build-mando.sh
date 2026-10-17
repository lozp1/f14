#!/bin/sh
# Build the payload, then the launcher, each with its own log and .exit file.
# The .exit file is written by THIS script, so a failing build cannot be hidden
# by the wrapper's own exit code (a lesson this port already learned once).
cd "$(dirname "$0")" || exit 1
{
  echo "=== payload: ./build.sh  $(date)"
  ./build.sh
  echo "payload rc=$?"
} > build-mando-payload.log 2>&1
echo $? > /dev/null
tail -1 build-mando-payload.log | sed 's/.*rc=//' > build-mando-payload.exit.txt
{
  echo "=== launcher: ./launcher/build.sh  $(date)"
  ./launcher/build.sh
  echo "launcher rc=$?"
} > build-mando-launcher.log 2>&1
tail -1 build-mando-launcher.log | sed 's/.*rc=//' > build-mando-launcher.exit.txt
echo "done"
