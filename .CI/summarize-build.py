"""Publish measured command stages; job-level cache/checkout times come from Actions."""
import json
import os
from pathlib import Path
import re

root = Path("build/metrics")
stages = {}
for filename in ("conan", "configure", "app", "cpp", "link", "package"):
    path = root / (filename + ".json")
    if path.exists():
        stages[filename] = json.loads(path.read_text(encoding="utf-8-sig"))
stats = (root / "sccache.txt").read_text(encoding="utf-8-sig")
def count(label):
    match = re.search(r"^" + re.escape(label) + r"\s+(\d+)\s*$", stats, re.MULTILINE)
    return int(match.group(1)) if match else None
hits, misses = count("Cache hits"), count("Cache misses")
cache = {"hits": hits, "misses": misses, "nonCacheable": count("Non-cacheable calls")}
if hits is not None and misses is not None and hits + misses:
    cache["hitPercent"] = round(100 * hits / (hits + misses), 2)
result = {"stages": stages, "sccache": cache,
          "notes": "app is the measured end-to-end build command. cpp is Ninja's native-object command span; link is the final executable link command. Spans can overlap and should not be summed. Total CI time also includes checkout, Qt/cache transfer, verification and post steps."}
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result))
summary = os.environ.get("GITHUB_STEP_SUMMARY")
if summary:
    with open(summary, "a", encoding="utf-8") as output:
        output.write("\nMeasured Windows build commands\n\n| Stage | Seconds |\n| --- | ---: |\n")
        for name, measurement in stages.items():
            output.write(f"| {name} | {measurement['seconds']:.3f} |\n")
        output.write(f"\nsccache: {cache}\n\n{result['notes']}\n")
