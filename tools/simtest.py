#!/usr/bin/env python3
"""Run every EmbSIM scenario of the project in parallel and summarise.

    tools/simtest.py                    # all scenarios in sim/project.json
    tools/simtest.py lockout alarms     # some of them
    EMBSIM=/path/to/embsim tools/simtest.py

The firmware must be built first (cmake --build --preset firmware).
Exit code 0 if every assertion passed. Reports go to build/sim/<name>.json.
Standard library only.
"""
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "sim" / "project.json"
OUT = ROOT / "build" / "sim"


def embsim():
    """EMBSIM, then a build in ~/EmbSIM, then PATH. An older, unrelated
    `embsim` CLI may come first on PATH: check that it takes a project."""
    for candidate in (os.environ.get("EMBSIM"),
                      str(Path.home() / "EmbSIM" / "build" / "dev" / "bin" / "embsim"),
                      shutil.which("embsim")):
        if not candidate or not Path(candidate).is_file():
            continue
        usage = subprocess.run([candidate], capture_output=True, text=True)
        if "run PROJECT" in usage.stdout + usage.stderr:
            return candidate
    sys.exit("EmbSIM not found (github.com/EmbLinkOs/EmbSIM): set EMBSIM to its embsim binary")


def run(binary, scenario):
    name = Path(scenario).stem
    report = OUT / f"{name}.json"
    start = time.monotonic()
    proc = subprocess.run([binary, "run", str(PROJECT), "--scenario", str(PROJECT.parent / scenario), "--json"],
                          capture_output=True, text=True)
    report.write_text(proc.stdout)
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return name, None, [f"no report (exit {proc.returncode}): {proc.stderr.strip()[:200]}"], time.monotonic() - start
    failures = [f"{a['id']}: {a.get('reason', '')}" for a in data["assertions"] if not a["ok"]]
    return name, data["assertions"], failures, time.monotonic() - start


def main():
    project = json.loads(PROJECT.read_text())
    scenarios = project["scenarios"]
    if len(sys.argv) > 1:
        wanted = set(sys.argv[1:])
        scenarios = [s for s in scenarios if Path(s).stem in wanted]
    image = (PROJECT.parent / project["boards"][0]["firmware"]["image"]).resolve()
    if not image.is_file():
        sys.exit(f"{image} missing: cmake --build --preset firmware")

    OUT.mkdir(parents=True, exist_ok=True)
    binary = embsim()
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda s: run(binary, s), scenarios))

    failed = 0
    for name, assertions, failures, seconds in results:
        total = len(assertions) if assertions else 0
        status = "PASS" if assertions is not None and not failures else "FAIL"
        failed += status == "FAIL"
        print(f"{status}  {name:<20} {total - len(failures) if assertions else 0:>3}/{total:<3} {seconds:6.1f} s")
        for f in failures:
            print(f"        {f}")
    print(f"{len(results) - failed}/{len(results)} scenarios passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
