#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Minimal nanopb-compatible encoder for kws_uart USB RPC (no grpcio-tools).

Wire: ``[uint32 LE length][NsxRpcMessage]``. INFER input is 16000 int16 LE
(32000 bytes). This is **not** the neuralspotx ``usb_rpc`` 5-class toy, **not**
LiteRT on the host, and **not** GATE 3 by itself — the MCU `pred=` on SWO
is the identity check against `host/expected.json`.
"""
from __future__ import annotations

import struct

NSX_MSG_PING_REQ = 0
NSX_MSG_PING_RESP = 1
NSX_MSG_INFER_REQ = 2
NSX_MSG_INFER_RESP = 3
NSX_MSG_STATUS_REQ = 4
NSX_MSG_STATUS_RESP = 5

CLIP_BYTES = 16000 * 2


def encode_varint(n: int) -> bytes:
    """Protobuf base-128 varint. O(log n)."""
    out = bytearray()
    x = int(n)
    while x > 0x7F:
        out.append((x & 0x7F) | 0x80)
        x >>= 7
    out.append(x & 0x7F)
    return bytes(out)


def encode_key(field: int, wire: int) -> bytes:
    """Return the protobuf key for *field* and *wire* type."""
    return encode_varint((field << 3) | wire)


def encode_bytes_field(field: int, payload: bytes) -> bytes:
    """Length-delimited field."""
    return encode_key(field, 2) + encode_varint(len(payload)) + payload


def encode_varint_field(field: int, value: int) -> bytes:
    """Varint field."""
    return encode_key(field, 0) + encode_varint(value)


def encode_fixed32_field(field: int, bits: int) -> bytes:
    """32-bit little-endian field (wire type 5)."""
    return encode_key(field, 5) + struct.pack("<I", bits & 0xFFFFFFFF)


def encode_ping_req(seq: int) -> bytes:
    """NsxRpcMessage PING_REQ."""
    inner = encode_varint_field(1, seq)
    return encode_varint_field(1, NSX_MSG_PING_REQ) + encode_bytes_field(2, inner)


def encode_status_req() -> bytes:
    """NsxRpcMessage STATUS_REQ."""
    inner = encode_varint_field(1, 0)
    return encode_varint_field(1, NSX_MSG_STATUS_REQ) + encode_bytes_field(6, inner)


def encode_infer_req(pcm: bytes, model_id: int = 0) -> bytes:
    """NsxRpcMessage INFER_REQ. *pcm* must be 32000 bytes of int16 LE."""
    if len(pcm) != CLIP_BYTES:
        raise ValueError(f"INFER wants {CLIP_BYTES} bytes, got {len(pcm)}")
    inner = encode_varint_field(1, model_id) + encode_bytes_field(2, pcm)
    return encode_varint_field(1, NSX_MSG_INFER_REQ) + encode_bytes_field(4, inner)


def frame(payload: bytes) -> bytes:
    """4-byte LE length prefix + payload."""
    return struct.pack("<I", len(payload)) + payload


def read_varint(buf: bytes, i: int) -> tuple[int, int]:
    """Return ``(value, next_index)``."""
    shift = 0
    n = 0
    while i < len(buf):
        b = buf[i]
        i += 1
        n |= (b & 0x7F) << shift
        if (b & 0x80) == 0:
            return n, i
        shift += 7
        if shift > 63:
            raise ValueError("varint too long")
    raise ValueError("truncated varint")


def iter_fields(buf: bytes):
    """Yield ``(field, wire, value_or_bytes)``."""
    i = 0
    while i < len(buf):
        key, i = read_varint(buf, i)
        field, wire = key >> 3, key & 7
        if wire == 0:
            val, i = read_varint(buf, i)
            yield field, wire, val
        elif wire == 2:
            n, i = read_varint(buf, i)
            yield field, wire, buf[i : i + n]
            i += n
        elif wire == 5:
            yield field, wire, buf[i : i + 4]
            i += 4
        else:
            raise ValueError(f"unsupported wire {wire}")


def decode_infer_resp(payload: bytes) -> dict[str, object]:
    """Parse an NsxRpcMessage INFER_RESP. Toy usb_rpc labels are not accepted as identity."""
    msg_type = None
    inner = b""
    for field, _wire, val in iter_fields(payload):
        if field == 1:
            msg_type = val
        elif field == 5:
            inner = val
    if msg_type != NSX_MSG_INFER_RESP:
        raise ValueError(f"not INFER_RESP (type={msg_type})")
    out: dict[str, object] = {"model_id": 0, "class_id": -1, "confidence": 0.0, "label": ""}
    for field, wire, val in iter_fields(inner):
        if field == 1:
            out["model_id"] = val
        elif field == 2:
            out["class_id"] = val
        elif field == 3 and wire == 5:
            out["confidence"] = struct.unpack("<f", val)[0]
        elif field == 4:
            out["label"] = bytes(val).decode("ascii", errors="replace")
    return out


TOY_USB_RPC_LABELS = frozenset({"idle", "walk", "run", "gesture", "unknown"})
