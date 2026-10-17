#!/usr/bin/env python3
"""build-installer-nsps.py -- DBI-installable NSPs for FIFA 14 (android32 launcher).

Two recipes, both packed with hacbrewpack (see installer/INSTALADOR.md):

  self       : Program NCA ExeFS = the 64-bit launcher (main + main.npdm),
               Program NCA RomFS  = the launcher's romfs (the 32-bit game
               fifa14_nx.nsp + fifa14_nx.build), Control = icon + name.
               This is the literal recipe of the task.

  forwarder  : Program NCA ExeFS = the HBL-style 64-bit loader + NPDM,
               Program NCA RomFS  = /nextNroPath + /nextArgv pointing at the
               launcher .nro on the SD card, Control = icon + name.
               This is what sphaira's "Install Forwarder" produces, prebuilt
               so DBI can install it.

Three recipes, both packed with hacbrewpack (see installer/INSTALADOR.md):

  self       : Program NCA ExeFS = the 64-bit launcher (main + main.npdm),
               Program NCA RomFS  = the launcher's romfs (the 32-bit game
               fifa14_nx.nsp + fifa14_nx.build), Control = icon + name.
               This is the literal recipe of the task.

  forwarder  : Program NCA ExeFS = the HBL-style 64-bit loader + NPDM,
               Program NCA RomFS  = /nextNroPath + /nextArgv pointing at the
               launcher .nro on the SD card, Control = icon + name.
               This is what sphaira's "Install Forwarder" produces, prebuilt
               so DBI can install it.

  direct     : Program NCA ExeFS = the 32-bit game itself (main + main.npdm
               copied verbatim from the validated ExeFS override), Program NCA
               RomFS = the same /nextNroPath + /nextArgv, Control = icon + name.
               Installing it makes the game BE the title: no launcher, no
               ExeFS override, nothing to install at boot.

All use the same 0x05xxxxxxxxxxxxxx title id (the launcher only installs the
32-bit game for a title whose id is in the 0x05 range: exefs_is_forwarder_tid).

Build one recipe, several, or all:

  python installer/build-installer-nsps.py direct      # -> <slug>-direct.nsp
  python installer/build-installer-nsps.py all         # self + forwarder + direct
  python installer/build-installer-nsps.py             # self + forwarder (legacy)
"""
import hashlib, json, os, pathlib, shutil, struct, subprocess, sys

DK          = pathlib.Path(os.environ.get("DEVKITPRO_WIN", "C:/devkitPro"))
TOOLS       = DK / "tools" / "bin"
PROJ        = pathlib.Path(__file__).resolve().parent.parent
LAUNCHER    = PROJ / "launcher"
INST        = PROJ / "installer"
TARGET      = "fifa14_nx"
APP_TITLE   = "FIFA 14"
APP_AUTHOR  = "aks796, EA Mobile"
APP_VERSION = "0.0.1"
NRO_PATH    = "sdmc:/switch/fifa14/fifa14_nx.nro"          # sdmc:/<PORT_ROOT_PATH>/<TARGET>.nro
KEYS        = pathlib.Path(os.environ.get("PROD_KEYS", "C:/Users/f.paolo/.switch/prod.keys"))
HACBREWPACK = pathlib.Path(os.environ.get(
    "HACBREWPACK", "C:/Program Files/NTON/_internal/nton/bin/hacbrewpack.exe"))
HBL_NSO     = pathlib.Path(os.environ.get("HBL_NSO", INST / "hbl.nso"))

SELF_NSP      = INST / (TARGET + "-installer.nsp")
FORWARDER_NSP = INST / (TARGET + "-forwarder.nsp")

