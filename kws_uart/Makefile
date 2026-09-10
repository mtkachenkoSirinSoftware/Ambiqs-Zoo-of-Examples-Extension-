# SPDX-License-Identifier: Apache-2.0
.PHONY: embed test-host

EMBED_MODEL := ../tools/embed_model.py
TFLITE := ../assets/depgraph_r060_kd_int8.tflite
GOLDEN := ../kws_core/tests/golden/synthetic.txt

embed: ## Identity WAV + embedded shipping tflite
	python3 host/make_identity_wav.py --golden $(GOLDEN) --out host/synthetic.wav
	python3 $(EMBED_MODEL) $(TFLITE) --out-dir model/generated

test-host: ## Framing, WAV identity, C++ decoder interop (no EVB)
	python3 host/test_uart_identity.py
	python3 host/test_stream_wav.py
	cmake -S tests -B build-host
	cmake --build build-host -j
	ctest --test-dir build-host --output-on-failure
