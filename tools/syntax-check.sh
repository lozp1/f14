#!/bin/sh
# Local syntax-only check of the port's sources with devkitARM (the same
# compiler the container uses), before the Docker build.
set -u
GCC="C:/devkitPRO/devkitARM/bin/arm-none-eabi-gcc.exe"
P="C:/Projects/ports/fifa14-nx"
ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mfloat-abi=softfp -mfpu=neon-fp-armv8 -mtp=soft -fPIE -ftls-model=local-exec"
INC="-I$P/source -I$P/runtime/source -I$P/libnx32/prefix/include -I$P/portlibs32/include -I$P/tools/miniz-stub"
DEF="-D__SWITCH__ -DDCR_GL_MESA=1 -DRT_MESA=26 -DPORT_PAYLOAD_NAME=\\"fifa14_nx\\""
WARN="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror=implicit-function-declaration -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=return-type"
rc=0
# All the port's own files (source/*.c) are checked: the ones this round edits
# plus the ones it only reads. tools/miniz-stub supplies the one header the
# container's image has and this host does not.
for f in source/fifa14_boot.c source/fifa14_input.c source/fifa14_physkbd.c source/fifa14_padscope.c source/fifa14_padact.c source/fifa14_spch.c source/fifa14_config.c source/fifa14_ctlcfg.c source/fifa14_blast_stubs.c source/fifa14_jni_tables.c source/fifa14_external.c source/fifa14_obb.c; do
  echo "=== $f"
  # to a file, not a pipe: `| head` would make $? head's status, never gcc's
  "$GCC" -fsyntax-only $ARCH $DEF $INC $WARN "$P/$f" > tools/.syntax.tmp 2>&1
  s=$?
  head -40 tools/.syntax.tmp
  [ $s -ne 0 ] && rc=1
done
rm -f tools/.syntax.tmp
echo "exit=$rc"
