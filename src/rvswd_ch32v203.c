
/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include "rvswd_ch32v203.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "string.h"

static char const TAG[] = "RVSWD-CH32V203";

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

// The start of CH32 CODE FLASH region.
#define CH32_CODE_BEGIN 0x08000000
// the end of the CH32 CODE FLASH region.
#define CH32_CODE_END 0x08004000

// FLASH status register.
#define CH32_FLASH_STATR 0x4002200C
// FLASH configuration register.
#define CH32_FLASH_CTLR 0x40022010
// FLASH address register.
#define CH32_FLASH_ADDR 0x40022014

// FLASH is busy writing or erasing.
#define CH32_FLASH_STATR_BUSY (1 << 0)
// FLASH is busy writing
#define CH32_FLASH_STATR_WRBUSY (1 << 1)
// FLASH write protection error
#define CH32_FLASH_STATR_WRPRTERR (1 << 4)
// FLASH is finished with the operation.
#define CH32_FLASH_STATR_EOP (1 << 5)
// FLASH enhanced read mode start
#define CH32_FLASH_STATR_EHMODS (1 << 7)

// Perform standard programming operation.
#define CH32_FLASH_CTLR_PG (1 << 0)
// Perform 1K sector erase.
#define CH32_FLASH_CTLR_PER (1 << 1)
// Perform full FLASH erase.
#define CH32_FLASH_CTLR_MER (1 << 2)
// Perform user-selected word program.
#define CH32_FLASH_CTLR_OBG (1 << 4)
// Perform user-selected word erasure.
#define CH32_FLASH_CTLR_OBER (1 << 5)
// Start an erase operation.
#define CH32_FLASH_CTLR_STRT (1 << 6)
// Lock the FLASH.
#define CH32_FLASH_CTLR_LOCK (1 << 7)
// Start a fast page programming operation (256 bytes).
#define CH32_FLASH_CTLR_FTPG (1 << 16)
// Start a fast page erase operation (256 bytes).
#define CH32_FLASH_CTLR_FTER (1 << 17)
// Start a page programming operation (256 bytes).
#define CH32_FLASH_CTLR_PGSTRT (1 << 21)

uint8_t const ch32_readmem[] = {0x88, 0x41, 0x02, 0x90};

uint8_t const ch32_writemem[] = {0x88, 0xc1, 0x02, 0x90};

rvswd_result_t ch32_halt_microprocessor(rvswd_handle_t* handle) {
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

rvswd_result_t ch32_resume_microprocessor(rvswd_handle_t* handle) {
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

rvswd_result_t ch32_reset_microprocessor_and_run(rvswd_handle_t* handle) {
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

bool ch32_write_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t value) {
    uint32_t command = regno         // Register to access.
                       | (1 << 16)   // Write access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_DATA0, value);
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    return true;
}

bool ch32_read_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t* value_out) {
    uint32_t command = regno         // Register to access.
                       | (0 << 16)   // Read access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    rvswd_read(handle, CH32_REG_DEBUG_DATA0, value_out);
    return true;
}

bool ch32_run_debug_code(rvswd_handle_t* handle, void const* code, size_t code_size) {
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

bool ch32_read_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t* value_out) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_readmem, sizeof(ch32_readmem));
    ch32_read_cpu_reg(handle, CH32_REGS_GPR + 10, value_out);
    return true;
}

bool ch32_write_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t value) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 10, value);
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_writemem, sizeof(ch32_writemem));
    return true;
}

// Wait for the FLASH chip to finish its current operation.
static bool ch32_wait_flash(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);

    uint32_t timeout = 1000;

    while (value & CH32_FLASH_STATR_BUSY) {
        ESP_LOGD(TAG, "Flash busy: FLASH_STATR = 0x%08" PRIx32 "\r\n", value);
        vTaskDelay(1);
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
        timeout--;
        if (timeout == 0) {
            return false;
        }
    }
    return true;
}

static void ch32_wait_flash_write(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    while (value & CH32_FLASH_STATR_WRBUSY) {
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    }
}

// Unlock the FLASH if not already unlocked.
bool ch32_unlock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    // Enter the unlock keys.
    ch32_write_memory_word(handle, 0x40022004, 0x45670123);
    ch32_write_memory_word(handle, 0x40022004, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022008, 0x45670123);
    ch32_write_memory_word(handle, 0x40022008, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022024, 0x45670123);
    ch32_write_memory_word(handle, 0x40022024, 0xCDEF89AB);

    // Check again if FLASH is unlocked.
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    return ((ctlr & CH32_FLASH_CTLR_LOCK) != CH32_FLASH_CTLR_LOCK);
}

