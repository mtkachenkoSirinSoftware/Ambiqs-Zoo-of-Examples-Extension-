#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host tests for tools/pack_tarball.py (no EVB, no nsx)."""
from __future__ import annotations

import hashlib
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

ZOO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ZOO / "tools"))
from pack_tarball import (  # noqa: E402
    ARC_ROOT,
    FORBIDDEN_SUBSTR,
    REQUIRED,
    SHIPPING_SHA,
    iter_members,
    pack,
    sha256_file,
    verify_archive,
)


class Members(unittest.TestCase):
    def test_required_are_selected(self) -> None:
        names = {p.as_posix() for p in iter_members(ZOO)}
        missing = [r for r in REQUIRED if r not in names]
        self.assertEqual(missing, [])

    def test_excludes_build_and_modules(self) -> None:
        names = {p.as_posix() for p in iter_members(ZOO)}
        self.assertFalse(any("/build/" in n or n.startswith("build/") for n in names))
        self.assertFalse(any("/modules/" in n for n in names))
        self.assertFalse(any(n.endswith("kws_model_manifest.json") for n in names))
        self.assertFalse(any("host/generated/" in n for n in names))
        self.assertFalse(any(".nsx/" in n for n in names))

    def test_no_forbidden_paths(self) -> None:
        names = {p.as_posix() for p in iter_members(ZOO)}
        blob = "\n".join(names)
        for bad in FORBIDDEN_SUBSTR:
            self.assertNotIn(bad, blob)


class RoundTrip(unittest.TestCase):
    def test_pack_verify_shipping_sha(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "ambiq_kws_examples.tar.gz"
            info = pack(ZOO, archive)
            self.assertTrue(archive.is_file())
            self.assertEqual(info["sha256"], sha256_file(archive))
            verify_archive(archive)
            with tarfile.open(archive, "r:gz") as tar:
                names = [m.name for m in tar.getmembers() if m.isfile()]
            self.assertTrue(all(n.startswith(ARC_ROOT + "/") for n in names))
            self.assertIn(f"{ARC_ROOT}/PACK_MANIFEST.txt", names)
            self.assertNotIn(f"{ARC_ROOT}/kws_uart/model/generated/kws_model_manifest.json", names)
            with tarfile.open(archive, "r:gz") as tar:
                fh = tar.extractfile(f"{ARC_ROOT}/assets/depgraph_r060_kd_int8.tflite")
                assert fh is not None
                tflite = fh.read()
            self.assertEqual(hashlib.sha256(tflite).hexdigest(), SHIPPING_SHA)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
