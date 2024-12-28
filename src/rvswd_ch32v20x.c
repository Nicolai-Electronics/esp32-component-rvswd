
/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include "rvswd_ch32v20x.h"
#include "ch32v20x_registers.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "string.h"

static char const TAG[] = "CH32V20X";

#define CH32_REG_DEBUG_DATA0        0x04  // Data register 0, can be used for temporary storage of data
#define CH32_REG_DEBUG_DATA1        0x05  // Data register 1, can be used for temporary storage of data
#define CH32_REG_DEBUG_DMCONTROL    0x10  // Debug module control register
#define CH32_REG_DEBUG_DMSTATUS     0x11  // Debug module status register
#define CH32_REG_DEBUG_HARTINFO     0x12  // Microprocessor status register
#define CH32_REG_DEBUG_ABSTRACTCS   0x16  // Abstract command status register
#define CH32_REG_DEBUG_COMMAND      0x17  // Astract command register
#define CH32_REG_DEBUG_ABSTRACTAUTO 0x18  // Abstract command auto-executtion
#define CH32_REG_DEBUG_PROGBUF0     0x20  // Instruction cache register 0
#define CH32_REG_DEBUG_PROGBUF1     0x21  // Instruction cache register 1
#define CH32_REG_DEBUG_PROGBUF2     0x22  // Instruction cache register 2
#define CH32_REG_DEBUG_PROGBUF3     0x23  // Instruction cache register 3
#define CH32_REG_DEBUG_PROGBUF4     0x24  // Instruction cache register 4
#define CH32_REG_DEBUG_PROGBUF5     0x25  // Instruction cache register 5
#define CH32_REG_DEBUG_PROGBUF6     0x26  // Instruction cache register 6
#define CH32_REG_DEBUG_PROGBUF7     0x27  // Instruction cache register 7
#define CH32_REG_DEBUG_HALTSUM0     0x40  // Halt status register
#define CH32_REG_DEBUG_CPBR         0x7C  // Capability register
#define CH32_REG_DEBUG_CFGR         0x7D  // Configuration register
#define CH32_REG_DEBUG_SHDWCFGR     0x7E  // Shadow configuration register

#define CH32_REGS_CSR 0x0000  // Offsets for accessing CSRs.
#define CH32_REGS_GPR 0x1000  // Offsets for accessing general-purpose (x)registers.

#define CH32_CFGR_KEY   0x5aa50000
#define CH32_CFGR_OUTEN (1 << 10)

#define CH32_CODE_BEGIN 0x08000000  // The start of CH32 CODE Flash region
#define CH32_CODE_END   0x08004000  // the end of the CH32 CODE Flash region

#define CH32V20X_FLASH_STATR 0x4002200C  // Flash status register
#define CH32V20X_FLASH_CTLR  0x40022010  // Flash configuration register
#define CH32_FLASH_ADDR      0x40022014  // Flash address register

static uint8_t const ch32v20x_readmem[] = {0x88, 0x41, 0x02, 0x90};
static uint8_t const ch32v20x_writemem[] = {0x88, 0xc1, 0x02, 0x90};

rvswd_result_t ch32v20x_halt_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request

    // Get the debug module status information, check rdata[9:8], if the value is 0b11,
    // it means the processor enters the halt state normally. Otherwise try again.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 8) & 0b11) == 0b11) {  // Check that processor has entered halted state
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to halt microprocessor, DMSTATUS=%" PRIx32, value);
            return false;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    ESP_LOGI(TAG, "Microprocessor halted");
    return RVSWD_OK;
}

rvswd_result_t ch32v20x_resume_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x40000001);  // Initiate a resume request

    // Get the debug module status information, check rdata[17:16],
    // if the value is 0b11, it means the processor has recovered.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if ((((value >> 10) & 0b11) == 0b11)) {
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to resume microprocessor, DMSTATUS=%" PRIx32, value);
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return RVSWD_OK;
}

