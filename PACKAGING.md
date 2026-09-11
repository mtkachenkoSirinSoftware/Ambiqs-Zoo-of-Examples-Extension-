# SPDX-License-Identifier: Apache-2.0
# Distribution tarball

```bash
make dist          # dist/ambiq_kws_examples.tar.gz
make verify-dist   # extract + inclusion / exclusion
make ci            # host tests, then dist + verify
```

`make ci-firmware` also `nsx build`s `kws_clip` and `kws_uart` (no flash).
Needs `nsx` on `PATH` and a TTY or the `script -q` wrapper in the README.

## In the archive

| Path | Why |
|---|---|
| `kws_core/` | shared frontend, helia runner, recognizer |
| `kws_clip/` | 1 s PCM in flash |
| `kws_uart/` | host WAV over FIFO-poll UART |
| `kws_pdm/` | EVB PDM MEMS |
| `assets/depgraph_r060_kd_int8.tflite` | shipping INT8 graph |
| `kws_uart/host/synthetic.wav` | identity WAV (same PCM as the flash clip) |
| `kws_uart/host/stream_wav.py` | `--list-ports`, `--dtr auto` |
| `kws_uart/host/rpc_infer.py` | USB RPC INFER of 16000 int16 |
| `tools/embed_model.py` | class A/B/C `--check` + C-array emit |
| `README.md`, `LICENSE`, `COVER.md`, this file | first-read + license |

Generated C arrays (`model/generated/`, clip PCM) and `nsx.lock` ship so
`nsx build` works without re-running embed.

## Not in the archive

AmbiqSuite SDK (NSX pulls `nsx-ambiqsuite`), venvs, `modules/`, `cmake/nsx/`,
`boards/`, `build/`, `kws_model_manifest.json` (absolute paths),
`host/generated/`.

## Host CI (no EVB)

`make test` runs GoogleTest on `kws_core`, Python identity on clip/uart/pdm,
UART framing + C++ decoder interop, and `embed_model.py --check` on the
shipping `.tflite`. TensorFlow is not required for those checks.

Firmware link (`nsx build`) is optional. Flash + SWO `pred=` needs an EVB.
