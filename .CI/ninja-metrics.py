"""Read real command spans without imposing artificial Ninja stage barriers."""
import json
from pathlib import Path

build = Path("build")
records = []
for line in (build / ".ninja_log").read_text(encoding="utf-8").splitlines():
    fields = line.split("\t")
    if len(fields) >= 5 and not line.startswith("#"):
        records.append((int(fields[0]), int(fields[1]), fields[3].replace("\\", "/")))
objects = [r for r in records if r[2].lower().endswith((".obj", ".o"))]
executable = [r for r in records if r[2].lower().endswith("bin/glassorino.exe")]
if not objects or not executable:
    raise SystemExit("Ninja log did not contain actual native object compilation and application linking")
measurements = {
    "cpp": {"stage": "cpp", "seconds": round((max(r[1] for r in objects) - min(r[0] for r in objects)) / 1000, 3),
            "objects": len(objects), "exitCode": 0, "measurement": "native object command wall span"},
    "link": {"stage": "link", "seconds": round((executable[-1][1] - executable[-1][0]) / 1000, 3),
             "exitCode": 0, "measurement": "final executable link command"},
}
for name, measurement in measurements.items():
    (build / "metrics" / (name + ".json")).write_text(json.dumps(measurement, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(measurement))
