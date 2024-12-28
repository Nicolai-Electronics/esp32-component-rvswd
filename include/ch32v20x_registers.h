/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Register definitions from the CH32V20x and CH32V30x reference manual

// Addresses
#define CH32V20X_ADDR_OPTION_BYTES 0x1FFFF800

// CH32V20X and CH32V30X flash status register
#define CH32V20X_FLASH_STATR_BSY      (1 << 0)  // Flash is busy writing or erasing
#define CH32V20X_FLASH_STATR_WRBUSY   (1 << 1)  // Flash is busy writing
#define CH32V20X_FLASH_STATR_WRPRTERR (1 << 4)  // Flash write protection error
#define CH32V20X_FLASH_STATR_EOP      (1 << 5)  // Flash is finished with the operation
#define CH32V20X_FLASH_STATR_EHMODS   (1 << 7)  // Flash enhanced read mode start

// CH32V20X and CH32V30X flash control register
#define CH32V20X_FLASH_CTLR_PG     (1 << 0)   // Perform standard programming operation
#define CH32V20X_FLASH_CTLR_PER    (1 << 1)   // Perform 1K sector erase
#define CH32V20X_FLASH_CTLR_MER    (1 << 2)   // Perform full Flash erase
#define CH32V20X_FLASH_CTLR_OBG    (1 << 4)   // Perform user-selected word program
#define CH32V20X_FLASH_CTLR_OBER   (1 << 5)   // Perform user-selected word erasure
#define CH32V20X_FLASH_CTLR_STRT   (1 << 6)   // Start an erase operation
#define CH32V20X_FLASH_CTLR_LOCK   (1 << 7)   // Lock the FLASH
#define CH32V20X_FLASH_CTLR_FTPG   (1 << 16)  // Start a fast page programming operation (256 bytes)
#define CH32V20X_FLASH_CTLR_FTER   (1 << 17)  // Start a fast page erase operation (256 bytes)
#define CH32V20X_FLASH_CTLR_PGSTRT (1 << 21)  // Start a page programming operation (256 bytes)
