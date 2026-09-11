#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Dump kws_pdm hop PCM from SEGGER RTT **channel 1** to a 16 kHz int16 file.

Same channel split as AmbiqSuite ``pdm_rtt_stream``: ch1 is raw PCM, not a
label. Labels stay on SWO (``nsx view``). This script is **not LiteRT** and
**not GATE 3** — do not score the file with the DS-CNN.

Kill ``nsx view`` / JLinkSWOViewer first — the J-Link OB is exclusive.

Firmware must be built with ``-DKWS_PDM_RTT_PCM=ON``. Style: NSX CoreMark
``rtt_capture.py`` (pylink ``rtt_read``) plus a DTCM scan for the control
block so ``--rtt-addr`` is optional.

Usage:
    source /workspace/scripts/env_floor2.sh
    python3 kws_pdm/host/rtt_pcm_dump.py --duration 3 --out /tmp/pdm.wav
"""
from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

CHANNEL = 1
SAMPLE_RATE_HZ = 16000
RTT_MAGIC = b"SEGGER RTT"
DTCM_BASE = 0x20000000
DTCM_SIZE = 0x80000
SCAN_CHUNK = 0x4000
DEFAULT_DEVICE = "AP510NFA-CBR"
NOTES = (
    "RTT ch1 is 16 kHz int16 hop PCM, not a label. "
    "This capture is not LiteRT and not GATE 3."
)


def pack_wav_pcm16(pcm: bytes, sample_rate: int = SAMPLE_RATE_HZ, channels: int = 1) -> bytes:
    """Return a 44-byte WAV header plus little-endian int16 PCM.

    Parameters
    ----------
    pcm : bytes
        Packed int16 samples, length even.
    sample_rate : int
        Hz. Default 16000.
    channels : int
        Mono = 1.

    Returns
    -------
    bytes
        ``RIFF``/``WAVE`` file.

    Raises
    ------
    ValueError
        If ``pcm`` length is odd.
    """
    if len(pcm) % 2:
        raise ValueError("PCM length must be even (int16)")
    block_align = channels * 2
    byte_rate = sample_rate * block_align
    hdr = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF",
        36 + len(pcm),
        b"WAVE",
        b"fmt ",
        16,
        1,
        channels,
        sample_rate,
        byte_rate,
        block_align,
        16,
        b"data",
        len(pcm),
    )
    return hdr + pcm


def find_magic_addresses(chunk: bytes, base: int) -> list[int]:
    """Return every address of ``SEGGER RTT`` inside *chunk* mapped at *base*."""
    out: list[int] = []
    start = 0
    while True:
        idx = chunk.find(RTT_MAGIC, start)
        if idx < 0:
            return out
        out.append(base + idx)
        start = idx + 1


def scan_dtcm_for_rtt(jlink: object) -> list[int]:
    """Scan Apollo510 DTCM (0x20000000, 512 KiB) for the RTT control block."""
    overlap = len(RTT_MAGIC) - 1
    found: list[int] = []
    seen: set[int] = set()
    offset = 0
    while offset < DTCM_SIZE:
        n = min(SCAN_CHUNK, DTCM_SIZE - offset)
        try:
            data = bytes(jlink.memory_read8(DTCM_BASE + offset, n))  # type: ignore[attr-defined]
        except Exception:  # noqa: BLE001 - unmapped windows
            data = b""
        for addr in find_magic_addresses(data, DTCM_BASE + offset):
            if addr not in seen:
                seen.add(addr)
                found.append(addr)
        if offset + n >= DTCM_SIZE:
            break
        offset += max(1, n - overlap)
    return found


def _open_jlink(device: str, speed_khz: int) -> object:
    import pylink  # type: ignore[import-untyped]

    jlink = pylink.JLink()
    jlink.open()
    jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
    jlink.connect(device, speed=speed_khz)
    return jlink


def _wait_rtt(jlink: object, timeout_s: float = 5.0) -> None:
    import pylink  # type: ignore[import-untyped]

    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            if jlink.rtt_get_num_up_buffers() >= 2:  # type: ignore[attr-defined]
                return
        except pylink.errors.JLinkRTTException:
            pass
        time.sleep(0.05)
    raise RuntimeError("RTT control block not located (need -DKWS_PDM_RTT_PCM=ON)")


def capture_pcm(
    *,
    device: str = DEFAULT_DEVICE,
    speed_khz: int = 4000,
    duration_s: float = 3.0,
    rtt_addr: int | None = None,
) -> bytes:
    """Attach over SWD and drain RTT up-channel 1 for *duration_s* seconds.

    Parameters
    ----------
    device : str
        J-Link device name.
    speed_khz : int
        SWD clock.
    duration_s : float
        Capture window.
    rtt_addr : int or None
        Control-block address; DTCM-scanned when omitted.

    Returns
    -------
    bytes
        Packed little-endian int16 PCM (may be empty if the host lagged).
    """
    jlink = _open_jlink(device, speed_khz)
    try:
        if jlink.halted():  # type: ignore[attr-defined]
            jlink.restart()  # type: ignore[attr-defined]
            time.sleep(0.1)
        addr = rtt_addr
        if addr is None:
            hits = scan_dtcm_for_rtt(jlink)
            if not hits:
                raise RuntimeError(
                    "no SEGGER RTT magic in DTCM; flash kws_pdm with "
                    "-DKWS_PDM_RTT_PCM=ON and kill nsx view"
                )
            addr = hits[0]
        jlink.rtt_start(block_address=addr)  # type: ignore[attr-defined]
        _wait_rtt(jlink)
        chunks: list[bytes] = []
        end = time.monotonic() + duration_s
        while time.monotonic() < end:
            data = jlink.rtt_read(CHANNEL, 4096)  # type: ignore[attr-defined]
            if data:
                chunks.append(bytes(data))
            else:
                time.sleep(0.02)
        return b"".join(chunks)
    finally:
        try:
            jlink.rtt_stop()  # type: ignore[attr-defined]
        except Exception:  # noqa: BLE001
            pass
        jlink.close()  # type: ignore[attr-defined]


def main() -> int:
    p = argparse.ArgumentParser(description=NOTES)
    p.add_argument("--device", default=DEFAULT_DEVICE)
    p.add_argument("--speed", type=int, default=4000)
    p.add_argument(
        "--rtt-addr",
        type=lambda s: int(s, 0),
        default=None,
        help="SEGGER RTT control block; default = DTCM scan",
    )
    p.add_argument("--duration", type=float, default=3.0)
    p.add_argument("--out", type=Path, required=True, help=" .wav or raw .pcm")
    args = p.parse_args()

    print(NOTES, file=sys.stderr)
    print(
        f"reading RTT channel {CHANNEL} at {SAMPLE_RATE_HZ} Hz int16 "
        "(kill nsx view first)",
        file=sys.stderr,
    )
    pcm = capture_pcm(
        device=args.device,
        speed_khz=args.speed,
        duration_s=args.duration,
        rtt_addr=args.rtt_addr,
    )
    n_samp = len(pcm) // 2
    print(f"captured {n_samp} samples ({len(pcm)} bytes)", file=sys.stderr)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.out.suffix.lower() == ".wav":
        args.out.write_bytes(pack_wav_pcm16(pcm))
    else:
        args.out.write_bytes(pcm)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
