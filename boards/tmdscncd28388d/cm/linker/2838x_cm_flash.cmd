/*
 * Linker command file: F2838x Connectivity Manager, TI Arm Clang, code in flash.
 *
 * CM memory (SPRUII0, CM memory map):
 *   flash   512 KB  0x00200000  (CPU1 boots the CM to the start of sector 0)
 *   C1/C0    16 KB  0x1FFFC000  (contiguous, CM-local)
 *   S0..S3   64 KB  0x20000000  (first 2 KB of S0 are used by the CM boot ROM)
 *   E0       16 KB  0x20010000  (Ethernet receive buffers: the lwIP pbuf pool)
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */

--retain=resetISR
--retain=vectorTableFlash

/* C2000Ware's prebuilt driverlib_cm.lib is built with a 16-bit wchar_t,
   tiarmclang code uses 32 bits; driverlib does not use wchar_t. */
--diag_suppress=16032

MEMORY
{
   CMBANK0_RESETISR : origin = 0x00200000, length = 0x00000008  /* boot-to-flash entry point */
   CMBANK0_FLASH    : origin = 0x00200008, length = 0x0007FFF8  /* sectors 0..13 */

   CRAM             : origin = 0x1FFFC000, length = 0x00004000  /* C1RAM + C0RAM */

   BOOT_RSVD        : origin = 0x20000000, length = 0x00000800  /* CM boot ROM stack */
   SRAM             : origin = 0x20000800, length = 0x0000F800  /* S0RAM (rest) .. S3RAM */
   E0RAM            : origin = 0x20010000, length = 0x00004000  /* Ethernet receive buffers */

   CPU1TOCMMSGRAM0  : origin = 0x20080000, length = 0x00000800
   CPU1TOCMMSGRAM1  : origin = 0x20080800, length = 0x00000800
   CMTOCPU1MSGRAM0  : origin = 0x20082000, length = 0x00000800
   CMTOCPU1MSGRAM1  : origin = 0x20082800, length = 0x00000800
   CPU2TOCMMSGRAM0  : origin = 0x20084000, length = 0x00000800
   CPU2TOCMMSGRAM1  : origin = 0x20084800, length = 0x00000800
   CMTOCPU2MSGRAM0  : origin = 0x20086000, length = 0x00000800
   CMTOCPU2MSGRAM1  : origin = 0x20086800, length = 0x00000800
}

SECTIONS
{
   /* Flash */
   .resetisr      : > CMBANK0_RESETISR
   .vftable       : > CMBANK0_FLASH                 /* 1024-byte aligned by the object */
   .text          : > CMBANK0_FLASH
   .rodata        : > CMBANK0_FLASH
   .cinit         : > CMBANK0_FLASH
   .init_array    : > CMBANK0_FLASH
   .ARM.exidx     : > CMBANK0_FLASH
   .ARM.extab     : > CMBANK0_FLASH

   /* Code that must run from RAM (flash wait-state setup, SysCtl_delay).
      CM_init() copies it before calling Flash_initModule(). */
   .TI.ramfunc    : {} LOAD = CMBANK0_FLASH,
                       RUN = CRAM,
                       LOAD_START(RamfuncsLoadStart),
                       LOAD_SIZE(RamfuncsLoadSize),
                       LOAD_END(RamfuncsLoadEnd),
                       RUN_START(RamfuncsRunStart),
                       RUN_SIZE(RamfuncsRunSize),
                       RUN_END(RamfuncsRunEnd),
                       ALIGN(8)

   /* CM-local RAM (not reachable by DMA): main stack (exceptions, and main()
      before the scheduler) and statically allocated task stacks (BOARD_TASK_STACK) */
   .stack         : > CRAM
   .task_stacks   : > CRAM, type=NOINIT

   /* Shared RAM */
   .vtable        : > SRAM                          /* run-time vector table, 1024-byte aligned */
   .freertos_heap : > SRAM, type=NOINIT             /* ucHeap, see board.c */
   .data          : > SRAM
   .bss           : > SRAM
   .sysmem        : > SRAM

   /* IPC message RAM (driverlib ipc.c) */
   MSGRAM_CM_TO_CPU1 : > CMTOCPU1MSGRAM0, type=NOINIT
   MSGRAM_CM_TO_CPU2 : > CMTOCPU2MSGRAM0, type=NOINIT
   MSGRAM_CPU1_TO_CM : > CPU1TOCMMSGRAM0, type=NOINIT
   MSGRAM_CPU2_TO_CM : > CPU2TOCMMSGRAM0, type=NOINIT
}
