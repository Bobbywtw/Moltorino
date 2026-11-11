"""Identify native app inputs independently of isolated QML/shader lab edits."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

inputs = subprocess.check_output([
    "git", "ls-tree", "-r", "HEAD", "--", "src", "lib", "cmake", "resources",
    "CMakeLists.txt", "conanfile.py", ".gitmodules", "tools/support",
    "tools/crash-handler",
])
record = {"schema": 1, "nativeFingerprint": hashlib.sha256(inputs).hexdigest(),
          "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
          "qtVersion": "6.9.3", "nightly": False, "lto": True,
          "configuration": "RelWithDebInfo"}
if len(sys.argv) > 1:
    target = Path(sys.argv[1])
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
print(json.dumps(record))
