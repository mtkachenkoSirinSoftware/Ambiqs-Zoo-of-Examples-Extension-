#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host tests for kws_uart framing and stream_wav (no EVB)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stream_wav import (  # noqa: E402
    BLOCK_SAMPLES,
    CLIP_SAMPLES,
    encode,
    fit_clip_pcm16,
    frames_for_pcm,
    crc16_ccitt,
    T_AUDIO,
    T_END,
    T_RESET,
    T_START,
)


class Crc(unittest.TestCase):
    def test_ccitt_false_vector(self) -> None:
        self.assertEqual(crc16_ccitt(b"123456789"), 0x29B1)


class FitClip(unittest.TestCase):
    def test_exact_is_unchanged(self) -> None:
        raw = b"\x01\x00" * CLIP_SAMPLES
        out, note = fit_clip_pcm16(raw)
        self.assertEqual(note, "exact")
        self.assertEqual(out, raw)

    def test_short_is_zero_padded(self) -> None:
        raw = b"\x02\x00" * 100
        out, note = fit_clip_pcm16(raw)
        self.assertIn("padded", note)
        self.assertEqual(len(out), CLIP_SAMPLES * 2)
        self.assertEqual(out[:200], raw)

    def test_long_keeps_first_second(self) -> None:
        raw = b"\x03\x00" * (CLIP_SAMPLES + 50)
        out, note = fit_clip_pcm16(raw)
        self.assertIn("truncated", note)
        self.assertEqual(out, raw[: CLIP_SAMPLES * 2])


class Frames(unittest.TestCase):
    def test_one_second_is_start_50_end(self) -> None:
        pcm = bytes(CLIP_SAMPLES * 2)
        frames = frames_for_pcm(pcm)
        self.assertEqual(len(frames), 52)
        self.assertEqual(frames[0], encode(T_START, 0))
        self.assertEqual(frames[-1], encode(T_END, 51))
        hop = pcm[: BLOCK_SAMPLES * 2]
        self.assertEqual(frames[1], encode(T_AUDIO, 1, hop))

    def test_writes_py_wire_blob(self) -> None:
        hop = b"".join((i * 101 - 16000).to_bytes(2, "little", signed=True) for i in range(BLOCK_SAMPLES))
        blob = encode(T_START, 0) + encode(T_AUDIO, 1, hop) + encode(T_END, 2)
        dest = Path(__file__).resolve().parent / "generated" / "py_wire.bin"
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(blob)
        self.assertEqual(dest.read_bytes(), blob)
        self.assertGreater(len(encode(T_RESET, 0)), 0)

    def test_dry_run_identity_wav(self) -> None:
        wav = Path(__file__).resolve().parent / "synthetic.wav"
        if not wav.is_file():
            self.skipTest("run make -C kws_uart embed")
        import subprocess

        r = subprocess.run(
            [sys.executable, str(Path(__file__).resolve().parent / "stream_wav.py"),
             str(wav), "--dry-run"],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertIn("16000 samples", r.stdout)
        self.assertIn("dry run", r.stdout)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
