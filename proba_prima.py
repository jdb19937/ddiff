#!/usr/bin/env python3

import shutil
import subprocess
import sys
from pathlib import Path

RADIX      = Path(__file__).resolve().parent
CORPORA    = RADIX / "corpora"
VETERUM    = CORPORA / "veterum"
NOVUM      = CORPORA / "novum"
APPLICATIO = CORPORA / "applicatio"


def proba() -> int:
    if APPLICATIO.exists():
        shutil.rmtree(APPLICATIO)
    APPLICATIO.mkdir()
    for fons in VETERUM.iterdir():
        shutil.copy2(fons, APPLICATIO / fons.name)

    differentia = subprocess.Popen(
        ["diff", "-ruN", "veterum", "novum"],
        cwd=CORPORA,
        stdout=subprocess.PIPE,
    )
    plica = subprocess.Popen(
        [str(RADIX / "ddiff")],
        stdin=differentia.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    differentia.stdout.close()
    applicator = subprocess.Popen(
        [str(RADIX / "dpatch")],
        cwd=APPLICATIO,
        stdin=plica.stdout,
        stderr=subprocess.DEVNULL,
    )
    plica.stdout.close()
    applicator.wait()
    plica.wait()
    differentia.wait()

    comparatio = subprocess.run(
        ["diff", "-r", str(NOVUM), str(APPLICATIO)],
        capture_output=True,
        text=True,
    )
    if comparatio.returncode == 0:
        print("PROBATIO SVCCESSIT")
        shutil.rmtree(APPLICATIO)
        return 0
    print("PROBATIO FALLIT")
    sys.stdout.write(comparatio.stdout)
    sys.stderr.write(comparatio.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(proba())
