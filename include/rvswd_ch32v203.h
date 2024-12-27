
/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rvswd.h"

// Optional user-defined status update callback.
void ch32_status_callback(char const* msg, int progress, int total);

// Program and restart the CH32V203.
bool ch32_program(rvswd_handle_t* handle, void const* firmware, size_t firmware_len);
