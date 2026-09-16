#!/usr/bin/env python3
r"""Author ApocryphaCivilWarStarts.esp.

There is no Creation Kit on this machine and this plugin does not need one: it holds exactly TWO
records, one per side, and they exist only so Alternate Perspective has something to name in its menu
and something to start. All of the behaviour is in ApocryphaCivilWarStarts.dll, which watches for one
of those quests starting. The plugin is written here the same way Build-PerkReallocationESP.py writes
its own - raw TES4 records - so the result is byte-for-byte reproducible and reviewable in a diff.

What it contains:

    TES4                             header, one master: Skyrim.esm
    GRUP QUST
      QUST 0x01000800  APS_StormcloakStart   "Stormcloak Recruit"
      QUST 0x01000801  APS_ImperialStart     "Imperial Recruit"

Both quests are deliberately inert - no stages, no aliases, no script. Neither is start-game-enabled,
so nothing happens until Alternate Perspective starts one; the DLL stops them again as soon as the
start sequence has run.

FormID note: the plugin declares one master, so its own records take on-disk index 01 and the records
are 0x01000800 and 0x01000801. Alternate Perspective is given "0x800" and "0x801" in the JSON, because
it calls Game.GetFormFromFile, which takes the FormID without the plugin's index - the same form AP's
own registration uses for its own starts.

Usage:  python build-esp.py [out.esp]
"""
import os
import struct
import sys

AUTHOR = "ApocryphaRealm"
DESCRIPTION = "Alternate Perspective - Civil War Starts"

FORM_VERSION = 44  # SSE

QUESTS = [
    (0x01000800, "APS_StormcloakStart", "Stormcloak Recruit"),
    (0x01000801, "APS_ImperialStart", "Imperial Recruit"),
]


def sub(tag: bytes, data: bytes) -> bytes:
    """One subrecord: 4-byte tag, 2-byte size, payload."""
    if len(data) > 0xFFFF:
        raise ValueError("subrecord %s is too long for a 16-bit size" % tag)
    return tag + struct.pack("<H", len(data)) + data


def record(tag: bytes, formid: int, data: bytes, flags: int = 0) -> bytes:
    """One record: 24-byte header then the subrecords. Never compressed - these are tiny."""
    return (tag + struct.pack("<I", len(data)) + struct.pack("<I", flags) +
            struct.pack("<I", formid) + struct.pack("<I", 0) +
            struct.pack("<H", FORM_VERSION) + struct.pack("<H", 0) + data)


def quest_record(formid: int, edid: str, full: str) -> bytes:
    # DNAM is the quest's own data: flags(u16) priority(u8) unused(u8) unknown(i32) type(i32).
    #   flags 0x0000 - NOT start-game-enabled. AP starts it; nothing else should.
    #   priority 0, type 0 (None) - it is not a main/side/misc quest and must not appear in the journal.
    dnam = struct.pack("<HBBiI", 0x0000, 0, 0, 0, 0)
    body = (sub(b"EDID", edid.encode("cp1252") + b"\x00") +
            sub(b"FULL", full.encode("cp1252") + b"\x00") +
            sub(b"DNAM", dnam) +
            sub(b"ANAM", struct.pack("<I", 0)))  # next alias id - there are no aliases
    return record(b"QUST", formid, body)


def tes4_header(record_count: int, next_object_id: int) -> bytes:
    # HEDR: version(f32) numRecords(i32) nextObjectID(u32)
    hedr = struct.pack("<fiI", 1.7, record_count, next_object_id)
    body = (sub(b"HEDR", hedr) +
            sub(b"CNAM", AUTHOR.encode("cp1252") + b"\x00") +
            sub(b"SNAM", DESCRIPTION.encode("cp1252") + b"\x00") +
            sub(b"MAST", b"Skyrim.esm\x00") +
            sub(b"DATA", struct.pack("<Q", 0)))
    return record(b"TES4", 0, body)


def main(out_path: str) -> int:
    quests = b"".join(quest_record(formid, edid, full) for formid, edid, full in QUESTS)
    group = (b"GRUP" + struct.pack("<I", len(quests) + 24) + b"QUST" +
             struct.pack("<I", 0) + struct.pack("<I", 0) + struct.pack("<I", 0))
    next_id = (max(q[0] for q in QUESTS) & 0xFFFFFF) + 1
    data = tes4_header(len(QUESTS), next_id) + group + quests

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)

    print("wrote %s (%d bytes)" % (out_path, len(data)))
    for formid, edid, full in QUESTS:
        print("  QUST %08X  %-22s \"%s\"   -> JSON id 0x%X" % (formid, edid, full, formid & 0xFFFFFF))
    return 0


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    default = os.path.join(here, "..", "dist", "ApocryphaCivilWarStarts.esp")
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(default)))
