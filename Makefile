#---------------------------------------------------------------------------------
# FIFA 14 -- Nintendo Switch wrapper (32-bit / AArch32, EA Nimble engine)
#
# Ships NO game code and NO game assets: the player's own APK (any name) is
# read at run time, and its three libraries are unpacked from it on the first
# launch (source/fifa14_storage.c). The 1.39 GB expansion stays on the SD card
# and is read directly (OBB access: still TODO, see NOTES.md).
#
# The build is the android32 runtime's (runtime/runtime.mk: devkitARM + libnx32
# + mesa32 from portlibs32/); ./build.sh runs it in the toolchain container.
# Output: fifa14_nx.nsp, which the launcher NRO carries (launcher/).
#---------------------------------------------------------------------------------
TARGET               := fifa14_nx
PORT_NPDM_PROGRAM_ID := 0x0100000000001014
include runtime/runtime.mk

.PHONY: check
check:
	@echo "run on the host: python3 runtime/tools/gen_imports.py --libs <apk>/lib/armeabi-v7a"
	@echo "                 python3 runtime/tools/gen_imports.py --check"
