#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Send 1 s PCM as usb_rpc INFER to kws_uart (DTR=True, NSX CDC).

Firmware: ``-DKWS_UART_USB_RPC=ON``. The device runs ``KwsApp`` and prints
``pred=`` on SWO. USB carries the same tfds class. Stock
``neuralspotx/examples/usb_rpc`` is a five-class stub on another image.

    python3 host/rpc_infer.py host/synthetic.wav --port /dev/ttyACM0
"""
from __future__ import annotations

import argparse
import pathlib
import sys
import time
import wave

from rpc_wire import (
    CLIP_BYTES,
    TOY_USB_RPC_LABELS,
    decode_infer_resp,
    encode_infer_req,
    encode_ping_req,
    encode_status_req,
    frame,
)
from stream_wav import NSX_CDC_PID, NSX_CDC_VID, classify_port, dtr_level, fit_clip_pcm16


def load_pcm16(path: pathlib.Path) -> bytes:
    """Read 16 kHz mono int16 WAV bytes."""
    with wave.open(str(path), "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2 or w.getframerate() != 16000:
            raise SystemExit(f"{path}: need 16 kHz mono int16")
        return w.readframes(w.getnframes())


def recv_frame(ser, timeout_s: float = 8.0) -> bytes:
    """Read one length-prefixed RPC payload."""
    import serial

    ser.timeout = timeout_s
    hdr = ser.read(4)
    if len(hdr) < 4:
        raise TimeoutError("timeout waiting for RPC length")
    n = int.from_bytes(hdr, "little")
    if n == 0 or n > 1024:
        raise ValueError(f"implausible RPC length {n}")
    body = ser.read(n)
    if len(body) < n:
        raise TimeoutError(f"timeout: expected {n} got {len(body)}")
    return body


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("wav", type=pathlib.Path)
    ap.add_argument("--port", default=None, help="CDC port; default = first 0xCafe/0x4011")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    pcm, note = fit_clip_pcm16(load_pcm16(args.wav))
    if len(pcm) != CLIP_BYTES:
        raise SystemExit(f"clip is {len(pcm)} bytes, want {CLIP_BYTES}")

    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        raise SystemExit("pyserial not installed")

    port = args.port
    if port is None:
        for p in serial.tools.list_ports.comports():
            if classify_port(p.vid, p.pid, p.manufacturer or "", p.product or "") == "nsx-cdc":
                port = p.device
                break
        if port is None:
            raise SystemExit(
                "no NSX CDC 0xCafe/0x4011 — plug the EVB device USB "
                "(not only J-Link) and flash -DKWS_UART_USB_RPC=ON"
            )

    kind = "other"
    for p in serial.tools.list_ports.comports():
        if p.device == port:
            kind = classify_port(p.vid, p.pid, p.manufacturer or "", p.product or "")
            break
    dtr = dtr_level(kind, "high" if kind == "nsx-cdc" else "auto")
    print(
        f"port={port} kind={kind} dtr={'high' if dtr else 'low'} "
        f"clip={args.wav.name} ({note})  NOT toy usb_rpc, not LiteRT"
    )

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = args.baud
    ser.timeout = 1.0
    ser.dsrdtr = False
    ser.dtr = True
    ser.open()
    ser.dtr = True
    time.sleep(2.0)
    ser.reset_input_buffer()
    try:
        ser.write(frame(encode_ping_req(0)))
        ser.flush()
        ping = recv_frame(ser)
        print(f"PING resp {len(ping)} bytes")
        ser.write(frame(encode_status_req()))
        ser.flush()
        st = recv_frame(ser)
        print(f"STATUS resp {len(st)} bytes")
        ser.write(frame(encode_infer_req(pcm)))
        ser.flush()
        raw = recv_frame(ser, timeout_s=15.0)
        resp = decode_infer_resp(raw)
        label = str(resp["label"])
        print(
            f"INFER class_id={resp['class_id']} label={label!r} "
            f"conf={resp['confidence']:.3f}"
        )
        if label in TOY_USB_RPC_LABELS:
            print(
                "ERROR: toy usb_rpc labels — this is the wrong image. "
                "Flash kws_uart with -DKWS_UART_USB_RPC=ON.",
                file=sys.stderr,
            )
            return 2
        print("MCU pred= is on SWO (nsx view). USB class is the same argmax.")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