rvswd_result_t ch32v20x_reset_microprocessor_and_run(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request

    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 18) & 0b11) == 0b11) {  // Check that processor has been reset
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to reset microprocessor");
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset request
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x10000001);  // Clear the core reset status signal
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset status signal clear request
    vTaskDelay(pdMS_TO_TICKS(10));

    return RVSWD_OK;
}

bool ch32v20x_write_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t value) {
    uint32_t command = regno         // Register to access.
                       | (1 << 16)   // Write access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_DATA0, value);
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    return true;
}

bool ch32v20x_read_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t* value_out) {
    uint32_t command = regno         // Register to access.
                       | (0 << 16)   // Read access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    rvswd_read(handle, CH32_REG_DEBUG_DATA0, value_out);
    return true;
}

bool ch32v20x_run_debug_code(rvswd_handle_t* handle, void const* code, size_t code_size) {
    if (code_size > 8 * 4) {
        ESP_LOGE(TAG, "Debug program is too long (%zd/%zd)", code_size, (size_t)8 * 4);
        return false;
    } else if (code_size & 1) {
        ESP_LOGE(TAG, "Debug program size must be a multiple of 2 (%zd)", code_size);
        return false;
    }

    // Copy into program buffer.
    uint32_t tmp[8] = {0};
    memcpy(tmp, code, code_size);
    for (size_t i = 0; i < 8; i++) {
        rvswd_write(handle, CH32_REG_DEBUG_PROGBUF0 + i, tmp[i]);
    }

    // Run program buffer.
    uint32_t command = (0 << 17)     // Do not perform transfer.
                       | (1 << 18)   // Run program buffer afterwards.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);

    return true;
}

bool ch32v20x_read_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t* value_out) {
    ch32v20x_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32v20x_run_debug_code(handle, ch32v20x_readmem, sizeof(ch32v20x_readmem));
    ch32v20x_read_cpu_reg(handle, CH32_REGS_GPR + 10, value_out);
    return true;
}

bool ch32v20x_write_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t value) {
    ch32v20x_write_cpu_reg(handle, CH32_REGS_GPR + 10, value);
    ch32v20x_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32v20x_run_debug_code(handle, ch32v20x_writemem, sizeof(ch32v20x_writemem));
    return true;
}

// Wait for the Flash chip to finish its current operation.
bool ch32v20x_wait_flash(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_STATR, &value);

    uint32_t timeout = 1000;

    while (value & CH32V20X_FLASH_STATR_BSY) {
        ESP_LOGD(TAG, "Flash busy: FLASH_STATR = 0x%08" PRIx32 "\r\n", value);
        vTaskDelay(1);
        ch32v20x_read_memory_word(handle, CH32V20X_FLASH_STATR, &value);
        timeout--;
        if (timeout == 0) {
            return false;
        }
    }
    return true;
}

void ch32v20x_wait_flash_write(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_STATR, &value);
    while (value & CH32V20X_FLASH_STATR_WRBUSY) {
        ch32v20x_read_memory_word(handle, CH32V20X_FLASH_STATR, &value);
    }
}

// Unlock the Flash if not already unlocked.
bool ch32v20x_unlock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_CTLR, &ctlr);

    // Enter the unlock keys.
    ch32v20x_write_memory_word(handle, 0x40022004, 0x45670123);
    ch32v20x_write_memory_word(handle, 0x40022004, 0xCDEF89AB);
    ch32v20x_write_memory_word(handle, 0x40022008, 0x45670123);
    ch32v20x_write_memory_word(handle, 0x40022008, 0xCDEF89AB);
    ch32v20x_write_memory_word(handle, 0x40022024, 0x45670123);
    ch32v20x_write_memory_word(handle, 0x40022024, 0xCDEF89AB);

    // Check again if Flash is unlocked.
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_CTLR, &ctlr);

    return ((ctlr & CH32V20X_FLASH_CTLR_LOCK) != CH32V20X_FLASH_CTLR_LOCK);
}

