# kws_pdm — EVB PDM MEMS → MFCC → heliaRT on Apollo510

Always-on KWS on a **live PDM microphone**. Labels print on **SWO**
(`nsx view`). This is not dummy-input `kws_infer`, not flash-clip GATE 3,
and not UART replay. Same graph, different acoustic identity.

Connect a digital microphone first — same note as AmbiqSuite `pdm_fft`.

## Official silicon bring-up (do this first)

On the EVB, **before** this image:

1. Build/flash AmbiqSuite `boards/apollo510_evb/examples/audio/pdm_rtt_stream`
   (or `pdm_fft`). Confirm CLK on GPIO 50 and live PCM / FFT with the MEMS.
2. Then flash `kws_pdm` (same pins, same PLL recipe).

Do not debug KWS MFCC while `pdm_rtt_stream` itself is silent.

## Hardware

| Signal | GPIO | BSP |
|---|---|---|
| PDM0 clock out | **50** | `AM_BSP_GPIO_PDM0_CLK_CB` |
| PDM0 data in | **51** | `AM_BSP_GPIO_PDM0_DATA_CB` |

The EVB has **no onboard microphone**. GPIO 50/51 are on the **VDDH2** domain.
Firmware requests **left** (`AM_HAL_PDM_CHANNEL_LEFT`), `KWS_PDM_LR_SWAP=0`
(datasheet default: left = CLK rising). MEMS L/R is part-specific: if SWO
`peak=` stays ~0 with a live mic, rebuild with `-DKWS_PDM_LR_SWAP=1`.

**Not guessed:** exact MEMS part, which header carries 50/51, VDDH2 voltage
vs 3.3 V mic, extra passives.

## Clock (pdm_fft PLL recipe)

```
PDM_CLK_OUT   = 24.576 MHz / (1+1) / (5+1) = 2.048 MHz
SAMPLING_FREQ = 2.048 MHz / (64 * 2)       = 16 kHz
```

PCM16 = bits `[23:8]` of the 32-bit DMA word. Host tests cover that packing
and the PLL arithmetic — they are **not** a microphone measurement.

## Build

```bash
cd ambiq_kws_examples/kws_pdm
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Optional L/R swap:

```bash
nsx build --app-dir . -- -DKWS_PDM_LR_SWAP=1
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
pdm clk_out=2048000 fs=16000 osr=64 gpio clk=50 data=51 lr_swap=0
model sha256=ae08012b5a5d
pred is live PDM, not GATE 3, not a GSC clip, not hpx profile
pdm peak=<n> dc=<n> overruns=0 dma_faults=0 derr=0
live pred=<label> score=<s> fired=0|1 invoke_cycles=<DWT>
  invoke_cycles is DWT CYCCNT of this binary's Invoke, not hpx profile
event label=<kw> score=<s>     # only when the recognizer fires
```

| Line | What it is |
|---|---|
| `peak=` / `dc=` | last 20 ms hop; ~0 peak with a live talker means dead channel |
| `live pred=` | last raw argmax on **this** PDM window; not LiteRT on a WAV |
| `fired=` / `event` | recognizer (smooth / threshold / debounce) |
| `invoke_cycles=` | DWT of **this** `Invoke`, not `hpx profile` |

1 Hz `live pred=` is expected; `event` is rarer. Do not quote either as GATE 3.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | `nsx_system_init`, drain PDM hops, SWO |
| `src/apollo510_audio.c` | ping-pong DMA `AudioSource`; `am_pdm0_isr` |
| `audio/pdm_pcm16.h` | PLL identity + 24-bit → PCM16 |
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
| `.text` | 291304 |
| `.data` | 5220 |
| `.bss` | 505248 |
| `.bin` | 290 KiB |

That is the **linked image**, not Apollo510 latency, not a microphone result.
`pred=` on SWO still needs a MEMS on GPIO 50/51, `nsx flash`, and `nsx view`.

