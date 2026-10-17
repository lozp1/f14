#!/usr/bin/env python3
"""fifa14-crash-evidencia.py -- the disassembly evidence behind the fix in
source/fifa14_gl.c, re-derived from libFIFA14.so instead of quoted.

Run:  python3 tools/fifa14-crash-evidencia.py [path-to-libFIFA14.so]

It re-derives, from the .so alone:

  1. the crash site and the caller (pc 0x3925b8b8 -> +0xe5c8b8 inside
     EA::StdC::Strcmp, lr 0x3896519c -> +0x56619c = the instruction after the
     only `bl Strcmp` in the function at +0x566038);
  2. the string the NULL was compared against: the PIC pair (r8 = pc +
     0xb81780 at +0x566050) makes r8 = 0x010e77d8 (link-time), and
     r1 = r8 + 0xfff21f17 (the literal at +0x56679c) = 0x010096ef, whose first
     byte is 0x4d = 'M' -- the r2 in crash.log -- and which reads "Mali-T604"
     in the GPU table ("Adreno (TM) 200/205/220", "PowerVR SGX 530/535/540/
     543/544", "NVIDIA Tegra", "GT-I8190N" are its neighbours);
  3. what returned the NULL: the virtual call at +0x566184 is
     slot 0x124 of the graphics object's vtable with argument 0x1F01
     (GL_RENDERER), and slot 0x124 is
     EA::Graphics::OpenGLES20Managed::glGetString (+0xe1f9f0 -> the +8 thunk
     -> OpenGLES20::glGetString +0xe1821c -> `bl glGetString@plt`);
  4. that the game cannot make a context itself: the .dynsym has no
     eglCreateContext/eglMakeCurrent/eglCreateWindowSurface/eglGetProcAddress
     import and .rodata has none of those names as a string, while
     eglGetCurrentDisplay/eglGetCurrentSurface/eglSwapBuffers ARE imported --
     i.e. on Android the Java GLSurfaceView owns the context.

No game code or data is read out of the .so here: the tool only reports
addresses, symbol names and the short GPU-name strings.
"""
import re
import struct
import subprocess
import sys
import os

OD = os.environ.get("ARM_OBJDUMP", r"C:/devkitPro/devkitARM/bin/arm-none-eabi-objdump.exe")
SO = sys.argv[1] if len(sys.argv) > 1 else r"C:/Projects/ports/fifa14/libs/libFIFA14.so"

# The three values the port's crash handler printed (crash.log), and the base
# the port's own +offset mapping implies: 0x3925b8b8 - 0xe5c8b8 = 0x383ff000.
# (That base also reconciles the register dump exactly: r8 0x394e67d8 ->
# .got+0x010e77d8, the PIC base the code computes, and r1 0x394086ef ->
# .rodata+0x010096ef = "Mali-T604", whose 'M' is the r2 0x4d.)
PC, LR, BASE = 0x3925B8B8, 0x3896519C, 0x383FF000
CRASH_VMA, LR_VMA = PC - BASE, LR - BASE  # 0xe5c8b8, 0x56619c

data = open(SO, "rb").read()
DYN, DYNSTR = 0x114, 0x87274


def word(vma):
    return struct.unpack_from("<I", data, vma)[0]


def rodata(vma, n=64):
    return data[vma:vma + n]


