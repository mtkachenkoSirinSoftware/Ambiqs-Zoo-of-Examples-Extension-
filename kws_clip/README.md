# kws_clip

Flash a known 1 s clip, run MFCC + heliaRT, print the class on **SWO**
(`nsx view`). Same debug transport as `neuralspotx/examples/kws_infer`.

This is not dummy-input `kws_infer` (zeros / PRNG). The PCM is `synthetic`
(16 kHz mono int16, 16000 samples). On that buffer the host interpreter
predicts **`go`** (tfds index 1). See [`../assets/LABELS.md`](../assets/LABELS.md).

Demonstrates:

- `nsx_system_init` (LP, cache, ITM, SpotMgr)
- heliaRT Invoke on a real MFCC tensor
- `NsxPmuProfiler` per-layer CSV (`NSX_PMU_PRESET_ML_DEFAULT`)

## Hardware

Apollo510 EVB + J-Link. No USB-UART, no MEMS.

## Build & Run

```bash
cd kws_clip
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Regenerate embedded arrays if missing (`make embed`, or CMake does it).

Host check (no board):

```bash
python3 host/test_clip_identity.py
```

## Expected output

```
kws_clip runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
clip=synthetic n=16000 sha256=341c766cf618
model sha256=ae08012b5a5d
labels=tfds index 1=go (not kws_infer MLPerf index 11=go); assets/LABELS.md
pred=go score=0.980 fired=0|1 invoke_cycles=<DWT> invoke_us=<CYCCNT/cpu_hz>
--- Per-Layer PMU ---
"Layer","Op","ARM_PMU_MVE_INST_RETIRED",...
dwt_cycles=<N> pmu_cycles=<N> inst_retired=<N>
```

| Line | Meaning |
|---|---|
| `pred=go` | last raw argmax on this clip (must match `host/expected.json`) |
| `fired=` | recognizer (smooth / threshold / debounce), not the raw argmax |
| `invoke_cycles=` / `invoke_us=` | DWT of the first `Invoke`; µs = CYCCNT ÷ printed `cpu_hz` |
| `--- Per-Layer PMU ---` | ML_DEFAULT CSV for that Invoke |
| `dwt_cycles=` / `pmu_cycles=` | DWT vs `ARM_PMU_CPU_CYCLES` on a **second** Invoke of the same tensor (`pmu_profiling` events). A mismatch is a fact, not a bug |

`score=` is softmax of int8 logits. The **name** `go` is the check.

## How it works

```c
NSX_TRY(nsx_system_init(&kCfg), "System init failed\n");
nsx_itm_printf_enable();
// Play 50 hops of 320 samples from flash, then FlushInference.
```

Tensor arena is `NSX_MEM_FAST_BSS` (TCM), `alignas(16)`, 32 KiB.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | init, play clip, SWO |
| `src/nsx_pmu_profiler.cc` | TFLM `MicroProfilerInterface` (same shape as `kws_infer`) |
| `src/flash_clip_source.c` | `AudioSource` over the flash player |
| `../kws_core/` | frontend, quantize, helia runner, recognizer |
| `host/expected.json` | interpreter identity for this PCM |
| `nsx.yml` / `nsx.lock` | helia-rt 1.19.0, ns-cmsis-nn 7.31.0 |

## Model

`assets/depgraph_r060_kd_int8.tflite`  
SHA256 `ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`

To swap a class-B INT8 file: [`../bring_your_model.md`](../bring_your_model.md).
After a swap, compare MCU vs interpreter on **that** SHA, not the shipping JSON.

If `nsx configure` has no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure.log
```
