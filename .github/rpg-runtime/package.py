"""Package the compiled core and its owned acceptance bridge."""
import json
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
output = Path(sys.argv[1]).resolve()
if not output.is_dir() or any(output.iterdir()):
    raise RuntimeError("CANDIDATE_OUTPUT_INVALID")
build = root / ".cache/retroarch"
notices = (root / "license.txt").read_bytes() + b"\nRetroArch frontend:\n" + (build / "COPYING").read_bytes()
# Preserve notices supplied by the recursive dependency tree in this build.
for directory, dirs, files in os.walk(root / "externals"):
    dirs[:] = sorted(name for name in dirs if not name.startswith("."))
    for name in sorted(files):
        if name.upper().startswith(("LICENSE", "COPYING")):
            path = Path(directory) / name
            if path.is_file() and path.stat().st_size < 1024 * 1024:
                notices += b"\n" + str(path.relative_to(root)).encode() + b":\n" + path.read_bytes()
with tempfile.TemporaryDirectory(dir=root / ".cache", prefix="package-") as temporary:
    stage = Path(temporary)
    for name in ("azahar_libretro.js", "azahar_libretro.wasm"):
        shutil.copyfile(build / name, stage / name)
    (stage / "build.json").write_text(json.dumps({"minimumEJSVersion": "4.3.0", "version": "retrom-content-result-v1"}) + "\n")
    shutil.copyfile(root / "config/core.json", stage / "core.json")
    (stage / "license.txt").write_bytes(notices)
    for path in stage.iterdir():
        path.chmod(0o644)
        os.utime(path, (0, 0))
    subprocess.run(["7z", "a", "-mtm=off", "-mta=off", "-mtc=off", "-bd", "-bso0", "-bsp0", "-t7z",
                    str(output / "azahar-thread-wasm.data"), *sorted(p.name for p in stage.iterdir())], cwd=stage, check=True)
(output / "LICENSE").write_bytes(notices)
timestamp = datetime.fromtimestamp(1778550856, timezone.utc).isoformat()
(output / "azahar.json").write_text(json.dumps({"core": "azahar", "buildStart": timestamp,
    "buildEnd": timestamp, "options": json.loads((root / "config/core.json").read_text())["options"]}) + "\n")
subprocess.run([sys.executable, str(root / ".github/rpg-runtime/candidate_descriptor.py"),
                "finalize", str(output), "--core-id", "azahar"], check=True)
