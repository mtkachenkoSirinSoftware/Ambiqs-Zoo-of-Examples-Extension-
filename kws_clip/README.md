# kws_clip — 1 s flash PCM → MFCC → heliaRT on Apollo510

Always-on KWS on a **known** 1 s clip stored in flash. Labels print on **SWO**
(`nsx view`), same transport as `neuralspotx/examples/kws_infer`.

This is not dummy-input `kws_infer` (zeros / PRNG into the tensor). The PCM
is the GATE-4 golden `synthetic` clip (16 kHz mono int16, 16000 samples). Host
LiteRT on that PCM predicts **`go`**. MCU `pred=` is comparable only on this
PCM — not a spoken GSC file, not PDM. Names are **tfds** order (`go` = index
1). `kws_infer` prints MLPerf order (`go` = index 11). See
[`../assets/LABELS.md`](../assets/LABELS.md).

## Hardware

Apollo510 EVB + J-Link. No USB-UART, no MEMS.

## Build

```bash
cd ambiq_kws_examples/kws_clip
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Regenerate embedded arrays (optional; CMake does this if they are missing):

```bash
make embed
```

Host identity check (no board):

```bash
cmake -S . -B build-host && ctest --test-dir build-host --output-on-failure
# or: python3 host/test_clip_identity.py
```

## Expected output (SWO)

```
kws_clip runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
clip=synthetic n=16000 sha256=341c766cf618
model sha256=ae08012b5a5d  (host LiteRT pred on this PCM: see host/expected.json)
labels=tfds index 1=go (not kws_infer MLPerf index 11=go); assets/LABELS.md
pred=go score=0.980 fired=0|1 invoke_cycles=<DWT> invoke_us=<CYCCNT/cpu_hz>
  invoke_us = CYCCNT / cpu_hz (see perf_mode=); not hpx profile
--- Per-Layer PMU ---
  last Invoke on synthetic PCM; not hpx profile; not always-on hop time
"Layer","Op",...
```

| Line | What it is |
|---|---|
| `pred=go` | last raw argmax; must match `host/expected.json` (tfds index 1) |
| `fired=` | recognizer (smooth / threshold / debounce); not GATE 3 |
| `invoke_cycles=` / `invoke_us=` | DWT of **this** `Invoke` / CYCCNT÷`cpu_hz`; not `hpx profile` |
| `--- Per-Layer PMU ---` | `nsx-pmu-armv8m` CSV for that Invoke; not always-on |
| `arena_used=` | target allocator; host reference kernel used 12352 B as a lower bound |

`score=` on the MCU is softmax of int8 logits; it can differ in the fourth
decimal from the host LiteRT float. The **name** `go` is the check.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | `nsx_system_init`, play clip, SWO `pred=` / PMU CSV |
| `src/nsx_pmu_profiler.cc` | TFLM `MicroProfilerInterface` (kws_infer shape) |
| `src/flash_clip_source.c` | AudioSource wrapper around `FlashClipPlayer` |
| `../kws_core/` | frontend, quantize, helia runner, recognizer |
| `audio/generated/` | embedded PCM + SHA256 |
| `model/generated/` | embedded `.tflite` + contract |
| `host/expected.json` | host LiteRT identity for this PCM |
| `nsx.yml` / `nsx.lock` | helia-rt 1.19.0 + ns-cmsis-nn 7.31.0 pins |

## Linked image (host, not latency)

`nsx build` produces `build/apollo510_evb/kws_clip` (ELF) and `.bin`. One
successful link on this tree:

| Segment | Bytes |
|---|---|
| `.text` | 337616 |
| `.data` | 5388 |
| `.bss` | 502512 |
| `.bin` | 335 KiB |

That is the **linked image**, not Apollo510 latency, not MACs, not host ms.
`pred=` on SWO still needs `nsx flash` + `nsx view` on an EVB.

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY, wrap it:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure.log
```

## Model

`assets/depgraph_r060_kd_int8.tflite` SHA256
`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`.
helia-rt `cfab1523` (v1.19.0), ns-cmsis-nn `b386770a` (v7.31.0).

A Demo A student is usually **Class B**: see [`../bring_your_model.md`](../bring_your_model.md).
After a swap, `host/expected.json` is stale — LiteRT must use **that** SHA.
