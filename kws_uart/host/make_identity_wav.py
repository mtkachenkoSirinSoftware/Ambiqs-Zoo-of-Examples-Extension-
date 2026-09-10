#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write host/synthetic.wav from kws_core golden PCM (identity clip)."""
from __future__ import annotations

import argparse
import pathlib
import struct
import wave


def golden_pcm(path: pathlib.Path) -> list[int]:
    """Load int16 samples from a golden txt (header then n values)."""
    lines = path.read_text().splitlines()
    n = int(lines[0].split()[1])
    return [int(x) for x in lines[1 : 1 + n]]


def write_wav(path: pathlib.Path, pcm: list[int], rate: int = 16000) -> None:
    """Write mono int16 WAV. O(n) pack."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", s) for s in pcm))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--golden", type=pathlib.Path, required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    args = ap.parse_args()
    pcm = golden_pcm(args.golden)
    write_wav(args.out, pcm)
    print(f"wrote {args.out} n={len(pcm)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
