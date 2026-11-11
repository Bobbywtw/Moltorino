"""Time a real command without hiding its output or changing its exit status."""
import argparse
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--name", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command[1:] if args.command[:1] == ["--"] else args.command
start = time.monotonic()
result = subprocess.run(command, check=False)
measurement = {"stage": args.name, "seconds": round(time.monotonic() - start, 3),
               "exitCode": result.returncode}
target = Path(args.output)
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(json.dumps(measurement, indent=2) + "\n", encoding="utf-8")
print(json.dumps(measurement), flush=True)
raise SystemExit(result.returncode)
