# SPDX-License-Identifier: Apache-2.0
.PHONY: embed test-host

EMBED_MODEL := ../tools/embed_model.py
TFLITE := ../assets/depgraph_r060_kd_int8.tflite

embed: ## Embedded shipping tflite
	python3 $(EMBED_MODEL) $(TFLITE) --out-dir model/generated

test-host: ## PLL recipe + PCM16 packing + notes (no EVB, not a mic measurement)
	python3 host/test_pdm_notes.py
	cmake -S tests -B build-host
	cmake --build build-host -j
	ctest --test-dir build-host --output-on-failure
