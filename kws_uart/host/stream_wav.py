#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Send a 1 s WAV to kws_uart (PRINT UART or nsx-usb CDC).

The MCU never parses WAV. This tool decodes to mono 16 kHz PCM16, pads or
truncates to exactly 1 s, frames it (PROTOCOL.md), and bursts it into MCU RAM.
Firmware infers after END. Labels are on SWO (`nsx view`), not this port.

Do not peak-normalise. Training and firmware both divide by signed max(x) of
the 1 s window.

Two inverted DTR contracts (usb_serial vs J-Link-OB):

* NSX CDC VID/PID ``0xCafe``/``0x4011`` — ``DTR=True`` or the device ignores RX
  (neuralspotx ``usb_serial``).
* J-Link VCP / PRINT UART — hold DTR/RTS **low** or the MCU resets.

``--dtr auto`` (default) picks high for NSX CDC and low otherwise.
``--list-ports`` prints the classification. Pick the right port.
"""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys
import time
import wave

SAMPLE_RATE = 16000
BLOCK_SAMPLES = 320
CLIP_SAMPLES = 16000

MAGIC = 0xA51C
VERSION = 1
T_START, T_AUDIO, T_END, T_RESET = 0x01, 0x02, 0x03, 0x04
T_STATUS, T_PREDICTION, T_ACK, T_NACK = 0x81, 0x82, 0x83, 0x84

NSX_CDC_VID = 0xCAFE
NSX_CDC_PID = 0x4011
SEGGER_VID = 0x1366

LABELS = [
    "down",
    "go",
    "left",
    "no",
    "off",
    "on",
    "right",
    "stop",
    "up",
    "yes",
    "_silence_",
    "_unknown_",
]


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE. Check value of b'123456789' is 0x29B1."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(msg_type: int, seq: int, payload: bytes = b"") -> bytes:
    """Serialise one frame (little-endian). Payload length <= 1024."""
    header = struct.pack("<HBBHH", MAGIC, VERSION, msg_type, seq, len(payload))
    header += struct.pack("<H", crc16_ccitt(header))
    return header + payload + struct.pack("<H", crc16_ccitt(payload))


def classify_port(
    vid: int | None,
    pid: int | None,
    manufacturer: str = "",
    product: str = "",
) -> str:
    """Return ``nsx-cdc``, ``jlink-vcp``, or ``other``.

    Parameters
    ----------
    vid, pid : int or None
        USB identifiers.
    manufacturer, product : str
        Port strings from ``list_ports``.

    Returns
    -------
    str
        Port kind. NSX CDC is Cafe:4011 as in ``usb_serial``.
    """
    vid_i = int(vid or 0)
    pid_i = int(pid or 0)
    if vid_i == NSX_CDC_VID and pid_i == NSX_CDC_PID:
        return "nsx-cdc"
    blob = f"{manufacturer} {product}".lower()
    if vid_i == SEGGER_VID or "segger" in blob or "j-link" in blob or "jlink" in blob:
        return "jlink-vcp"
    return "other"


def dtr_level(kind: str, override: str = "auto") -> bool:
    """Return True if DTR should be asserted.

    Parameters
    ----------
    kind : str
        ``classify_port`` result.
    override : str
        ``auto``, ``low``, or ``high``.
    """
    if override == "high":
        return True
    if override == "low":
        return False
    return kind == "nsx-cdc"


def list_pcm_ports() -> list[tuple[str, str, str]]:
    """Return ``(device, kind, description)`` rows from pyserial."""
    import serial.tools.list_ports

    rows: list[tuple[str, str, str]] = []
    for p in serial.tools.list_ports.comports():
        kind = classify_port(p.vid, p.pid, p.manufacturer or "", p.product or "")
        desc = p.description or ""
        rows.append((p.device, kind, desc))
    return rows


def load_wav(path: pathlib.Path) -> bytes:
    """WAV → mono 16 kHz int16 LE bytes. Rejects other rates/widths."""
    with wave.open(str(path), "rb") as w:
        n_ch, width, rate, n_frames = (
            w.getnchannels(),
            w.getsampwidth(),
            w.getframerate(),
            w.getnframes(),
        )
        raw = w.readframes(n_frames)
    if width != 2:
        raise SystemExit(f"{path}: {width * 8}-bit PCM unsupported; convert to 16-bit first")
    if rate != SAMPLE_RATE:
        raise SystemExit(f"{path}: {rate} Hz unsupported; need {SAMPLE_RATE} Hz (no resample)")
    if n_ch == 1:
        return raw
    if n_ch != 2:
        raise SystemExit(f"{path}: {n_ch} channels unsupported")
    out = bytearray()
    for i in range(0, len(raw), 4):
        l, r = struct.unpack_from("<hh", raw, i)
        out += struct.pack("<h", int(round((l + r) / 2)))
    return bytes(out)


def fit_clip_pcm16(pcm: bytes, n_samples: int = CLIP_SAMPLES) -> tuple[bytes, str]:
    """Pad or truncate to exactly one 1 s window. Returns (pcm, note)."""
    n = len(pcm) // 2
    want = n_samples * 2
    if n == n_samples:
        return pcm[:want], "exact"
    if n < n_samples:
        return pcm + b"\x00" * (want - len(pcm)), f"padded {n}->{n_samples}"
    return pcm[:want], f"truncated {n}->{n_samples}"


def frames_for_pcm(pcm: bytes) -> list[bytes]:
    """START + 50 AUDIO_BLOCK + END for a 1 s clip."""
    n_blocks = len(pcm) // (BLOCK_SAMPLES * 2)
    out = [encode(T_START, 0)]
    for i in range(n_blocks):
        chunk = pcm[i * BLOCK_SAMPLES * 2 : (i + 1) * BLOCK_SAMPLES * 2]
        out.append(encode(T_AUDIO, (i + 1) & 0xFFFF, chunk))
    out.append(encode(T_END, (n_blocks + 1) & 0xFFFF))
    return out


def open_pcm_serial(port: str, baud: int, dtr: bool = False):
    """Open the PCM VCOM with an explicit DTR/RTS polarity."""
    import serial

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.05
    ser.dsrdtr = False
    ser.rtscts = False
    ser.dtr = dtr
    ser.rts = False
    ser.open()
    ser.dtr = dtr
    ser.rts = False
    time.sleep(0.2 if not dtr else 0.3)
    return ser


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("wav", type=pathlib.Path, nargs="*")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument(
        "--dtr",
        choices=("auto", "low", "high"),
        default="auto",
        help="auto: high on NSX CDC 0xCafe/0x4011, low on J-Link VCP",
    )
    ap.add_argument(
        "--list-ports",
        action="store_true",
        help="print J-Link VCP vs NSX CDC (usb_serial: pick the right port)",
    )
    ap.add_argument(
        "--realtime",
        action="store_true",
        help="pace blocks at 20 ms (optional; MCU stores first, then infers)",
    )
    ap.add_argument("--dry-run", action="store_true", help="frame it but open no port")
    args = ap.parse_args()

    if args.list_ports:
        try:
            rows = list_pcm_ports()
        except ImportError:
            raise SystemExit("pyserial not installed")
        if not rows:
            print("no serial ports")
            return 0
        for dev, kind, desc in rows:
            dtr = "high" if dtr_level(kind, "auto") else "low"
            print(f"{dev:24} {kind:10} dtr={dtr}  {desc}")
        return 0

    if not args.wav:
        raise SystemExit("pass one or more WAV paths (16 kHz mono int16, 1 s)")

    clips: list[tuple[str, bytes]] = []
    for wav in args.wav:
        pcm, note = fit_clip_pcm16(load_wav(wav))
        clips.append((wav.stem, pcm))
        if args.dry_run:
            print(f"{wav.stem}: {len(pcm) // 2} samples ({note})")

    ser = None
    if not args.dry_run:
        try:
            import serial  # noqa: F401
        except ImportError:
            raise SystemExit("pyserial not installed")
        kind = "other"
        try:
            import serial.tools.list_ports

            for p in serial.tools.list_ports.comports():
                if p.device == args.port:
                    kind = classify_port(p.vid, p.pid, p.manufacturer or "", p.product or "")
                    break
        except Exception:  # noqa: BLE001
            kind = "other"
        dtr = dtr_level(kind, args.dtr)
        print(
            f"port={args.port} kind={kind} dtr={'high' if dtr else 'low'} "
            "(NSX CDC=0xCafe/0x4011 needs DTR; J-Link VCP needs DTR low)"
        )
        ser = open_pcm_serial(args.port, args.baud, dtr=dtr)

    try:
        for name, pcm in clips:
            _stream_pcm(name, pcm, args.baud, args.realtime, args.dry_run, ser)
    finally:
        if ser is not None:
            ser.close()
    return 0


def _stream_pcm(
    name: str, pcm: bytes, baud: int, realtime: bool, dry_run: bool, ser
) -> None:
    frames = frames_for_pcm(pcm)
    total = sum(len(f) for f in frames)
    print(
        f"{name}: {len(pcm) // 2} samples, {len(frames)} frames, {total} bytes "
        f"(>= {total * 10 / baud:.2f} s at {baud} baud); MCU stores 1 s then infers "
        "(labels on SWO / nsx view, not this UART)"
    )
    if dry_run:
        print("  dry run: nothing transmitted")
        return

    rst = encode(T_RESET, 0)
    ser.write(rst)
    time.sleep(0.1)
    ser.reset_input_buffer()
    t0 = time.time()
    for i, f in enumerate(frames):
        ser.write(f)
        if realtime and i:
            target = t0 + i * BLOCK_SAMPLES / SAMPLE_RATE
            delay = target - time.time()
            if delay > 0:
                time.sleep(delay)


if __name__ == "__main__":
    raise SystemExit(main())
