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
        self.assertNotIn("pred", exp)
        self.assertEqual(exp["firmware"]["backend"], "nsx-audio")
        self.assertEqual(exp["firmware"]["hop_samples"], 320)
        self.assertEqual(exp["firmware"]["fs_hz"], 16000)

    def test_header_pins_pdm_fft_recipe(self) -> None:
        text = HEADER.read_text()
        self.assertIn("#define KWS_PDM_SRC_HZ 24576000u", text)
        self.assertIn("#define KWS_PDM_CLKO_DIV 5u", text)
        self.assertIn("#define KWS_PDM_DECIMATION 64u", text)

    def test_nsx_audio_default_note_is_16khz_hfrc2(self) -> None:
        note = (PDM / "host" / "nsx_audio_default.md").read_text()
        self.assertIn("HFRC2_ADJ", note)
        self.assertIn("16 000 Hz", note)
        self.assertIn("exactly 16 kHz", note)
        self.assertIn("320", note)
        yml = (PDM / "nsx.yml").read_text()
        self.assertIn("nsx-audio", yml)
        src = (PDM / "src" / "nsx_audio_source.c").read_text()
        self.assertIn("KWS_AUDIO_BLOCK_SAMPLES", src)
        self.assertIn("nsx_audio_pdm_default", src)
        main = (PDM / "src" / "main.cc").read_text()
        self.assertIn('Frame %lu  peak=', main)
        self.assertIn("KwsPrintPerfBanner", main)
        self.assertIn("KWS_PDM_RTT_PCM", main)
        self.assertIn("rtt ch1=PCM", main)
        cmake = (PDM / "CMakeLists.txt").read_text()
        self.assertIn("KWS_PDM_RTT_PCM", cmake)
        self.assertIn("option(KWS_PDM_RTT_PCM", cmake)
        rtt_h = (PDM / "src" / "kws_pdm_rtt.h").read_text()
        self.assertIn("kKwsPdmRttPcmChannel = 1", rtt_h)
        rtt_c = (PDM / "src" / "kws_pdm_rtt.c").read_text()
        self.assertIn("SEGGER_RTT_Write", rtt_c)
        self.assertIn('\"PCM\"', rtt_c)
        rtt_md = (PDM / "rtt" / "README.md").read_text()
        self.assertIn("Channel 1 is hop PCM", rtt_md)
        self.assertIn("not a label", rtt_md.lower())

    def test_rtt_pcm_dump_is_ch1_16khz_not_gate3(self) -> None:
        import importlib.util

        path = PDM / "host" / "rtt_pcm_dump.py"
        spec = importlib.util.spec_from_file_location("rtt_pcm_dump", path)
        self.assertIsNotNone(spec)
        assert spec is not None and spec.loader is not None
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.CHANNEL, 1)
        self.assertEqual(mod.SAMPLE_RATE_HZ, 16000)
        self.assertIn("not GATE 3", mod.NOTES)
        self.assertIn("not LiteRT", mod.NOTES)
        dummy = b"\x00\x01\xff\x7f"
        wav = mod.pack_wav_pcm16(dummy)
        self.assertEqual(wav[:4], b"RIFF")
        self.assertEqual(wav[8:12], b"WAVE")
        self.assertEqual(len(wav), 44 + len(dummy))
        src = path.read_text()
        self.assertIn("rtt_read(CHANNEL", src)
        self.assertNotIn("tflite_runtime", src)
        self.assertNotIn("tensorflow", src.lower())


if __name__ == "__main__":
    raise SystemExit(unittest.main())
