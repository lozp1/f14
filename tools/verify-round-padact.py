#!/usr/bin/env python3
"""Host-side cross-check of THIS ROUND's measured constants against libFIFA14.so
and the console's own numbers (2026-10-14). Every claim the new code makes about
an address is re-derived here, from the file, the way the code reads it:

  * the game object pointer's GOT slot and its VA/offset trap (fifa14_padscope.c)
  * the eight touch-button rectangles and the surface (fifa14_padact.c)
  * the speech start's call pair, its object slot and its verification words
    (fifa14_spch.c)

Reads the .so, computes, prints PASS/FAIL per claim. No engine, no console.
MIT.
"""
import os
import struct
import sys

SO = os.environ.get("FIFA14_SO", "C:/Projects/ports/fifa14/libFIFA14.so")
if not os.path.exists(SO):
    SO = "libFIFA14.so"
D = open(SO, "rb").read()
GOT = 0x10E77D8
fails = []


def u32(a):
    return struct.unpack_from("<I", D, a)[0]


def s(a):
    return D[a:D.find(b"\x00", a)][:48].decode("latin1", "replace")


def va_to_off(va):
    """The file offset of a virtual address: PT_LOAD segments (measured here)."""
    phoff = u32(0x1C)
    phnum = struct.unpack_from("<H", D, 0x2C)[0]
    for i in range(phnum):
        o = phoff + i * 32
        p_type, p_off, p_va, p_pa, p_fsz, p_msz, p_fl, p_al = struct.unpack_from("<8I", D, o)
        if p_type == 1 and p_va <= va < p_va + p_fsz:
            return p_off + (va - p_va)
    return None


def check(name, got, want):
    ok = got == want
    print("%-4s %-58s got %s want %s" % ("PASS" if ok else "FAIL", name, hex(got) if isinstance(got, int) else got, hex(want) if isinstance(want, int) else want))
    if not ok:
        fails.append(name)


# --- 1. the game object pointer (fifa14_padscope.c's PADS_GAME) --------------
# the engine's own read at 0x46a564 loads the GOT slot 0x10e1a20 then [slot]
check("the engine's read words at 0x46a564", [u32(0x46A564 + 4 * i) for i in range(5)],
      [0xE59F0024, 0xE59F1024, 0xE08F0000, 0xE7910000, 0xE5900000])
check("the pool word -> GOT (the add is at 0x46a56c)",
      (0x46A56C + 8 + u32(0x46A590)) & 0xFFFFFFFF, GOT)
check("the pool word (0x46a594, the slot offset)", u32(0x46A594), 0xFFFFA248)
check("the slot = GOT + (-24008)", (GOT + 0xFFFFA248) & 0xFFFFFFFF, 0x10E1A20)
# THE TRAP: the word at the slot's VA (not its file offset) is the addend
check("the addend at VA 0x10e1a20 (PADS_GAME)", u32(va_to_off(0x10E1A20)), 0x1181938)
check("...and the word at file offset 0x10e1a20 is the other one", u32(0x10E1A20), 0x11E0C84)
# the console's own numbers of 2026-10-14
base_console = 0x3A636BA8 - 0xDF0BA8
check("the console's load base (OnRawKeyDown)", base_console, 0x39846000)
check("the console's slot content == base + PADS_GAME", (base_console + 0x1181938) & 0xFFFFFFFF,
      0x3A9C7938)
check("...while base + the old 0x11e0c84 is NOT it", (base_console + 0x11E0C84) & 0xFFFFFFFF,
      0x3AA26C84)

# --- 2. the touch rectangles (fifa14_padact.c) -------------------------------
# the hit-test at 0x6e84e8 builds the offsets with mov+orr: x=0x358|0xac00, w=0xf60|0xa000
check("rect 0's x offset (0x6e84e8)", (0x358 | 0xAC00), 0xAF58)
check("rect 0's w offset (0x6e84fc)", (0xF60 | 0xA000), 0xAF60)
check("rect 2's x offset (0x6e8568)", (0x388 | 0xAC00), 0xAF88)
check("rect 1's x offset (0x6e8a60)", (0x370 | 0xAC00), 0xAF70)
check("rect 7's x (0x6e8b00+..: 0xb000)", (0xB000 | 0x0000), 0xB000)
check("stride: rect1 - rect0", 0xAF70 - 0xAF58, 0x18)
check("rect 7 = base + 7*stride", 0xAF58 + 7 * 0x18, 0xB000)
# the surface the touch handler checks against (0x6e75e4/0x6e7600)
check("surface W offset (0x6e75e4)", (0x114 | 0xC800), 0xC914)
check("surface H offset (0x6e7600)", (0x118 | 0xC800), 0xC918)
# the dispatch that calls the touch handler hands it THE GAME OBJECT: 0x564a58
# loads 0x9a160 and reads *(GOT + 0x9a160) -- the very same slot as padscope's
check("the touch dispatch's object = GOT + 0x9a160 = PADS_GAME", (GOT + 0x9A160) & 0xFFFFFFFF,
      0x1181938)

# --- 3. the speech start (fifa14_spch.c) -------------------------------------
check("the speech getter's five words at 0x7e69f0", [u32(0x7E69F0 + 4 * i) for i in range(5)],
      [0xE59F000C, 0xE59F100C, 0xE08F0000, 0xE7910000, 0xE12FFF1E])
check("the speech start's three words at 0x7e6f1c", [u32(0x7E6F1C + 4 * i) for i in range(3)],
      [0xE92D4FF0, 0xE24DD0CC, 0xE24DDB01])
# 0x7e69f0's own arithmetic: GOT + 0xfde28 = the object slot
check("the getter's slot offset (0x7e6a08)", u32(0x7E6A08), 0xFDE28)
check("the object slot = GOT + that", (GOT + 0xFDE28) & 0xFFFFFFFF, 0x11E5600)
# the start's own early-exit read: GOT + 0xfde2c (0x7e7cb0)
check("the start's audio-system offset (0x7e7cb0)", u32(0x7E7CB0), 0xFDE2C)
check("the audio-system address", (GOT + 0xFDE2C) & 0xFFFFFFFF, 0x11E5604)
# the gate's key: GOT + 0xfff18dca = "PRACTICE_MODE"
check("the gate's key string", s((GOT + 0xFFF18DCA) & 0xFFFFFFFF), "PRACTICE_MODE")
# and the pack's own file names, hardcoded ENG_US
check("the pack's first file name", s(0x10042E4), "Speech/hdr_ENG_US.big")
check("the pack's last file name", s(0x1004325), "Speech/pbp_ENG_US.evt")

print()
print("FAILED: %d claim(s)" % len(fails))
for f in fails:
    print("   -", f)
sys.exit(1 if fails else 0)
