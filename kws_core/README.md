# kws_core

Portable always-on KWS pipeline: PCM hop → MFCC → int8 → ModelRunner →
recognizer. No UART HAL, no PDM, no NSX bootstrap.

Host tests (golden frontend + flash-clip player, host-stub runtime):

```bash
cmake -S kws_core -B kws_core/build
cmake --build kws_core/build -j
ctest --test-dir kws_core/build --output-on-failure
```

The NSX `kws_clip` / `kws_uart` / `kws_pdm` images compile these same sources
(`cmake/kws_core.cmake`). Cycle counters on the host are **zero** by design
(`profiling/kws_profile_host.cc`) — not an Apollo510 measurement.