// Lock the FLASH
bool ch32v20x_lock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;

    // Check if Flash is locked
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_CTLR, &ctlr);

    if ((ctlr & CH32V20X_FLASH_CTLR_LOCK) == CH32V20X_FLASH_CTLR_LOCK) {
        ESP_LOGW(TAG, "Target Flash already locked");
        return true;
    }

    // Lock FLASH
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, ctlr | CH32V20X_FLASH_CTLR_LOCK);

    // Check again if Flash is locked
    ch32v20x_read_memory_word(handle, CH32V20X_FLASH_CTLR, &ctlr);

    return ((ctlr & CH32V20X_FLASH_CTLR_LOCK) == CH32V20X_FLASH_CTLR_LOCK);
}

// If unlocked: Erase a 256-byte block of FLASH.
bool ch32v20x_erase_flash_block(rvswd_handle_t* handle, uint32_t addr) {
    if (addr % 256) return false;
    bool wait_res = ch32v20x_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, CH32V20X_FLASH_CTLR_FTER);
    ch32v20x_write_memory_word(handle, CH32_FLASH_ADDR, addr);
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, CH32V20X_FLASH_CTLR_FTER | CH32V20X_FLASH_CTLR_STRT);
    wait_res = ch32v20x_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, 0);
    return true;
}

// If unlocked: Write a 256-byte block of FLASH.
bool ch32v20x_write_flash_block(rvswd_handle_t* handle, uint32_t addr, void const* data) {
    if (addr % 256) return false;

    bool wait_res = ch32v20x_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, CH32V20X_FLASH_CTLR_FTPG);

    ch32v20x_write_memory_word(handle, CH32_FLASH_ADDR, addr);

    uint32_t wdata[64];
    memcpy(wdata, data, sizeof(wdata));
    for (size_t i = 0; i < 64; i++) {
        ch32v20x_write_memory_word(handle, addr + i * 4, wdata[i]);
        ch32v20x_wait_flash_write(handle);
    }

    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, CH32V20X_FLASH_CTLR_FTPG | CH32V20X_FLASH_CTLR_PGSTRT);
    wait_res = ch32v20x_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32v20x_write_memory_word(handle, CH32V20X_FLASH_CTLR, 0);
    vTaskDelay(1);

    uint32_t rdata[64];
    for (size_t i = 0; i < 64; i++) {
        vTaskDelay(0);
        ch32v20x_read_memory_word(handle, addr + i * 4, &rdata[i]);
    }
    if (memcmp(wdata, rdata, sizeof(wdata))) {
        ESP_LOGE(TAG, "Write block mismatch at %08" PRIx32, addr);
        ESP_LOGE(TAG, "Write:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, wdata[i]);
        }
        ESP_LOGE(TAG, "Read:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, rdata[i]);
        }
        return false;
    }

    return true;
}

// If unlocked: Erase and write a range of Flash memory.
bool ch32v20x_write_flash(rvswd_handle_t* handle, uint32_t addr, void const* _data, size_t data_len,
                          ch32v20x_status_callback status_callback) {
    if (addr % 64) {
        return false;
    }

    uint8_t const* data = _data;

    char buffer[32];

    for (size_t i = 0; i < data_len; i += 256) {
        vTaskDelay(0);
        snprintf(buffer, sizeof(buffer) - 1, "Writing at 0x%08" PRIx32, addr + i);
        if (status_callback) {
            status_callback(buffer, i * 100 / data_len);
        }

        if (!ch32v20x_erase_flash_block(handle, addr + i)) {
            ESP_LOGE(TAG, "Error: Failed to erase Flash at %08" PRIx32, addr + i);
            return false;
        }

        if (!ch32v20x_write_flash_block(handle, addr + i, data + i)) {
            ESP_LOGE(TAG, "Error: Failed to write Flash at %08" PRIx32, addr + i);
            return false;
        }
    }

    return true;
}

