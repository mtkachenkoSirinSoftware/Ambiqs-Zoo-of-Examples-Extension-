#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host identity check for kws_clip (no EVB, no heliaRT).

Confirms the embedded clip SHA matches the golden PCM and host/expected.json.
Does not run the DS-CNN (that is LiteRT in expected.json / MCU on the EVB).
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
import unittest
from pathlib import Path

CLIP = Path(__file__).resolve().parents[1]
ZOO = CLIP.parent
GOLDEN = ZOO / "kws_core" / "tests" / "golden" / "synthetic.txt"
HEADER = CLIP / "audio" / "generated" / "kws_clip_data.h"
EXPECTED = CLIP / "host" / "expected.json"
TFLITE = ZOO / "assets" / "depgraph_r060_kd_int8.tflite"
_SHA = re.compile(r'#define KWS_CLIP_SHA256 "([0-9a-f]{64})"')
_NAME = re.compile(r'#define KWS_CLIP_NAME "([^"]+)"')
_N = re.compile(r"#define KWS_CLIP_N_SAMPLES (\d+)")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def sha256_golden_pcm(path: Path) -> tuple[str, int]:
    lines = path.read_text().splitlines()
    n = int(lines[0].split()[1])
    pcm = [int(x) for x in lines[1 : 1 + n]]
    blob = b"".join(int(s).to_bytes(2, "little", signed=True) for s in pcm)
    return hashlib.sha256(blob).hexdigest(), n


class ClipIdentity(unittest.TestCase):
    def test_header_matches_golden_and_expected(self) -> None:
        self.assertTrue(HEADER.is_file(), f"missing {HEADER}; run make -C kws_clip embed")
        text = HEADER.read_text()
        sha_h = _SHA.search(text)
        name_h = _NAME.search(text)
        n_h = _N.search(text)
        self.assertIsNotNone(sha_h)
        self.assertIsNotNone(name_h)
        self.assertIsNotNone(n_h)
        assert sha_h is not None and name_h is not None and n_h is not None
        gold_sha, gold_n = sha256_golden_pcm(GOLDEN)
        self.assertEqual(sha_h.group(1), gold_sha)
        self.assertEqual(int(n_h.group(1)), gold_n)
        exp = json.loads(EXPECTED.read_text())
        self.assertEqual(exp["clip"]["sha256"], gold_sha)
        self.assertEqual(exp["clip"]["name"], name_h.group(1))
        self.assertEqual(exp["pred"], "go")
        self.assertEqual(exp["model"]["sha256"], sha256_file(TFLITE))
        self.assertEqual(
            exp["model"]["sha256"],
            "ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7",
        )
        self.assertEqual(exp["label"], 1)
        labels = (ZOO / "assets" / "LABELS.md").read_text()
        self.assertIn("| 1 | **`go`** | `unknown` |", labels)
        self.assertIn("| 11 | `_unknown_` | **`go`** |", labels)
        self.assertIn("nsx-pmu-armv8m", (CLIP / "nsx.yml").read_text())
        cmake = (CLIP / "CMakeLists.txt").read_text()
        self.assertIn("nsx::pmu_armv8m", cmake)
        self.assertIn("KWS_CLIP_PMU", cmake)
        main = (CLIP / "src" / "main.cc").read_text()
        self.assertIn("KwsPrintPerfBanner", main)
        self.assertIn("PrintClipPmuCsv", main)
        self.assertIn("labels=tfds index 1=go", main)
        profiler = (CLIP / "src" / "nsx_pmu_profiler.cc").read_text()
        self.assertIn("ARM_PMU_CPU_CYCLES", profiler)
        self.assertIn("ARM_PMU_INST_RETIRED", profiler)
        runner = (ZOO / "kws_core" / "model" / "backends" / "runner_helia_rt.cc").read_text()
        self.assertIn("dwt_cycles=", runner)
        self.assertIn("pmu_cycles=", runner)
        self.assertIn("inst_retired=", runner)
        self.assertIn("model-only, this binary", runner)
        self.assertIn("A mismatch is a fact, not a bug", runner)
        self.assertIn("NSX_PMU_PRESET_ML_DEFAULT", runner)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
