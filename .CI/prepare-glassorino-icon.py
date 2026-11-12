#!/usr/bin/env python3
"""Prepare the supplied Glassorino logo for Qt and Windows resources."""
import base64
import hashlib
import struct
from pathlib import Path

root = Path(__file__).resolve().parent.parent
png = base64.b64decode((root / ".CI" / "glassorino-icon.b64").read_text("ascii").strip(), validate=True)
if hashlib.sha256(png).hexdigest() != "df35eb4a6f90e3062d9997899c8252b4e4f8ffad4a360ff6836f746e7f685d6d":
    raise RuntimeError("Glassorino icon asset digest mismatch")
if png[:8] != b"\x89PNG\r\n\x1a\n":
    raise RuntimeError("Invalid PNG icon")
res = root / "resources"
(res / "icon.png").write_bytes(png)
# Windows supports PNG-compressed ICO entries; use the source's 64px artwork.
ico = struct.pack("<HHH", 0, 1, 1)
ico += struct.pack("<BBBBHHII", 64, 64, 0, 0, 1, 32, len(png), 22)
(res / "icon.ico").write_bytes(ico + png)
print("Glassorino icon prepared")
