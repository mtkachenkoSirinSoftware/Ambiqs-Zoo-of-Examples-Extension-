# SPDX-License-Identifier: Apache-2.0
.PHONY: embed test-host

EMBED_CLIP := ../tools/embed_clip.py
EMBED_MODEL := ../tools/embed_model.py
TFLITE := ../assets/depgraph_r060_kd_int8.tflite
GOLDEN := ../kws_core/tests/golden/synthetic.txt

embed: ## Write audio/generated + model/generated from golden PCM + shipping tflite
	python3 $(EMBED_CLIP) --golden $(GOLDEN) --out-dir audio/generated
	python3 $(EMBED_MODEL) $(TFLITE) --out-dir model/generated

test-host: ## Clip SHA vs golden vs expected.json (no EVB)
	python3 host/test_clip_identity.py
