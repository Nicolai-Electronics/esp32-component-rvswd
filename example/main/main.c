/*
 * SPDX-FileCopyrightText: 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "rvswd_ch32v20x.h"

static const char* TAG = "example";

extern uint8_t const coprocessor_firmware_start[] asm("_binary_coprocessor_bin_start");
extern uint8_t const coprocessor_firmware_end[] asm("_binary_coprocessor_bin_end");

void callback(char const* msg, uint8_t progress) {
    ESP_LOGI(TAG, "%s: %d%%", msg, progress);
}

static void flash_coprocessor(void) {
    rvswd_handle_t handle = {
        .swdio = 22,
        .swclk = 23,
    };

    ch32v20x_read_option_bytes(&handle);

    bool success = ch32v20x_program(&handle, coprocessor_firmware_start,
                                    coprocessor_firmware_end - coprocessor_firmware_start, callback);

    if (success) {
        ESP_LOGI(TAG, "Succesfully flashed the CH32V203 microcontroller");
    } else {
        ESP_LOGE(TAG, "Failed to flash the CH32V203 microcontroller");
    }

    // vTaskDelay(pdMS_TO_TICKS(5000));
    // esp_restart();
}

void app_main(void) {
    flash_coprocessor();
    return;
}