// Lock the FLASH
bool ch32_lock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;

    // Check if FLASH is locked
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    if ((ctlr & CH32_FLASH_CTLR_LOCK) == CH32_FLASH_CTLR_LOCK) {
        ESP_LOGW(TAG, "Target flash already locked");
        return true;
    }

    // Lock FLASH
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, ctlr | CH32_FLASH_CTLR_LOCK);

    // Check again if FLASH is locked
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    return ((ctlr & CH32_FLASH_CTLR_LOCK) == CH32_FLASH_CTLR_LOCK);
}

// If unlocked: Erase a 256-byte block of FLASH.
bool ch32_erase_flash_block(rvswd_handle_t* handle, uint32_t addr) {
    if (addr % 256) return false;
    bool wait_res = ch32_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER);
    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER | CH32_FLASH_CTLR_STRT);
    wait_res = ch32_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    return true;
}

// If unlocked: Write a 256-byte block of FLASH.
bool ch32_write_flash_block(rvswd_handle_t* handle, uint32_t addr, void const* data) {
    if (addr % 256) return false;

    bool wait_res = ch32_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG);

    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);

    uint32_t wdata[64];
    memcpy(wdata, data, sizeof(wdata));
    for (size_t i = 0; i < 64; i++) {
        ch32_write_memory_word(handle, addr + i * 4, wdata[i]);
        ch32_wait_flash_write(handle);
    }

    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG | CH32_FLASH_CTLR_PGSTRT);
    wait_res = ch32_wait_flash(handle);
    if (!wait_res) {
        return false;
    }
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    vTaskDelay(1);

    uint32_t rdata[64];
    for (size_t i = 0; i < 64; i++) {
        vTaskDelay(0);
        ch32_read_memory_word(handle, addr + i * 4, &rdata[i]);
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

// If unlocked: Erase and write a range of FLASH memory.
bool ch32_write_flash(rvswd_handle_t* handle, uint32_t addr, void const* _data, size_t data_len) {
    if (addr % 64) {
        return false;
    }

    uint8_t const* data = _data;

    char buffer[32];

    for (size_t i = 0; i < data_len; i += 256) {
        vTaskDelay(0);
        snprintf(buffer, sizeof(buffer) - 1, "Writing at 0x%08" PRIx32, addr + i);
        ch32_status_callback(buffer, i, data_len);

        if (!ch32_erase_flash_block(handle, addr + i)) {
            ESP_LOGE(TAG, "Error: Failed to erase FLASH at %08" PRIx32, addr + i);
            return false;
        }

        if (!ch32_write_flash_block(handle, addr + i, data + i)) {
            ESP_LOGE(TAG, "Error: Failed to write FLASH at %08" PRIx32, addr + i);
            return false;
        }
    }

    return true;
}

bool ch32_clear_running_operations(rvswd_handle_t* handle) {
    uint32_t timeout = 100;
    while (1) {
        uint32_t value = 0;
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
        if (value & CH32_FLASH_STATR_BUSY) {
            if (value & CH32_FLASH_STATR_EOP) {
                ESP_LOGD(TAG, "Clearing EOP flag...\r\n");
                ch32_write_memory_word(handle, CH32_FLASH_STATR, value | CH32_FLASH_STATR_EOP);
            } else if (value & CH32_FLASH_STATR_WRPRTERR) {
                ESP_LOGD(TAG, "Clearing WRPRTERR flag...\r\n");
                ch32_write_memory_word(handle, CH32_FLASH_STATR, value | CH32_FLASH_STATR_WRPRTERR);
            } else if (value & CH32_FLASH_STATR_WRBUSY) {
                ESP_LOGD(TAG, "Waiting for busy flag to clear...\r\n");
                timeout--;
                if (timeout == 0) {
                    ESP_LOGE(TAG, "Timeout while waiting for target to clear busy flag!\r\n");
                    return false;
                }
            } else {
                uint32_t ctlr_value = 0;
                ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr_value);
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

// Program and restart the CH32V203.
bool ch32_program(rvswd_handle_t* handle, void const* firmware, size_t firmware_len) {
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

    res = ch32_reset_microprocessor_and_run(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset target");
        return false;
    }

    res = ch32_halt_microprocessor(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to halt target");
        return false;
    }

    bool bool_res = ch32_unlock_flash(handle);

    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to unlock flash");
        return false;
    }

    bool_res = ch32_clear_running_operations(handle);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to clear target running operations");
        return false;
    };

    bool_res = ch32_write_flash(handle, 0x08000000, firmware, firmware_len);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to write target flash");
        return false;
    };

    bool_res = ch32_lock_flash(handle);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to lock target flash");
        return false;
    };

    res = ch32_reset_microprocessor_and_run(handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset target to run firmware");
        return false;
    }

    return true;
}

// Default status callback implementation.
void __attribute__((weak)) ch32_status_callback(char const* msg, int progress, int total) {
    ESP_LOGI(TAG, "%s: %d%% (%d/%d)", msg, progress * 100 / total, progress, total);
}
