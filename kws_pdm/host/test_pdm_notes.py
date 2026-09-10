#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host notes for kws_pdm: shipping model SHA, no LiteRT identity (live mic)."""
from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

PDM = Path(__file__).resolve().parents[1]
ZOO = PDM.parent
EXPECTED = PDM / "host" / "expected.json"
TFLITE = ZOO / "assets" / "depgraph_r060_kd_int8.tflite"
SHIPPING = "ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7"
HEADER = PDM / "audio" / "pdm_pcm16.h"


class PdmNotes(unittest.TestCase):
    def test_expected_refuses_gate3(self) -> None:
        exp = json.loads(EXPECTED.read_text())
        self.assertEqual(exp["measurement"], "kws_pdm_live_mic")
        self.assertNotIn("pred", exp)
        self.assertIn("not GATE 3", exp["notes"])
        self.assertEqual(exp["model"]["sha256"], SHIPPING)
        self.assertEqual(exp["model"]["sha256"], hashlib.sha256(TFLITE.read_bytes()).hexdigest())
        self.assertEqual(exp["pdm"]["clk_out_hz"], 2048000)
        self.assertEqual(exp["pdm"]["fs_hz"], 16000)

    def test_header_pins_pdm_fft_recipe(self) -> None:
        text = HEADER.read_text()
        self.assertIn("#define KWS_PDM_SRC_HZ 24576000u", text)
        self.assertIn("#define KWS_PDM_CLKO_DIV 5u", text)
        self.assertIn("#define KWS_PDM_DECIMATION 64u", text)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
