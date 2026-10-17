#!/usr/bin/env python3
"""Verify the FIFA 14 port artifacts after a payload + launcher build.

Checks (the ones the task asks for):
  1. sha256(fifa14_nx.nsp) == sha256(launcher/romfs/fifa14_nx.nsp)
  2. launcher/fifa14_nx.nro is NEWER than the payload (both copies)
  3. the payload appears VERBATIM (contiguous, byte for byte) inside the .nro
  4. no 'error:' in the build logs
Prints a JSON report. MIT.
"""
import hashlib
import json
import os
import re
import sys

PORT = "C:/Projects/ports/fifa14-nx"
PAYLOAD = os.path.join(PORT, "fifa14_nx.nsp")
ROMFS = os.path.join(PORT, "launcher", "romfs", "fifa14_nx.nsp")
NRO = os.path.join(PORT, "launcher", "fifa14_nx.nro")
LOGS = sys.argv[1:] or ["build-nimble.log", "build-nimble-launcher.log"]


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def main():
    rep = {}
    rep["payload_sha256"] = sha(PAYLOAD)
    rep["payload_bytes"] = os.path.getsize(PAYLOAD)
    rep["romfs_sha256"] = sha(ROMFS)
    rep["romfs_bytes"] = os.path.getsize(ROMFS)
    rep["sha_match"] = rep["payload_sha256"] == rep["romfs_sha256"]
    rep["payload_mtime"] = os.path.getmtime(PAYLOAD)
    rep["romfs_mtime"] = os.path.getmtime(ROMFS)
    rep["nro_mtime"] = os.path.getmtime(NRO)
    rep["nro_bytes"] = os.path.getsize(NRO)
    rep["nro_newer_than_payload"] = rep["nro_mtime"] > rep["payload_mtime"]
    rep["nro_newer_than_romfs"] = rep["nro_mtime"] > rep["romfs_mtime"]

    # verbatim: the whole payload as one contiguous run inside the .nro
    payload = open(PAYLOAD, "rb").read()
    nro = open(NRO, "rb").read()
    at = nro.find(payload[:65536])
    ok = False
    offs = []
    if at >= 0:
        offs.append(at)
        ok = nro[at:at + len(payload)] == payload
        # any further copies?
        p = at + 1
        while True:
            p = nro.find(payload[:65536], p)
            if p < 0:
                break
            offs.append(p)
            p += 1
    rep["payload_verbatim_in_nro"] = ok
    rep["payload_offsets_in_nro"] = offs

    # build logs
    errs = []
    warns = 0
    for lg in LOGS:
        p = os.path.join(PORT, lg)
        if not os.path.exists(p):
            errs.append("missing log %s" % lg)
            continue
        txt = open(p, errors="replace").read()
        errs += ["%s: %s" % (lg, l) for l in txt.splitlines() if "error:" in l]
        warns += len([l for l in txt.splitlines() if re.search(r"\bwarning\b", l)])
    rep["error_lines"] = errs
    rep["warning_lines"] = warns
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main()
