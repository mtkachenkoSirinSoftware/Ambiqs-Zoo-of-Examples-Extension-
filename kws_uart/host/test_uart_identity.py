#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host identity: synthetic.wav PCM SHA matches golden and expected.json."""
from __future__ import annotations

import hashlib
import json
import sys
import unittest
import wave
from pathlib import Path

UART = Path(__file__).resolve().parents[1]
ZOO = UART.parent
GOLDEN = ZOO / "kws_core" / "tests" / "golden" / "synthetic.txt"
WAV = UART / "host" / "synthetic.wav"
EXPECTED = UART / "host" / "expected.json"
TFLITE = ZOO / "assets" / "depgraph_r060_kd_int8.tflite"
SHIPPING = "ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7"


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sha256_golden_pcm(path: Path) -> tuple[str, int]:
    lines = path.read_text().splitlines()
    n = int(lines[0].split()[1])
    pcm = [int(x) for x in lines[1 : 1 + n]]
    blob = b"".join(int(s).to_bytes(2, "little", signed=True) for s in pcm)
    return hashlib.sha256(blob).hexdigest(), n


def wav_pcm16(path: Path) -> bytes:
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 1
        assert w.getsampwidth() == 2
        assert w.getframerate() == 16000
        return w.readframes(w.getnframes())


class UartIdentity(unittest.TestCase):
    def test_wav_matches_golden_and_expected(self) -> None:
        self.assertTrue(WAV.is_file(), f"missing {WAV}; run make -C kws_uart embed")
        gold_sha, gold_n = sha256_golden_pcm(GOLDEN)
        pcm = wav_pcm16(WAV)
        self.assertEqual(len(pcm) // 2, gold_n)
        self.assertEqual(hashlib.sha256(pcm).hexdigest(), gold_sha)
        exp = json.loads(EXPECTED.read_text())
        self.assertEqual(exp["clip"]["sha256"], gold_sha)
        self.assertEqual(exp["pred"], "go")
        self.assertEqual(exp["label"], 1)
        self.assertEqual(exp["model"]["sha256"], sha256_file(TFLITE))
        self.assertEqual(exp["model"]["sha256"], SHIPPING)
        labels = (ZOO / "assets" / "LABELS.md").read_text()
        self.assertIn("| 1 | **`go`** | `unknown` |", labels)
        self.assertIn("| 11 | `_unknown_` | **`go`** |", labels)
        main = (UART / "src" / "main.cc").read_text()
        self.assertIn("KwsPrintPerfBanner", main)
        self.assertIn("KwsPrintInvokeTail", main)
        self.assertIn("labels=tfds index 1=go", main)
        self.assertIn("usb_rpc INFER is a 5-class toy", main)
        cmake = (UART / "CMakeLists.txt").read_text()
        self.assertIn("KWS_UART_USB_CDC", cmake)
        self.assertIn("KWS_UART_USB_RPC", cmake)
        yml = (UART / "nsx.yml").read_text()
        self.assertIn("nsx-usb", yml)
        self.assertIn("nsx-nanopb", yml)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
