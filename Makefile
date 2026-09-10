# SPDX-License-Identifier: Apache-2.0
.PHONY: help test test-core test-clip test-uart test-pdm test-tools test-pack \
        check dist verify-dist ci ci-firmware

MODEL ?= assets/depgraph_r060_kd_int8.tflite
ARCHIVE ?= dist/ambiq_kws_examples.tar.gz

help: ## Host targets (no EVB)
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | \
	 awk 'BEGIN{FS=":.*?## "}{printf "  %-16s %s\n", $$1, $$2}'

test-core: ## Host GoogleTest for kws_core (no EVB)
	cmake -S kws_core -B kws_core/build
	cmake --build kws_core/build -j
	ctest --test-dir kws_core/build --output-on-failure

test-clip: ## kws_clip PCM/model identity vs host/expected.json
	$(MAKE) -C kws_clip test-host

test-uart: ## kws_uart framing + WAV identity + C++ decoder (no EVB)
	$(MAKE) -C kws_uart test-host

test-pdm: ## kws_pdm PLL recipe + PCM16 packing (no EVB, not a mic measurement)
	$(MAKE) -C kws_pdm test-host

test-tools: ## Class A/B/C --check (Class B embed if a student .tflite is present)
	python3 tools/test_embed_check.py

test-pack: ## Tarball include/exclude (no EVB)
	python3 tools/test_pack_tarball.py

test: test-core test-clip test-uart test-pdm test-tools test-pack ## All host tests

check: ## Classify MODEL (default: shipping assets .tflite)
	python3 tools/embed_model.py $(MODEL) --check

dist: ## Submission tarball under dist/
	python3 tools/pack_tarball.py --archive $(ARCHIVE)

verify-dist: dist ## Pack then check inclusion / exclusion / shipping SHA
	python3 tools/pack_tarball.py --verify-only --archive $(ARCHIVE)

ci: test verify-dist ## Host CI: tests + tarball (no nsx, no flash)

ci-firmware: ## nsx build kws_clip and kws_uart (no flash). Needs nsx on PATH.
	@command -v nsx >/dev/null || { echo "ci-firmware: nsx not on PATH"; exit 1; }
	cd kws_clip && nsx lock --app-dir . && \
	  (nsx configure --app-dir . || script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-clip.log) && \
	  nsx build --app-dir .
	cd kws_uart && nsx lock --app-dir . && \
	  (nsx configure --app-dir . || script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-uart.log) && \
	  nsx build --app-dir .
