# Optional builds

CMake flags on the three apps. Not extra directories. This `nsx` has no
CMake-arg passthrough — set the cache, then `nsx build`.

```bash
script -q -e -c 'cmake -D<FLAG>=ON build/apollo510_evb' /tmp/cmake-flag.log
nsx build --app-dir .
```

## `kws_clip`

Per-layer PMU CSV is **on** (`KWS_CLIP_PMU`). After each clip it prints
`NSX_PMU_PRESET_ML_DEFAULT`, then `dwt_cycles=` / `pmu_cycles=` /
`inst_retired=` on a second Invoke of the same tensor (`pmu_profiling`
events). GPIO / Joulescope markers are not wired.

## `kws_uart`

| Flag | Default | What it does |
|---|---|---|
| *(none)* | PRINT UART @ 921600, FIFO poll, DTR low | PCM on COM/PRINT; labels on SWO |
| `KWS_UART_USB_CDC=ON` | off | Same `0xA51C` frames on nsx-usb CDC (`0xCafe`/`0x4011`, DTR=True) |
| `KWS_UART_USB_RPC=ON` | off | `usb_rpc` framing; INFER = 16000 int16 → `KwsApp` |

CDC and RPC are mutually exclusive. CDC/RPC need the EVB **device** USB, not
only J-Link.

## `kws_pdm`

| Flag | Default | What it does |
|---|---|---|
| *(none)* | `nsx-audio`, hop 320, HFRC2_ADJ 16 kHz | Live PDM |
| `KWS_PDM_USE_NSX_AUDIO=OFF` | on | Suite `pdm_fft` PLL HAL |
| `KWS_PDM_LR_SWAP=1` | off | HAL only |
| `KWS_PDM_RTT_PCM=ON` | off | Hop PCM on RTT channel 1; labels stay on SWO |

## Related NSX examples

`hello_world` (ITM), `kws_infer` (heliaRT + PMU, dummy tensor),
`audio_capture` (`nsx-audio`, 480-sample frames), `pmu_profiling`,
`usb_serial`, `usb_rpc`. Suite preflight for PDM: `pdm_fft`, `pdm_rtt_stream`.
