# kws_pdm — EVB PDM MEMS → MFCC → heliaRT on Apollo510

Always-on KWS on a **live PDM microphone**. Labels print on **SWO**
(`nsx view`). This is not dummy-input `kws_infer`, not flash-clip GATE 3,
and not UART replay. Same graph, different acoustic identity.

Connect a digital microphone first — same note as AmbiqSuite `pdm_fft`.

## Official silicon bring-up (do this first)

Two official paths, then this image. Do not debug KWS MFCC while PDM itself is
silent.

1. **NSX** `neuralspotx/examples/audio_capture` — `nsx-audio`, 16 kHz, SWO
   `Frame N  peak=`. README there says the Apollo510 EVB has an **onboard**
   PDM MEMS. The AmbiqSuite EVB Quick Start / BSP do **not** list an onboard
   mic; PDM0 is GPIO **50 CLK / 51 DATA** on **VDDH2**. Treat their onboard
   claim as unverified here until their board file is checked. Peak on SWO is
   still the right preflight.
2. **Suite** `boards/apollo510_evb/examples/audio/pdm_rtt_stream` (or
   `pdm_fft`) — same pins, PLL 2.048 MHz CLK → 16 kHz, PCM on RTT ch1.

Then flash `kws_pdm`. Default backend is **`nsx-audio`** (hop 320, HFRC2_ADJ
16 kHz — [`host/nsx_audio_default.md`](host/nsx_audio_default.md)). That is
not the Suite PLL recipe. HAL PLL backend: `nsx build -- -DKWS_PDM_USE_NSX_AUDIO=OFF`.

Do not debug KWS MFCC while `audio_capture` / `pdm_rtt_stream` itself is silent.

## Hardware

| Signal | GPIO | BSP |
|---|---|---|
| PDM0 clock out | **50** | `AM_BSP_GPIO_PDM0_CLK_CB` |
| PDM0 data in | **51** | `AM_BSP_GPIO_PDM0_DATA_CB` |

The EVB Quick Start has **no onboard microphone** (BSP GPIO 50/51).
`audio_capture`'s onboard-mic sentence is their README, not a fact we re-measured.
Firmware requests **left** (`AM_HAL_PDM_CHANNEL_LEFT`). `nsx-audio` hard-codes
`bLRSwap=0`. MEMS L/R is part-specific: if SWO `peak=` stays ~0 with a live
mic, rebuild the **HAL** backend with `-DKWS_PDM_USE_NSX_AUDIO=OFF
-DKWS_PDM_LR_SWAP=1`.

**Not guessed:** exact MEMS part, which header carries 50/51, VDDH2 voltage
vs 3.3 V mic, extra passives.

## Clock

**Default (`nsx-audio`):** HFRC2_ADJ → 16 kHz (see
[`host/nsx_audio_default.md`](host/nsx_audio_default.md)). Hop = 320 samples
(20 ms), not `audio_capture`'s 480.

**HAL fallback** (`-DKWS_PDM_USE_NSX_AUDIO=OFF`) is the `pdm_fft` PLL recipe:

```
PDM_CLK_OUT   = 24.576 MHz / (1+1) / (5+1) = 2.048 MHz
SAMPLING_FREQ = 2.048 MHz / (64 * 2)       = 16 kHz
```

PCM16 on the HAL path = bits `[23:8]` of the 32-bit DMA word. Host tests cover
that packing — they are **not** a microphone measurement.

## Build

```bash
cd ambiq_kws_examples/kws_pdm
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Optional L/R swap (HAL backend only):

```bash
nsx build --app-dir . -- -DKWS_PDM_USE_NSX_AUDIO=OFF -DKWS_PDM_LR_SWAP=1
```

Host checks (no board, no mic):

```bash
make test-host
```

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-pdm.log
```

## Expected output (SWO)

```
kws_pdm runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
pdm backend=nsx-audio hop=320 fs=16000 clk=HFRC2_ADJ (not pdm_fft PLL)
gpio clk=50 data=51  (BSP; audio_capture README onboard-mic claim unverified here)
model sha256=ae08012b5a5d
pred is live PDM, not GATE 3, not a GSC clip, not hpx profile
Frame 50  peak=<n> dc=<n> overruns=0 dma_faults=0 derr=0
live pred=<label> score=<s> fired=0|1 invoke_cycles=<DWT> invoke_us=<us>
  invoke_us = CYCCNT / cpu_hz (see perf_mode=); not hpx profile
event label=<kw> score=<s>     # only when the recognizer fires
```

| Line | What it is |
|---|---|
| `peak=` / `dc=` | last 20 ms hop; `Frame N  peak=` matches `audio_capture` shape; not a KWS score |
| `live pred=` | last raw argmax on **this** PDM window; not LiteRT on a WAV |
| `fired=` / `event` | recognizer (smooth / threshold / debounce) |
| `invoke_cycles=` | DWT of **this** `Invoke`, not `hpx profile` |

1 Hz `live pred=` is expected; `event` is rarer. Do not quote either as GATE 3.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | `nsx_system_init`, drain PDM hops, SWO |
| `src/nsx_audio_source.c` | default `AudioSource` via `nsx-audio`, hop 320 |
| `src/apollo510_audio.c` | HAL PLL backend (`-DKWS_PDM_USE_NSX_AUDIO=OFF`) |
| `audio/pdm_pcm16.h` | PLL identity + 24-bit → PCM16 |
| `host/nsx_audio_default.md` | 2.2 source reading of `nsx_audio_pdm_default` |
| `host/expected.json` | pins + SHA; **no** `pred` field |
| `nsx.yml` / `nsx.lock` | helia-rt 1.19.0 + ns-cmsis-nn 7.31.0 pins |

## Model

`assets/depgraph_r060_kd_int8.tflite` SHA256
`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`.
A Demo A student is usually **Class B**: see [`../bring_your_model.md`](../bring_your_model.md).
PDM `live pred=` is still not GATE 3 after a swap.

## Linked image (host, not latency)

`nsx build` produces `build/apollo510_evb/kws_pdm` (ELF) and `.bin`. One
successful link on this tree:

| Segment | Bytes |
|---|---|
| `.text` | 291856 |
| `.data` | 2620 |
| `.bss` | 507840 |
| `.bin` | 288 KiB |

That is the **linked image**, not Apollo510 latency, not a microphone result.
`pred=` on SWO still needs a MEMS on GPIO 50/51, `nsx flash`, and `nsx view`.

