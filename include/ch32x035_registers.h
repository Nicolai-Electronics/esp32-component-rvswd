/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Register definitions from the CH32X035 reference manual

// CH32X035 flash status register
#define CH32X035_FLASH_STATR_BSY         (1 << 0)   // Flash is busy writing or erasing
#define CH32X035_FLASH_STATR_WRPRTERR    (1 << 4)   // Flash write protection error
#define CH32X035_FLASH_STATR_EOP         (1 << 5)   // Flash is finished with the operation
#define CH32X035_FLASH_STATR_FWAKE_FLAG  (1 << 6)   // Flash wake-up flag
#define CH32X035_FLASH_STATR_TURBO       (1 << 7)   // Flash turbo mode enable
#define CH32X035_FLASH_STATR_BOOT_AVA    (1 << 12)  // Initial configuration word status
#define CH32X035_FLASH_STATR_BOOT_STATUS (1 << 13)  // Source of currently executing program
#define CH32X035_FLASH_STATR_BOOT_MODE   (1 << 14)  // Switch between user area and boot area
#define CH32X035_FLASH_STATR_BOOT_LOCK   (1 << 15)  // Boot zone lockout

// CH32X035 flash control register
#define CH32X035_FLASH_CTLR_PER     (1 << 1)   // Perform 1K sector erase
#define CH32X035_FLASH_CTLR_MER     (1 << 2)   // Perform full flash erase
#define CH32X035_FLASH_CTLR_OBPG    (1 << 4)   // Perform user option bytes word program
#define CH32X035_FLASH_CTLR_OBER    (1 << 5)   // Perform user option bytes word erasure
#define CH32X035_FLASH_CTLR_STRT    (1 << 6)   // Start an erase or program operation
#define CH32X035_FLASH_CTLR_LOCK    (1 << 7)   // Lock the flash
#define CH32X035_FLASH_CTLR_OBWRE   (1 << 9)   // Unlock the user option bytes
#define CH32X035_FLASH_CTLR_ERRIE   (1 << 10)  // Error status interrupt enable
#define CH32X035_FLASH_CTLR_EOPIE   (1 << 12)  // Operation completion interrupt enable
#define CH32X035_FLASH_CTLR_FWAKEIE (1 << 13)  // Wake-up interrupt enable
#define CH32X035_FLASH_CTLR_FLOCK   (1 << 15)  // Fast programming lock
#define CH32X035_FLASH_CTLR_FTPG    (1 << 16)  // Perform a fast page programming operation (256 bytes)
#define CH32X035_FLASH_CTLR_FTER    (1 << 17)  // Start a fast page erase operation (256 bytes)
#define CH32X035_FLASH_CTLR_BUFLOAD (1 << 18)  // Cache data into BUF
#define CH32X035_FLASH_CTLR_BUFRST  (1 << 19)  // Buffer BUF reset operation
#define CH32X035_FLASH_CTLR_BER32   (1 << 23)  // Execute block erasure 32KB
