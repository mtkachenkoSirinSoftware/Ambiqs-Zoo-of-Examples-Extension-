# SPDX-License-Identifier: Apache-2.0
# Swap an INT8 `.tflite`

Do not patch `kws_model_data.cc` by hand.

```bash
python3 tools/embed_model.py path/to/model.tflite --check
# exit 0 → A or B; exit 2 → C (stop) or U (unstructured sidecar)
```

| Class | Meaning | Action |
|---|---|---|
| **A** | geometry + quant identity = shipping SHA `ae08012b…` | flash the default image |
| **B** | `[1,49,10,1]→[1,12]` int8, different scale/SHA | embed + `nsx build` / flash |
| **C** | wrong shape, dtype, class count, or per-axis I/O quant | **stop** |

`kws_config.h` reads scales from the generated `kws_model_contract.h`.

Frontend must stay `tf.signal` MFCC (30/20 ms). A model trained on
`microfrontend` is a different identity — do not compare accuracy without
saying so.

This graph uses **Pad** and has no in-graph Softmax. Stock `kws_infer`
registers Softmax and not Pad — dropping the C array into that example is
not enough; their resolver and `kLabels[]` stay MLPerf.

## Embed into an example

```bash
python3 tools/embed_model.py path/to/model.tflite \
    --out-dir kws_uart/model/generated
# same file into kws_clip and kws_pdm if those images should match
cd kws_uart
nsx build --app-dir .
nsx flash --app-dir .
```

`--out-dir kws_clip/model/generated` for the flash-clip image. helia-rt is
read from `kws_clip/modules/helia-rt` after `nsx lock`, or pass `--helia-rt`.

## Embed into `kws_infer`

Do not hand-edit `neuralspotx/examples/kws_infer/src/kws_model_data.h`:

```bash
python3 tools/embed_model.py path/to/model.tflite --check
python3 tools/embed_model.py path/to/model.tflite \
    --kws-infer-header path/to/neuralspotx/examples/kws_infer/src/kws_model_data.h
```

That writes their C array only. It does **not** add `AddPad()`, and it does
**not** rewrite `kLabels[]` (`go` = index **11** there, index **1** here).
[`assets/LABELS.md`](assets/LABELS.md). Dummy-input `kws_infer` still does
not run this PCM.

## After class B

`kws_uart/host/expected.json` and `kws_clip/host/expected.json` are the
**shipping** identity (`ae08012b…`, `pred=go` on `synthetic`). Compare last
raw argmax on **identical PCM** against an interpreter loaded with **this**
SHA. No host peak-norm. Labels on SWO, not UART TX.

PDM `live pred=` is still the microphone after a swap.
