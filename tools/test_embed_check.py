#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host tests for tools/embed_model.py --check (A/B/C). No TensorFlow."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ZOO = Path(__file__).resolve().parents[1]
EMBED = ZOO / "tools" / "embed_model.py"
TFLITE = ZOO / "assets" / "depgraph_r060_kd_int8.tflite"
SHIPPING = "ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7"


_class_b_env = os.environ.get("KWS_CLASS_B_TFLITE", "")
CLASS_B = Path(_class_b_env) if _class_b_env else Path("/nonexistent")
CLASS_C = (
    ZOO
    / "kws_clip"
    / "modules"
    / "helia-rt"
    / "tensorflow"
    / "lite"
    / "micro"
    / "examples"
    / "hello_world"
    / "models"
    / "hello_world_int8.tflite"
)

sys.path.insert(0, str(ZOO / "tools"))
from embed_model import classify  # noqa: E402


def _run_check(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(EMBED), str(path), "--check"],
        capture_output=True,
        text=True,
        check=False,
    )


class Classify(unittest.TestCase):
    def test_shipping_geometry_is_class_a(self) -> None:
        info = {
            "sha256": SHIPPING,
            "inputs": [
                {
                    "shape": [1, 49, 10, 1],
                    "type": "INT8",
                    "scale": 0.5899888277053833,
                    "zero_point": 81,
                }
            ],
            "outputs": [
                {
                    "shape": [1, 12],
                    "type": "INT8",
                    "scale": 0.20843878388404846,
                    "zero_point": 42,
                }
            ],
        }
        v = classify(info)
        self.assertEqual(v["class"], "A")
        self.assertTrue(v["can_embed"])

    def test_requantised_same_shape_is_class_b(self) -> None:
        info = {
            "sha256": "deadbeef" * 8,
            "inputs": [
                {
                    "shape": [1, 49, 10, 1],
                    "type": "INT8",
                    "scale": 0.25,
                    "zero_point": 0,
                }
            ],
            "outputs": [
                {
                    "shape": [1, 12],
                    "type": "INT8",
                    "scale": 0.1,
                    "zero_point": 0,
                }
            ],
        }
        v = classify(info)
        self.assertEqual(v["class"], "B")
        self.assertTrue(v["can_embed"])

    def test_wrong_shape_is_class_c(self) -> None:
        info = {
            "sha256": "x",
            "inputs": [
                {
                    "shape": [1, 49, 10],
                    "type": "INT8",
                    "scale": 0.5,
                    "zero_point": 0,
                }
            ],
            "outputs": [
                {
                    "shape": [1, 12],
                    "type": "INT8",
                    "scale": 0.2,
                    "zero_point": 0,
                }
            ],
        }
        v = classify(info)
        self.assertEqual(v["class"], "C")
        self.assertFalse(v["can_embed"])


@unittest.skipUnless(TFLITE.is_file(), "shipping .tflite missing")
class CheckCli(unittest.TestCase):
    def test_shipping_tflite_is_class_a(self) -> None:
        proc = _run_check(TFLITE)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        payload = json.loads(proc.stdout)
        self.assertEqual(payload["compatibility"]["class"], "A")
        self.assertEqual(payload["sha256"], SHIPPING)


@unittest.skipUnless(CLASS_C.is_file(), "helia-rt hello_world not vendored")
class CheckClassC(unittest.TestCase):
    def test_hello_world_is_class_c(self) -> None:
        proc = _run_check(CLASS_C)
        self.assertEqual(proc.returncode, 2, proc.stderr)
        payload = json.loads(proc.stdout)
        self.assertEqual(payload["compatibility"]["class"], "C")
        self.assertFalse(payload["compatibility"]["can_embed"])


@unittest.skipUnless(CLASS_B.is_file(), "optional class-B .tflite not in this checkout")
class CheckClassB(unittest.TestCase):
    def test_class_b_embeds(self) -> None:
        proc = _run_check(CLASS_B)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        payload = json.loads(proc.stdout)
        self.assertEqual(payload["compatibility"]["class"], "B")
        self.assertNotEqual(payload["sha256"], SHIPPING)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            emb = subprocess.run(
                [sys.executable, str(EMBED), str(CLASS_B), "--out-dir", str(out)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(emb.returncode, 0, emb.stderr + emb.stdout)
            hdr = (out / "kws_model_contract.h").read_text()
            self.assertIn(payload["sha256"], hdr)
            self.assertNotIn(SHIPPING, hdr)
            self.assertIn("KWS_MODEL_INPUT_TYPE_INT8", hdr)
            self.assertTrue((out / "kws_model_data.cc").is_file())


class KwsInferHeader(unittest.TestCase):
    def test_shipping_emits_kws_infer_drop_in_without_labels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp) / "kws_model_data.h"
            out = Path(tmp) / "zoo"
            proc = subprocess.run(
                [
                    sys.executable,
                    str(EMBED),
                    str(TFLITE),
                    "--out-dir",
                    str(out),
                    "--kws-infer-header",
                    str(header),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr + proc.stdout)
            text = header.read_text()
            self.assertIn("unsigned char kws_model_data[]", text)
            self.assertIn(f"kws_model_data_len = {TFLITE.stat().st_size}", text)
            self.assertIn(SHIPPING, text)
            self.assertNotIn("static const char *kLabels", text)
            self.assertNotIn('"silence"', text)
            blob = TFLITE.read_bytes()
            self.assertIn(f"0x{blob[0]:02x},", text)
            self.assertIn("kLabels[] unchanged", proc.stdout)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
