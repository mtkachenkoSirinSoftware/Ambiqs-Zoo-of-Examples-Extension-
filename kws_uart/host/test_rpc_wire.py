#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host tests for USB RPC INFER framing (no EVB, not LiteRT)."""
from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rpc_wire import (  # noqa: E402
    CLIP_BYTES,
    NSX_MSG_INFER_REQ,
    TOY_USB_RPC_LABELS,
    decode_infer_resp,
    encode_infer_req,
    encode_key,
    encode_varint,
    encode_varint_field,
    encode_bytes_field,
    encode_fixed32_field,
    frame,
)


class InferWire(unittest.TestCase):
    def test_infer_req_is_32000_int16_not_toy(self) -> None:
        pcm = b"\x01\x00" * 16000
        body = encode_infer_req(pcm)
        wrapped = frame(body)
        self.assertEqual(int.from_bytes(wrapped[:4], "little"), len(body))
        self.assertEqual(len(pcm), CLIP_BYTES)
        self.assertIn(pcm[:8], body)
        self.assertNotIn(b"idle", body)
        self.assertGreater(len(body), CLIP_BYTES)

    def test_decode_infer_resp_tfds_go(self) -> None:
        inner = (
            encode_varint_field(1, 0)
            + encode_varint_field(2, 1)
            + encode_fixed32_field(3, struct.unpack("<I", struct.pack("<f", 0.981))[0])
            + encode_bytes_field(4, b"go")
        )
        payload = encode_varint_field(1, 3) + encode_bytes_field(5, inner)
        resp = decode_infer_resp(payload)
        self.assertEqual(resp["class_id"], 1)
        self.assertEqual(resp["label"], "go")
        self.assertNotIn(resp["label"], TOY_USB_RPC_LABELS)

    def test_varint_32000(self) -> None:
        self.assertEqual(encode_varint(32000), b"\x80\xfa\x01")
        self.assertEqual(encode_key(2, 2), b"\x12")


if __name__ == "__main__":
    raise SystemExit(unittest.main())
