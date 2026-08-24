#!/usr/bin/env python3
"""Parseur d'en-tete ~PSP (PRX/EBOOT chiffre) et triage de dumps UMD.

Usage:
    python3 tools/psp_header.py EBOOT.BIN [BOOT.BIN ...]
"""
import collections
import hashlib
import math
import struct
import sys

HDR_SIZE = 0x150


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    n = len(data)
    counts = collections.Counter(data)
    return -sum(v / n * math.log2(v / n) for v in counts.values())


def parse_psp_header(d: bytes) -> dict:
    u = lambda fmt, off: struct.unpack_from(fmt, d, off)
    elf_size, psp_size, boot_entry, modinfo_off, bss_size = u("<5I", 0x28)
    return {
        "mod_attr": u("<H", 0x04)[0],
        "comp_attr": u("<H", 0x06)[0],
        "module_version": (d[0x09], d[0x08]),
        "module_name": d[0x0A:0x0A + 28].split(b"\x00")[0].decode("ascii", "replace"),
        "header_version": d[0x26],
        "nsegments": d[0x27],
        "elf_size": elf_size,
        "psp_size": psp_size,
        "boot_entry": boot_entry,
        "modinfo_offset": modinfo_off,
        "bss_size": bss_size,
        "seg_align": u("<4H", 0x3C),
        "seg_addr": u("<4I", 0x44),
        "seg_size": u("<4I", 0x54),
        "devkit_version": u("<I", 0x78)[0],
        "decrypt_mode": d[0x7C],
        "comp_size": u("<I", 0xB0)[0],
        "tag": u("<I", 0xD0)[0],
    }


def describe(path: str) -> None:
    with open(path, "rb") as fp:
        d = fp.read()

    print("=== %s ===" % path)
    print("  taille    : %d (0x%X)" % (len(d), len(d)))
    print("  sha1      : %s" % hashlib.sha1(d).hexdigest())
    print("  entropie  : %.3f bits/octet" % entropy(d))

    if not any(d):
        print("  -> fichier entierement nul : emplacement blanchi sur l'UMD,")
        print("     le code reel est dans l'EBOOT.BIN chiffre.")
        return

    if d[:4] != b"~PSP":
        print("  -> magic inattendu (%s), ni ~PSP ni fichier nul." % d[:4].hex())
        if d[:4] == b"\x7fELF":
            print("     C'est un ELF : deja en clair, passer directement a Ghidra.")
        return

    h = parse_psp_header(d)
    dk = h["devkit_version"]
    print("  module    : %s v%d.%d" % (h["module_name"], h["module_version"][0], h["module_version"][1]))
    print("  SDK       : %d.%02d (0x%08X)" % (dk >> 24, (dk >> 16) & 0xFF, dk))
    print("  attrs     : mod=0x%04X comp=0x%04X%s"
          % (h["mod_attr"], h["comp_attr"], "  [gzip]" if h["comp_attr"] else "  [non compresse]"))
    print("  tag KIRK  : 0x%08X   decrypt_mode=%d" % (h["tag"], h["decrypt_mode"]))
    print("  elf_size  : 0x%X   psp_size: 0x%X" % (h["elf_size"], h["psp_size"]))
    print("  entry     : 0x%X   module_info: 0x%X" % (h["boot_entry"], h["modinfo_offset"]))
    print("  bss       : 0x%X" % h["bss_size"])
    for i in range(h["nsegments"]):
        print("  segment %d : addr=0x%08X size=0x%X align=%d"
              % (i, h["seg_addr"][i], h["seg_size"][i], h["seg_align"][i]))

    footprint = max(a + s for a, s in zip(h["seg_addr"], h["seg_size"])) + h["bss_size"]
    print("  empreinte : ~%.0f KiB en RAM (.bss inclus)" % (footprint / 1024))

    body = entropy(d[HDR_SIZE:])
    if body > 7.9:
        print("  -> corps chiffre (entropie %.3f). Dechiffrer avec pspdecrypt" % body)
        print("     avant toute analyse.")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for path in sys.argv[1:]:
        describe(path)
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