# ---- 64-bit, full-capability NPDM (the ports' own runtime caps + all services)
NPDM = {
  "name": "Application",
  "program_id": "0x0", "program_id_range_min": "0x0", "program_id_range_max": "0x0",
  "main_thread_stack_size": "0x100000", "main_thread_priority": 44, "default_cpu_id": 0,
  "process_category": 0, "pool_partition": 0, "is_64_bit": True,
  "address_space_type": 1, "is_retail": True,
  "filesystem_access": {"permissions": "0xFFFFFFFFFFFFFFFF",
                        "content_owner_ids": ["0x0100000000001000"]},
  "service_host": ["*"], "service_access": ["*"],
  "kernel_capabilities": [
    {"type": "kernel_flags", "value": {"highest_thread_priority": 59,
      "lowest_thread_priority": 28, "highest_cpu_id": 2, "lowest_cpu_id": 0}},
    {"type": "application_type", "value": 1},
    {"type": "min_kernel_version", "value": "0x30"},
    {"type": "handle_table_size", "value": 512},
    {"type": "debug_flags", "value": {"allow_debug": False,
      "force_debug_prod": True, "force_debug": False}},
    {"type": "map_region", "value": [{"region_type": 1, "is_ro": True}]},
  ],
}
# all syscalls 0x00..0x7F + unassigned names, as the stock hbl NPDM
SC = ["svcUnassigned00","svcSetHeapSize","svcSetMemoryPermission","svcSetMemoryAttribute",
 "svcMapMemory","svcUnmapMemory","svcQueryMemory","svcExitProcess","svcCreateThread",
 "svcStartThread","svcExitThread","svcSleepThread","svcGetThreadPriority","svcSetThreadPriority",
 "svcGetThreadCoreMask","svcSetThreadCoreMask","svcGetCurrentProcessorNumber","svcSignalEvent",
 "svcClearEvent","svcMapSharedMemory","svcUnmapSharedMemory","svcCreateTransferMemory",
 "svcCloseHandle","svcResetSignal","svcWaitSynchronization","svcCancelSynchronization",
 "svcArbitrateLock","svcArbitrateUnlock","svcWaitProcessWideKeyAtomic","svcSignalProcessWideKey",
 "svcGetSystemTick","svcConnectToNamedPort","svcSendSyncRequestLight","svcSendSyncRequest",
 "svcSendSyncRequestWithUserBuffer","svcSendAsyncRequestWithUserBuffer","svcGetProcessId",
 "svcGetThreadId","svcBreak","svcOutputDebugString","svcReturnFromException","svcGetInfo",
 "svcFlushEntireDataCache","svcFlushDataCache","svcMapPhysicalMemory","svcUnmapPhysicalMemory",
 "svcGetDebugFutureThreadInfo","svcGetLastThreadInfo","svcGetResourceLimitLimitValue",
 "svcGetResourceLimitCurrentValue","svcSetThreadActivity","svcGetThreadContext3",
 "svcWaitForAddress","svcSignalToAddress","svcSynchronizePreemptionState",
 "svcGetResourceLimitPeakValue","svcUnassigned38","svcCreateIoPool","svcCreateIoRegion",
 "svcUnassigned3B","svcKernelDebug","svcChangeKernelTraceState","svcUnassigned3E",
 "svcUnassigned3F","svcCreateSession","svcAcceptSession","svcReplyAndReceiveLight",
 "svcReplyAndReceive","svcReplyAndReceiveWithUserBuffer","svcCreateEvent","svcMapIoRegion",
 "svcUnmapIoRegion","svcMapPhysicalMemoryUnsafe","svcUnmapPhysicalMemoryUnsafe","svcSetUnsafeLimit",
 "svcCreateCodeMemory","svcControlCodeMemory","svcSleepSystem","svcReadWriteRegister",
 "svcSetProcessActivity","svcCreateSharedMemory","svcMapTransferMemory","svcUnmapTransferMemory",
 "svcCreateInterruptEvent","svcQueryPhysicalAddress","svcQueryIoMapping",
 "svcCreateDeviceAddressSpace","svcAttachDeviceAddressSpace","svcDetachDeviceAddressSpace",
 "svcMapDeviceAddressSpaceByForce","svcMapDeviceAddressSpaceAligned","svcMapDeviceAddressSpace",
 "svcUnmapDeviceAddressSpace","svcInvalidateProcessDataCache","svcStoreProcessDataCache",
 "svcFlushProcessDataCache","svcDebugActiveProcess","svcBreakDebugProcess",
 "svcTerminateDebugProcess","svcGetDebugEvent","svcContinueDebugEvent","svcGetProcessList",
 "svcGetThreadList","svcGetDebugThreadContext","svcSetDebugThreadContext",
 "svcQueryDebugProcessMemory","svcReadDebugProcessMemory","svcWriteDebugProcessMemory",
 "svcSetHardwareBreakPoint","svcGetDebugThreadParam","svcUnassigned6E","svcGetSystemInfo",
 "svcCreatePort","svcManageNamedPort","svcConnectToPort","svcSetProcessMemoryPermission",
 "svcMapProcessMemory","svcUnmapProcessMemory","svcQueryProcessMemory","svcMapProcessCodeMemory",
 "svcUnmapProcessCodeMemory","svcCreateProcess","svcStartProcess","svcTerminateProcess",
 "svcGetProcessInfo","svcCreateResourceLimit","svcSetResourceLimitLimitValue",
 "svcCallSecureMonitor"]
