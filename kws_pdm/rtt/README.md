# SEGGER RTT (kws_pdm hop PCM)

Upstream: [SEGGERMicro/RTT](https://github.com/SEGGERMicro/RTT)
Release: **V8.58.0**
Commit: `4d8feab3150f86f37a9d323ddc88d6cdf5673072`
Copied from `third_party/helia-profiler/.../vendor/segger_rtt` (same pin).
License: [LICENSE.md](LICENSE.md)

## Channel split (same as Suite `pdm_rtt_stream`)

| Channel | Contents |
|---|---|
| **1** | Hop PCM: 16 kHz mono **int16**, 320 samples/hop. **Not a label.** |
| SWO / ITM | `pred=`, `Frame N peak=`, `live pred=` — labels stay here |

Channel 1 is hop PCM, not a label. Do not parse channel 0 as PCM.

Compile-time: `-DKWS_PDM_RTT_PCM=ON`. Default **OFF**. Control block stays in
`.bss` (TCM on Apollo510). The channel-1 ring is `NSX_MEM_SRAM_BSS`.
After each hop write the firmware `SCB_CleanDCache()` so J-Link SWD sees
WrOff (Cortex-M55 D-cache; same as NSX CoreMark).

Host drain: [`../host/rtt_pcm_dump.py`](../host/rtt_pcm_dump.py). That file
is **not** LiteRT and **not** GATE 3. Kill `nsx view` first (OB exclusive).