bool ch32v20x_clear_running_operations(rvswd_handle_t* handle) {
    uint32_t timeout = 100;
    while (1) {
        uint32_t value = 0;
        ch32v20x_read_memory_word(handle, CH32V20X_FLASH_STATR, &value);
        if (value & CH32V20X_FLASH_STATR_BSY) {
            if (value & CH32V20X_FLASH_STATR_EOP) {
                ESP_LOGD(TAG, "Clearing EOP flag...\r\n");
                ch32v20x_write_memory_word(handle, CH32V20X_FLASH_STATR, value | CH32V20X_FLASH_STATR_EOP);
            } else if (value & CH32V20X_FLASH_STATR_WRPRTERR) {
                ESP_LOGD(TAG, "Clearing WRPRTERR flag...\r\n");
                ch32v20x_write_memory_word(handle, CH32V20X_FLASH_STATR, value | CH32V20X_FLASH_STATR_WRPRTERR);
            } else if (value & CH32V20X_FLASH_STATR_WRBUSY) {
                ESP_LOGD(TAG, "Waiting for busy flag to clear...\r\n");
                timeout--;
                if (timeout == 0) {
                    ESP_LOGE(TAG, "Timeout while waiting for target to clear busy flag!\r\n");
                    return false;
                }
            } else {
                uint32_t ctlr_value = 0;
                ch32v20x_read_memory_word(handle, CH32V20X_FLASH_CTLR, &ctlr_value);
                ESP_LOGE(TAG,
                         "Target busy for unknown reason (FLASH_STATR: 0x%08" PRIx32 ", FLASH_CTLR: 0x%08" PRIx32
                         ")!\r\n",
                         value, ctlr_value);
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            return true;
        }
    }
}

// Configure option bytes of the CH32V20X
bool ch32v20x_read_option_bytes(rvswd_handle_t* handle) {
    rvswd_result_t res;

    res = rvswd_init(handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "RVSWD initialization error %u!", res);
        return false;
    }

    res = rvswd_reset(handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "RVSWD reset error %u!", res);
        return false;
    }

    res = ch32v20x_reset_microprocessor_and_run(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset target");
        return false;
    }

    res = ch32v20x_halt_microprocessor(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to halt target");
        return false;
    }

    uint32_t option_bytes[4] = {0};
    for (uint8_t i = 0; i < 4; i++) {
        ch32v20x_read_memory_word(handle, CH32V20X_ADDR_OPTION_BYTES + (i * 4), &option_bytes[i]);
    }

    uint8_t rdpr = ((option_bytes[0] >> 0) & 0xFF);
    uint8_t nrdpr = ((option_bytes[0] >> 8) & 0xFF);

    if (((uint8_t)(~nrdpr)) == rdpr) {
        if (rdpr == 0xA5) {
            printf("Read protection disabled\r\n");
        } else {
            printf("Read protection enabled\r\n");
        }
    } else {
        printf("Invalid read protection config 0x%02x 0x%02x\r\n", rdpr, nrdpr);
    }

    uint8_t user = ((option_bytes[0] >> 16) & 0xFF);
    uint8_t nuser = ((option_bytes[0] >> 24) & 0xFF);
    if (((uint8_t)(~nuser)) == user) {
        if (user & (1 << 0)) {
            printf("Independent watchdog is disabled by hardware\r\n");
        } else {
            printf("Independent watchdog is not disabled by hardware\r\n");
        }
        if (user & (1 << 1)) {
            printf("System will not reset when entering stop mode\r\n");
        } else {
            printf("System will reset when entering stop mode\r\n");
        }
        if (user & (1 << 2)) {
            printf("System is not reset when entering standby mode\r\n");
        } else {
            printf("System is reset when entering standby mode\r\n");
        }
        uint8_t ram_code_mod = (user >> 6) & 3;
        printf("RAM code mode: %02X\r\n", ram_code_mod);
    } else {
        printf("Invalid user config 0x%02x 0x%02x\r\n", user, nuser);
    }

    uint8_t data0 = ((option_bytes[1] >> 0) & 0xFF);
    uint8_t ndata0 = ((option_bytes[1] >> 8) & 0xFF);
    if (((uint8_t)(~ndata0)) == data0) {
        printf("User data 0: 0x%02x\r\n", data0);
    } else {
        printf("Invalid user data 0 value 0x%02x 0x%02x\r\n", data0, ndata0);
    }

    uint8_t data1 = ((option_bytes[1] >> 16) & 0xFF);
    uint8_t ndata1 = ((option_bytes[1] >> 24) & 0xFF);
    if (((uint8_t)(~ndata1)) == data1) {
        printf("User data 1: 0x%02x\r\n", data1);
    } else {
        printf("Invalid user data 1 value 0x%02x 0x%02x\r\n", data1, ndata1);
    }

    uint8_t wrpr0 = ((option_bytes[2] >> 0) & 0xFF);
    uint8_t nwrpr0 = ((option_bytes[2] >> 8) & 0xFF);
    if (((uint8_t)(~nwrpr0)) == wrpr0) {
        printf("Write protection 0: 0x%02x\r\n", wrpr0);
    } else {
        printf("Invalid write protection 0 value 0x%02x 0x%02x\r\n", wrpr0, nwrpr0);
    }

    uint8_t wrpr1 = ((option_bytes[2] >> 16) & 0xFF);
    uint8_t nwrpr1 = ((option_bytes[2] >> 24) & 0xFF);
    if (((uint8_t)(~nwrpr1)) == wrpr1) {
        printf("Write protection 1: 0x%02x\r\n", wrpr1);
    } else {
        printf("Invalid write protection 1 value 0x%02x 0x%02x\r\n", wrpr1, nwrpr1);
    }

    uint8_t wrpr2 = ((option_bytes[3] >> 0) & 0xFF);
    uint8_t nwrpr2 = ((option_bytes[3] >> 8) & 0xFF);
    if (((uint8_t)(~nwrpr2)) == wrpr2) {
        printf("Write protection 2: 0x%02x\r\n", wrpr2);
    } else {
        printf("Invalid write protection 2 value 0x%02x 0x%02x\r\n", wrpr2, nwrpr2);
    }

    uint8_t wrpr3 = ((option_bytes[3] >> 16) & 0xFF);
    uint8_t nwrpr3 = ((option_bytes[3] >> 24) & 0xFF);
    if (((uint8_t)(~nwrpr3)) == wrpr3) {
        printf("Write protection 3: 0x%02x\r\n", wrpr3);
    } else {
        printf("Invalid write protection 3 value 0x%02x 0x%02x\r\n", wrpr3, nwrpr3);
    }

    return true;
}

// Program and restart the CH32V20X
bool ch32v20x_program(rvswd_handle_t* handle, void const* firmware, size_t firmware_len,
                      ch32v20x_status_callback status_callback) {
    rvswd_result_t res;

    res = rvswd_init(handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "RVSWD initialization error %u!", res);
        return false;
    }

    res = rvswd_reset(handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "RVSWD reset error %u!", res);
        return false;
    }

    res = ch32v20x_reset_microprocessor_and_run(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset target");
        return false;
    }

    res = ch32v20x_halt_microprocessor(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to halt target");
        return false;
    }

    bool bool_res = ch32v20x_unlock_flash(handle);

    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to unlock flash");
        return false;
    }

    bool_res = ch32v20x_clear_running_operations(handle);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to clear target running operations");
        return false;
    };

    bool_res = ch32v20x_write_flash(handle, 0x08000000, firmware, firmware_len, status_callback);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to write target flash");
        return false;
    };

    bool_res = ch32v20x_lock_flash(handle);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to lock target flash");
        return false;
    };

    res = ch32v20x_reset_microprocessor_and_run(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset target to run firmware");
        return false;
    }

    if (status_callback) {
        status_callback("Programming done", 100);
    }

    return true;
}
