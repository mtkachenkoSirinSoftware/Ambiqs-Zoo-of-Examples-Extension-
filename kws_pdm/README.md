# kws_pdm

Always-on KWS on a **live PDM microphone**. Labels on **SWO** (`nsx view`).
Same graph as `kws_clip` / `kws_uart`, different PCM origin — do not compare
`live pred=` to the flash/UART clip.

Connect a digital mic first (AmbiqSuite `pdm_fft` note).

Demonstrates:

- `nsx-audio` PDM (`nsx_audio_pdm_default`, hop **320** @ 16 kHz)
- `Frame N  peak=` in the `audio_capture` shape
- Optional RTT channel-1 hop dump (`pdm_rtt_stream` split)

## Hardware

| Signal | GPIO | BSP |
|---|---|---|
| PDM0 clock out | **50** | `AM_BSP_GPIO_PDM0_CLK_CB` |
| PDM0 data in | **51** | `AM_BSP_GPIO_PDM0_DATA_CB` |

Clock/data sit on **VDDH2**. The MEMS part, header, and rail vs 3.3 V are
not guessed here.

The Apollo510 EVB Quick Start does not list an onboard mic. `audio_capture`
README says onboard MEMS — treat that as their README, not a re-measured
board fact. Firmware requests **left**. `nsx-audio` hard-codes `bLRSwap=0`.
If `peak=` stays ~0 with a live mic, rebuild the **HAL** backend:

```bash
script -q -e -c 'cmake -DKWS_PDM_USE_NSX_AUDIO=OFF -DKWS_PDM_LR_SWAP=1 build/apollo510_evb' /tmp/cmake-pdm-lr.log
nsx build --app-dir .
```

## Bring-up (do this first)

Do not debug MFCC while PDM itself is silent.

1. NSX `audio_capture` — SWO `Frame N  peak=` at 16 kHz / 480 samples/frame.
2. Suite `pdm_rtt_stream` or `pdm_fft` — GPIO 50/51, PLL 2.048 MHz → 16 kHz.

Default `kws_pdm` backend is **`nsx-audio`** (HFRC2_ADJ → **exactly** 16 kHz,
hop 320). That is not the Suite PLL recipe and not `audio_capture`’s 480-sample
frame. Clock reading: [`host/nsx_audio_default.md`](host/nsx_audio_default.md).

HAL PLL (Suite `pdm_fft` recipe):

```bash
script -q -e -c 'cmake -DKWS_PDM_USE_NSX_AUDIO=OFF build/apollo510_evb' /tmp/cmake-pdm-hal.log
nsx build --app-dir .
```

```
PDM_CLK_OUT   = 24.576 MHz / (1+1) / (5+1) = 2.048 MHz
SAMPLING_FREQ = 2.048 MHz / (64 * 2)       = 16 kHz
```

## Build & Run

```bash
cd kws_pdm
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Host check (no mic): `make test-host`

## Expected output

```
kws_pdm runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
pdm backend=nsx-audio hop=320 fs=16000 clk=HFRC2_ADJ
gpio clk=50 data=51
model sha256=ae08012b5a5d
Frame 50  peak=<n> dc=<n> overruns=0 dma_faults=0 derr=0
live pred=<label> score=<s> fired=0|1 invoke_cycles=<DWT> invoke_us=<us>
```

| Line | Meaning |
|---|---|
| `peak=` / `dc=` | last 20 ms hop; not a KWS score |
| `live pred=` | last raw argmax on this PDM window |
| `fired=` / `event` | recognizer |
| `invoke_cycles=` | DWT of this `Invoke` |

~1 Hz `live pred=` is expected; `event` is rarer.

## Optional: RTT channel 1 (PCM dump)

Same split as Suite `pdm_rtt_stream`: ch1 = hop PCM, labels on SWO. Default
**OFF**.

```bash
script -q -e -c 'cmake -DKWS_PDM_RTT_PCM=ON build/apollo510_evb' /tmp/cmake-pdm-rtt.log
nsx build --app-dir . && nsx flash --app-dir .
# J-Link OB is exclusive — stop nsx view first
python3 host/rtt_pcm_dump.py --duration 3 --out /tmp/pdm.wav
```

16 kHz mono int16. Do not score that file as the flash/UART identity.
[`rtt/README.md`](rtt/README.md).

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | init, drain hops, SWO |
| `src/nsx_audio_source.c` | default `AudioSource`, hop 320 |
| `src/apollo510_audio.c` | HAL PLL (`-DKWS_PDM_USE_NSX_AUDIO=OFF`) |
| `src/kws_pdm_rtt.c` | RTT ch1 (`-DKWS_PDM_RTT_PCM=ON`) |
| `host/rtt_pcm_dump.py` | drain ch1 → `.wav` / `.pcm` |
| `host/expected.json` | pins + SHA; **no** `pred` field |

## Model

Same shipping INT8 as `kws_clip`. Swap: [`../bring_your_model.md`](../bring_your_model.md).
PDM `live pred=` is still the microphone after a swap.

If `nsx configure` has no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-pdm.log
```
