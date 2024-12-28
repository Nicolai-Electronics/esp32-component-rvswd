
/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include "rvswd.h"

typedef void (*ch32v20x_status_callback)(char const* msg, uint8_t progress);

// Option bytes
bool ch32v20x_read_option_bytes(rvswd_handle_t* handle);

// Program and restart the CH32V203
bool ch32v20x_program(rvswd_handle_t* handle, void const* firmware, size_t firmware_len,
                      ch32v20x_status_callback status_callback);

rvswd_result_t ch32v20x_halt_microprocessor(rvswd_handle_t* handle);
rvswd_result_t ch32v20x_resume_microprocessor(rvswd_handle_t* handle);
rvswd_result_t ch32v20x_reset_microprocessor_and_run(rvswd_handle_t* handle);
bool ch32v20x_write_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t value);
bool ch32v20x_read_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t* value_out);
bool ch32v20x_run_debug_code(rvswd_handle_t* handle, void const* code, size_t code_size);
bool ch32v20x_read_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t* value_out);
bool ch32v20x_write_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t value);
bool ch32v20x_wait_flash(rvswd_handle_t* handle);
void ch32v20x_wait_flash_write(rvswd_handle_t* handle);
bool ch32v20x_unlock_flash(rvswd_handle_t* handle);
bool ch32v20x_lock_flash(rvswd_handle_t* handle);
bool ch32v20x_erase_flash_block(rvswd_handle_t* handle, uint32_t addr);
bool ch32v20x_write_flash_block(rvswd_handle_t* handle, uint32_t addr, void const* data);
bool ch32v20x_write_flash(rvswd_handle_t* handle, uint32_t addr, void const* _data, size_t data_len,
                          ch32v20x_status_callback status_callback);
bool ch32v20x_clear_running_operations(rvswd_handle_t* handle);
bool ch32v20x_read_option_bytes(rvswd_handle_t* handle);
