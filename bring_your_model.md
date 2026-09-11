# SPDX-License-Identifier: Apache-2.0
# Bring your INT8 student (phase 5)

Demo A / pruneopt stays **outside** this tree. Here you only swap a finished
`.tflite`. Do not patch `kws_model_data.cc` by hand.

```bash
cd ambiq_kws_examples
python3 tools/embed_model.py path/to/student_int8.tflite --check
# exit 0 → A or B; exit 2 → C (stop) or U (unstructured sidecar)
```

| Class | Meaning | Action |
|---|---|---|
| **A** | geometry + quant identity = shipping SHA `ae08012b…` | flash the default image |
| **B** | `[1,49,10,1]→[1,12]` int8, different scale/SHA | embed + `nsx build` / flash |
| **C** | wrong shape, dtype, class count, per-axis I/O quant | **stop** — firmware work, not a swap |

Keras cascade graphs from Demo A are almost always Class B. Re-embed; `kws_config.h`
reads scales from the generated `kws_model_contract.h`.

Frontend must stay `tf.signal` MFCC / `mfcc_tf` (30/20 ms). A student trained
on `microfrontend` is a different identity — do not compare its accuracy to
these examples without saying so.

## Embed into an example

```bash
# after --check is A or B:
python3 tools/embed_model.py path/to/student_int8.tflite \
    --out-dir kws_uart/model/generated
# same file into kws_clip and kws_pdm if those images should match
cd kws_uart
nsx build --app-dir .
nsx flash --app-dir .
```

`--out-dir kws_clip/model/generated` for the flash-clip image.
helia-rt is read from `kws_clip/modules/helia-rt` after `nsx lock`, or pass
`--helia-rt`.

## Embed into neuralspotx `kws_infer` (Class B)

Do not hand-edit `neuralspotx/examples/kws_infer/src/kws_model_data.h`.
Refresh it with the same script:

```bash
python3 tools/embed_model.py path/to/student_int8.tflite --check
python3 tools/embed_model.py path/to/student_int8.tflite \
    --kws-infer-header path/to/neuralspotx/examples/kws_infer/src/kws_model_data.h
```

That header is their C array (`kws_model_data` / `kws_model_data_len`). It does
**not** rewrite `kLabels[]` in their `main.cc`. That table stays **classic
MLPerf** (`go` = index **11**) until they edit it. This zoo prints **tfds**
order (`go` = index **1**). Same 12-way INT8, different name-at-index — see
[`assets/LABELS.md`](assets/LABELS.md). Dummy-input `kws_infer` still does not
run this PCM and is not GATE 3.

## UART / clip vs LiteRT (Class B)

`kws_uart/host/expected.json` and `kws_clip/host/expected.json` are **shipping**
identity (`ae08012b…`, `pred=go` on `synthetic`). After a Class B embed they
are stale.

Compare last **raw argmax** on **identical PCM** against LiteRT loaded with
**this** student SHA — not against the shipping JSON. Same WAV, no host
peak-norm. MCU labels are SWO (`nsx view`), not UART TX.

PDM `live pred=` is still not GATE 3 after a model swap.

## Not in this drop

Prune notebooks, Optuna, SNR, ALSA host-mic, unstructured / Tucker sidecars
(Class U unless `--allow-unstructured`).