def symtab():
    out = []
    for i in range(0x87160 // 16):
        name, val, size, info, _o, shndx = struct.unpack_from("<IIIBBH", data, DYN + 16 * i)
        end = data.index(b"\0", DYNSTR + name)
        out.append((data[DYNSTR + name:end].decode("latin1"), val, size, shndx))
    return out


def objdump(args):
    return subprocess.run([OD] + args, capture_output=True, text=True, errors="replace").stdout


def main():
    syms = {n: (v, s, sh) for n, v, s, sh in symtab()}
    print("pc 0x%08x -> libFIFA14.so+0x%x (given base 0x%08x)" % (PC, CRASH_VMA, BASE))
    print("lr 0x%08x -> libFIFA14.so+0x%x" % (LR, LR_VMA))

    # 1. the caller: a `bl` to the crash address, the instruction before lr
    call = LR_VMA - 4  # ARM: lr = the `bl`'s address + 4
    insn = word(call)
    off = insn & 0xFFFFFF
    off -= 0x1000000 if off & 0x800000 else 0
    tgt = (call + 8 + off * 4) & 0xFFFFFFFF
    print("\n1. the caller")
    print("   %s" % objdump(["-d", "--start-address=0x%x" % (call - 8),
                             "--stop-address=0x%x" % (call + 8), SO]).splitlines()[-2].strip())
    print("   %s" % objdump(["-d", "--start-address=0x%x" % (call - 8),
                             "--stop-address=0x%x" % (call + 8), SO]).splitlines()[-1].strip())
    print("   the bl at +0x%x targets +0x%x = _ZN2EA4StdC6StrcmpEPKcS2_ -> %s"
          % (call, tgt, "yes" if tgt == CRASH_VMA - 4 else "NO"))
    print("   the fault is at +0x%x = Strcmp+4 (`ldrb r3,[r0]`), lr = +0x%x is the"
          % (CRASH_VMA, LR_VMA))
    print("   next instruction -> the crash IS this compare")

    # 2. the PIC pair and the string
    add_insn, add_lit, str_lit = 0x566050, 0x566744, 0x56679C
    r8 = (add_insn + 8 + word(add_lit)) & 0xFFFFFFFF
    r1 = (r8 + word(str_lit)) & 0xFFFFFFFF
    s = rodata(r1)
    print("\n2. the string (the PIC base, +0x566050 `add r8, pc, r0`, and the")
    print("   literals at +0x%x and +0x%x)" % (add_lit, str_lit))
    print("   r8 = 0x%08x + 0x%08x = 0x%08x   (crash.log r8 should be base+this)"
          % (add_insn + 8, word(add_lit), r8))
    print("   r1 = r8 + 0x%08x = 0x%08x -> \"%s\""
          % (word(str_lit), r1, s.split(b"\0")[0].decode("latin1")))
    print("   first byte 0x%02x = '%s'  == crash.log r2 = 0x4d  -> %s"
          % (s[0], chr(s[0]), "MATCH" if s[0] == 0x4D else "NO"))
    print("   its neighbours in .rodata (the GPU table):")
    base = r1 - 0x30
    print("     %s" % data[base:base + 0x100].replace(b"\0", b"|").decode("latin1"))

    # 3. what returned the NULL
    print("\n3. the call that returned NULL (at +0x566184): slot 0x124 = the")
    print("   #292 of `ldr r2,[r1,#292]`, called with r1 = 0x1f01 = GL_RENDERER")
    for ln in objdump(["-d", "--start-address=0x566168", "--stop-address=0x56619c", SO]).splitlines()[-12:]:
        print("   " + ln.strip())
    for name in ("_ZN2EA8Graphics17OpenGLES20Managed11glGetStringEj",
                 "_ZN2EA8Graphics10OpenGLES2011glGetStringEj"):
        v, sz, sh = syms[name]
        print("   %s @ +0x%x (size 0x%x)" % (name, v, sz))
    print("   " + "\n   ".join(l.strip() for l in objdump(
        ["-d", "--start-address=0xe1f9f0", "--stop-address=0xe1fa08", SO]).splitlines()[-6:]))
    print("   " + "\n   ".join(l.strip() for l in objdump(
        ["-d", "--start-address=0xe1821c", "--stop-address=0xe1822c", SO]).splitlines()[-4:]))

    # 4. the game has no context of its own
    print("\n4. the game's own EGL surface (the .dynsym's undefined symbols)")
    und = [n for n, v, s, sh in symtab() if sh == 0]
    for probe in ("eglCreateContext", "eglMakeCurrent", "eglCreateWindowSurface",
                  "eglGetProcAddress", "eglGetCurrentDisplay", "eglGetCurrentSurface",
                  "eglSwapBuffers", "glGetString"):
        print("   %-22s in .dynsym: %-3s  as a string in the image: %s"
              % (probe, "yes" if probe in und else "NO",
                 "yes" if data.find(probe.encode()) >= 0 else "NO"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
