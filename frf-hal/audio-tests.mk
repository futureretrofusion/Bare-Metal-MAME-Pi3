# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Host-only SPSC audio stress test. Include after Makefile:
# make -C frf-hal -f Makefile -f audio-tests.mk host-test-audio-threads
.PHONY: host-test-audio-threads
host-test-audio-threads: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -pthread -o $(BUILD)/host-test-audio-threads tests/test_audio_threads.c audio_queue.c
	$(BUILD)/host-test-audio-threads