NPDM["kernel_capabilities"].insert(1, {"type": "syscalls",
   "value": {name: hex(i) for i, name in enumerate(SC)}})

def sha256(b): return hashlib.sha256(b).hexdigest()

def compute_tid(nro_path):
    """sphaira's derivation: 0x0500_0000_0000_0000 | sha256(path+path)[0:8] & mask."""
    h = hashlib.sha256((nro_path + nro_path).encode()).digest()[:8]
    return 0x0500000000000000 | (int.from_bytes(h, "little") & 0x00FFFFFFFFFFF000)

TID = compute_tid(NRO_PATH)

def run(args, cwd=None):
    r = subprocess.run([str(a) for a in args], cwd=str(cwd) if cwd else None,
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write("FAILED: " + " ".join(str(a) for a in args) + "\n")
        sys.stderr.write(r.stdout + "\n" + r.stderr + "\n")
        raise SystemExit(1)
    return r

def make_keyset(dest):
    want = {"header_key", "key_area_key_application_00", "key_area_key_application_01"}
    lines = [l.strip() for l in KEYS.read_text(errors="ignore").splitlines()
             if l.split("=")[0].strip().lower() in want]
    assert any(l.lower().startswith("header_key") for l in lines), "header_key missing"
    dest.write_text("\n".join(lines) + "\n")

def write_control(d):
    (d / "control").mkdir(parents=True, exist_ok=True)
    nacp = d / "control" / "control.nacp"
    run([TOOLS / "nacptool.exe", "--create", APP_TITLE, APP_AUTHOR, APP_VERSION,
         nacp, "--titleid=" + hex(TID)])
    shutil.copy2(LAUNCHER / "icon.jpg", d / "control" / "icon_AmericanEnglish.dat")

def write_npdm(d):
    j = json.loads(json.dumps(NPDM))
    j["program_id"] = j["program_id_range_min"] = j["program_id_range_max"] = hex(TID)
    (d / "npdm.json").write_text(json.dumps(j, indent=2) + "\n")
    (d / "exefs").mkdir(parents=True, exist_ok=True)
    run([TOOLS / "npdmtool.exe", d / "npdm.json", d / "exefs" / "main.npdm"])

def pack(work, keyset):
    r = run([HACBREWPACK, "--keyset", keyset, "--titleid", "%016x" % TID,
             "--nologo", "--plaintext", "--keepncadir"], cwd=work)
    (work / "pack.log").write_text(r.stdout)
    return work / "hacbrewpack_nsp" / ("%016x.nsp" % TID)

def build_self():
    d = INST / "self"
    for sub in ("exefs", "romfs", "control"):
        shutil.rmtree(d / sub, ignore_errors=True)
    d.mkdir(parents=True, exist_ok=True)
    write_npdm(d)
    run([TOOLS / "elf2nso.exe", LAUNCHER / (TARGET + ".elf"), d / "exefs" / "main"])
    (d / "romfs").mkdir(exist_ok=True)
    for f in ("%s.nsp" % TARGET, "%s.build" % TARGET):
        shutil.copy2(LAUNCHER / "romfs" / f, d / "romfs" / f)
    write_control(d)
    ks = d / "keyset.txt"; make_keyset(ks)
    out = pack(d, ks)
    shutil.copy2(out, SELF_NSP)
    return SELF_NSP

def build_forwarder():
    d = INST / "forwarder"
    for sub in ("exefs", "romfs", "control"):
        shutil.rmtree(d / sub, ignore_errors=True)
    d.mkdir(parents=True, exist_ok=True)
    write_npdm(d)
    shutil.copy2(HBL_NSO, d / "exefs" / "main")
    (d / "romfs").mkdir(exist_ok=True)
    (d / "romfs" / "nextNroPath").write_text(NRO_PATH)
    (d / "romfs" / "nextArgv").write_text(NRO_PATH)
    write_control(d)
    ks = d / "keyset.txt"; make_keyset(ks)
    out = pack(d, ks)
    shutil.copy2(out, FORWARDER_NSP)
    return FORWARDER_NSP

# --------------------------------------------------------------------------
# 'direct' recipe: the Program NCA ExeFS IS the 32-bit game itself.
#
# Same NACP / romfs / icon / title id as the forwarder, but the ExeFS is the
# (main, main.npdm) pair of the already validated ExeFS override that the
# launcher writes to the SD card at atmosphere/contents/<title id>/exefs.nsp
# (runtime/source/dcr_exefs.h :: exefs_build_override()).  main.npdm already
# carries the title id, so installing this NSP makes the game BE the title:
# no launcher, no ExeFS override, nothing to install at boot.  The romfs keeps
# /nextNroPath + /nextArgv so the game can still find its own .nro.
#
# --nosignncasig2 is REQUIRED here: without it hacbrewpack re-signs main.npdm
# ("Patching ACID public key") and rewrites 170 bytes of the NPDM (META ACI0
# /ACID offsets + ACID public key and signature), so the ExeFS would no longer
# match the override byte for byte.
# --------------------------------------------------------------------------
ATMO_CONTENTS = pathlib.Path(os.environ.get("ATMO_CONTENTS", "D:/atmosphere/contents"))
DIRECT_NSP    = INST / "fifa14-direct.nsp"

def pfs0_parse(b, at=0):
    """Parse the PFS0 at `at` -> (files, data_start, total_len)."""
    assert b[at:at+4] == b'PFS0', "no PFS0 at 0x%x" % at
    n = struct.unpack_from('<I', b, at+4)[0]
    strsz = struct.unpack_from('<I', b, at+8)[0]
    files = []
    for i in range(n):
        e = at + 0x10 + 24*i
        off, sz, no = struct.unpack_from('<QQI', b, e)
        s = at + 0x10 + 24*n + no
        files.append((b[s:b.index(b'\0', s)].decode(), off, sz))
    tot = 0x10 + 24*n + strsz + max([o + s for _, o, s in files] or [0])
    return files, at + 0x10 + 24*n + strsz, tot

def program_exefs(nsp):
    """Carve the Program NCA's ExeFS PFS0 out of an NSP (no key material).

    hacbrewpack runs with --plaintext, so only the 0xC00-byte NCA3 header is
    encrypted; the sections are plaintext.  The Program NCA is the one whose
    ExeFS PFS0 holds exactly main + main.npdm."""
    files, ds, _ = pfs0_parse(nsp)
    for nm, off, sz in files:
        if not nm.endswith('.nca'):
            continue
        nca = nsp[ds+off: ds+off+sz]
        i = nca.find(b'PFS0')
        while 0 <= i < 0x4000:
            try:
                fl, fds, ftot = pfs0_parse(nca, i)
            except Exception:
                fl = []
            if sorted(n for n, _, _ in fl) == ['main', 'main.npdm']:
                pfs0 = nca[i:i+ftot]
                return nm, pfs0, [(n, pfs0[fds-i+o: fds-i+o+s]) for n, o, s in fl]
            i = nca.find(b'PFS0', i+1)
    raise SystemExit("no Program NCA ExeFS found in " + str(nsp))

def first_pfs0_name(nca):
    """Name of the first file of the first parseable PFS0 inside an NCA."""
    i = nca.find(b'PFS0')
    while 0 <= i < 0x4000:
        try:
            fl, _, _ = pfs0_parse(nca, i)
            if fl:
                return fl[0][0]
        except Exception:
            pass
        i = nca.find(b'PFS0', i+1)
    return ''

def override_path():
    return ATMO_CONTENTS / ("%016X" % TID) / "exefs.nsp"

def override_files():
    p = override_path()
    b = p.read_bytes()
    fl, ds, tot = pfs0_parse(b)
    assert tot == len(b), "%s has %d trailing bytes" % (p, len(b) - tot)
    out = {n: b[ds+o: ds+o+s] for n, o, s in fl}
    for n in ('main', 'main.npdm'):
        assert n in out, "%s lacks %s" % (p, n)
    return p, out

def pack_direct(work, keyset):
    r = run([HACBREWPACK, "--keyset", keyset, "--titleid", "%016x" % TID,
             "--nologo", "--plaintext", "--keepncadir", "--nosignncasig2"],
            cwd=work)
    (work / "pack.log").write_text(r.stdout)
    return work / "hacbrewpack_nsp" / ("%016x.nsp" % TID)

def build_direct():
    d = INST / "direct"
    for sub in ("exefs", "romfs", "control"):
        shutil.rmtree(d / sub, ignore_errors=True)
    d.mkdir(parents=True, exist_ok=True)
    ovp, ovf = override_files()
    (d / "exefs").mkdir(parents=True, exist_ok=True)
    for nm in ('main', 'main.npdm'):
        (d / "exefs" / nm).write_bytes(ovf[nm])
    (d / "romfs").mkdir(exist_ok=True)
    (d / "romfs" / "nextNroPath").write_text(NRO_PATH)
    (d / "romfs" / "nextArgv").write_text(NRO_PATH)
    write_control(d)
    ks = d / "keyset.txt"; make_keyset(ks)
    out = pack_direct(d, ks)
    shutil.copy2(out, DIRECT_NSP)
    print("direct ExeFS source: %s" % ovp)
    return DIRECT_NSP

def verify_direct():
    """Prove that installer/<slug>-direct.nsp installs THE GAME, not a launcher."""
    ovp, ovf = override_files()
    ov = ovp.read_bytes()
    nsp = DIRECT_NSP.read_bytes()
    log = (INST / "direct" / "pack.log").read_text(errors="ignore")
    warns = [l for l in log.splitlines() if l.strip().lower().startswith("warning")]

    print("\n== verificacion de %s ==" % DIRECT_NSP.name)
    print("[1] build                  : exit 0, warnings=%d %s" % (len(warns), warns))

    nca, pfs0, fl = program_exefs(nsp)
    fd = dict(fl)
    print("[2] ExeFS identico override: %s  (%s %d B sha256=%s  vs  %s %d B sha256=%s)"
          % (pfs0 == ov, nca, len(pfs0), sha256(pfs0), ovp.name, len(ov), sha256(ov)))
    for n in ('main', 'main.npdm'):
        print("      %-10s %9d B sha256=%s igual=%s"
              % (n, len(fd[n]), sha256(fd[n]), fd[n] == ovf[n]))

    nro = NRO_PATH.encode()
    print("[3] romfs                  : nextNroPath x%d  nextArgv x%d  '%s' x%d"
          % (nsp.count(b'nextNroPath'), nsp.count(b'nextArgv'), NRO_PATH, nsp.count(nro)))

    npdm = fd['main.npdm']
    aci0 = struct.unpack_from('<I', npdm, 0x70)[0]
    acid = struct.unpack_from('<I', npdm, 0x78)[0]
    allf, ads, _ = pfs0_parse(nsp)
    cnmtname = ''
    for nm, off, sz in allf:
        if nm.endswith('.cnmt.nca'):
            cnmtname = first_pfs0_name(nsp[ads+off: ads+off+sz])
    print("[4] title id               : %016X  (npdm aci0+0x10=%016X acid+0x210=%016X acid+0x218=%016X)"
          % (TID, struct.unpack_from('<Q', npdm, aci0+0x10)[0],
             struct.unpack_from('<Q', npdm, acid+0x210)[0],
             struct.unpack_from('<Q', npdm, acid+0x218)[0]))
    print("      cnmt '%s'  title id little-endian occurrences in nsp: %d"
          % (cnmtname, nsp.count(struct.pack('<Q', TID))))
    print("[5] nsp                    : %d bytes sha256=%s" % (len(nsp), sha256(nsp)))

RECIPES = {"self": build_self, "forwarder": build_forwarder, "direct": build_direct}

if __name__ == "__main__":
    want = sys.argv[1:] or ["self", "forwarder"]
    if want == ["all"]:
        want = ["self", "forwarder", "direct"]
    for w in want:
        if w not in RECIPES:
            raise SystemExit("unknown recipe %r (self|forwarder|direct|all)" % w)
    for p in [RECIPES[w]() for w in want]:
        data = p.read_bytes()
        print("%s  %d bytes  sha256=%s  title_id=%016x" %
              (p.name, len(data), sha256(data), TID))
    if "direct" in want:
        verify_direct()
