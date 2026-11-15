#!/usr/bin/env python3
"""
acme_pak.py - list and extract Looney Tunes: Acme Arsenal (PS2) .PAK archives.

Usage:
  python acme_pak.py list    MAIN.PAK            # print every file with offset/size/compression
  python acme_pak.py extract MAIN.PAK out_dir    # unpack everything (decompressing FFCK files)
  python acme_pak.py extract MAIN.PAK out_dir GAMEDATA/BOOT   # only paths starting with a prefix

Format notes are in docs/pak-format.md. Only the standard library is needed.
"""
import os
import struct
import sys

MAGIC = 0xD00FD00F
SECTOR = 2048


# ---------------------------------------------------------------- UCL decoders
# Ports of the UCL reference decompressors (8-bit bit buffer). Acme Arsenal's
# MAIN.PAK only uses method 0 (NRV2B); 2D/2E are included because the game's
# loader supports them (methods 1 and 2) but they are untested against real data.

class _Bits:
    def __init__(self, src):
        self.s, self.i, self.bb = src, 0, 0

    def bit(self):
        if not (self.bb & 0x7F):
            self.bb = self.s[self.i] * 2 + 1
            self.i += 1
        else:
            self.bb *= 2
        return (self.bb >> 8) & 1

    def byte(self):
        v = self.s[self.i]
        self.i += 1
        return v


def _copy(dst, dist, length):
    p = len(dst) - dist
    for k in range(length):
        dst.append(dst[p + k])


def nrv2b(src):
    b, dst, last = _Bits(src), bytearray(), 1
    while True:
        while b.bit():
            dst.append(b.byte())
        m = 1
        while True:
            m = m * 2 + b.bit()
            if b.bit():
                break
        if m == 2:
            m = last
        else:
            m = (m - 3) * 256 + b.byte()
            if m >= 0xFFFFFFFF - 3 * 256 or m == 0xFFFFFFFF:
                break
            m += 1
            last = m
        n = b.bit() * 2 + b.bit()
        if n == 0:
            n = 1
            while True:
                n = n * 2 + b.bit()
                if b.bit():
                    break
            n += 2
        n += (m > 0xD00)
        _copy(dst, m, n + 1)
    return bytes(dst)


def _nrv2de(src, e):
    b, dst, last = _Bits(src), bytearray(), 1
    while True:
        while b.bit():
            dst.append(b.byte())
        m = 1
        while True:
            m = m * 2 + b.bit()
            if b.bit():
                break
            m = (m - 1) * 2 + b.bit()
        if m == 2:
            m = last
            n = b.bit()
        else:
            m = (m - 3) * 256 + b.byte()
            if m == 0xFFFFFFFF:
                break
            n = (m ^ 0xFFFFFFFF) & 1
            m = (m >> 1) + 1
            last = m
        if e:
            if n:
                n = 1 + b.bit()
            elif b.bit():
                n = 3 + b.bit()
            else:
                n = 1
                while True:
                    n = n * 2 + b.bit()
                    if b.bit():
                        break
                n += 3
        else:
            n = n * 2 + b.bit()
            if n == 0:
                n = 1
                while True:
                    n = n * 2 + b.bit()
                    if b.bit():
                        break
                n += 2
        n += (m > 0x500)
        _copy(dst, m, n + 1)
    return bytes(dst)


DECODERS = {0: nrv2b, 1: lambda s: _nrv2de(s, False), 2: lambda s: _nrv2de(s, True)}
METHOD_NAMES = {0: "nrv2b", 1: "nrv2d", 2: "nrv2e"}


# ---------------------------------------------------------------- PAK reading

def read_toc(data):
    magic, version, toc_size, _, file_size, _, count, _ = struct.unpack_from("<8I", data, 0)
    if magic != MAGIC:
        raise ValueError("not an Acme Arsenal PAK (bad magic %08X)" % magic)
    entries, p = [], 0x20
    for _ in range(count):
        size, offset, sector, nparts = struct.unpack_from("<IIIB", data, p)
        p += 13
        parts = []
        for _ in range(nparts):
            ln = data[p]
            parts.append(data[p + 1:p + 1 + ln].decode("latin-1"))
            p += 1 + ln
        name = "/".join(parts[:-1]) + "." + parts[-1]   # last part is the extension
        entries.append({"name": name, "pos": sector * SECTOR + offset, "size": size, "record": p})
    return entries


def read_file(data, entry):
    blob = data[entry["pos"]:entry["pos"] + entry["size"]]
    if blob[:4] != b"FFCK":
        return blob, "raw"
    _, method, usize, csize = struct.unpack_from("<4sIII", blob, 0)
    out = DECODERS[method](blob[16:16 + csize])
    if len(out) != usize:
        raise ValueError("%s: size mismatch after decompression" % entry["name"])
    return out, METHOD_NAMES[method]


def main(argv):
    if len(argv) < 3 or argv[1] not in ("list", "extract"):
        print(__doc__)
        return 1
    data = open(argv[2], "rb").read()
    entries = read_toc(data)
    if argv[1] == "list":
        for e in entries:
            blob = data[e["pos"]:e["pos"] + 4]
            kind = "ffck" if blob == b"FFCK" else "raw "
            print("%-6s %10d @ 0x%08X  %s" % (kind, e["size"], e["pos"], e["name"]))
        print("%d files" % len(entries))
        return 0
    if len(argv) < 4:
        print(__doc__)
        return 1
    out_dir, prefix = argv[3], (argv[4].replace("\\", "/").upper() if len(argv) > 4 else "")
    done = 0
    for e in entries:
        if not e["name"].upper().startswith(prefix):
            continue
        content, _ = read_file(data, e)
        path = os.path.join(out_dir, *e["name"].split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(content)
        done += 1
    print("extracted %d files to %s" % (done, out_dir))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
