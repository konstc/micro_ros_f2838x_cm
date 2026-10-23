/*
 * Linker command file, lwIP part: link together with 2838x_cm_flash.cmd in
 * projects that use lwIP (memory regions are defined there).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */

SECTIONS
{
   .lwip_pbuf_pool : { *(.lwip.memp_memory_PBUF_POOL_base) } > E0RAM, type=NOINIT
   .lwip_mem       : { *(.lwip.*) } > SRAM, type=NOINIT
}
