/*
 * C2000Ware CM driverlib for code built with TI Arm Clang.
 *
 * Include this header instead of "driverlib_cm.h" / "cm.h".
 *
 * The driverlib functions come from C2000Ware's prebuilt driverlib_cm.lib
 * (built with the legacy TI Arm compiler, which honours driverlib's
 * CODE_SECTION/DATA_SECTION pragmas: RAM functions, vector table and IPC
 * buffers are placed correctly). Only the headers are compiled by tiarmclang,
 * and they need three adjustments, all confined to this header:
 *
 *  - cpu.h declares and calls legacy TI intrinsics that clang does not support
 *    (_disable_IRQ, _set_interrupt_priority, ...). They are renamed to the
 *    inline-asm implementations below before cpu.h is read.
 *  - ethernet.h uses the legacy "interrupt" keyword; it is undefined while
 *    the headers are read (Cortex-M handlers are plain functions).
 *  - flash.h carries CODE_SECTION pragmas, which clang ignores; the warning is
 *    silenced only for the driverlib headers (as is one benign warning that an
 *    i2c.h inline function triggers).
 *
 * Link: C2000Ware driverlib/f2838x/driverlib_cm/ccs/<Debug|Release>/driverlib_cm.lib.
 * The linker command file suppresses warning 16032 (wchar_t size): the library
 * is built with a 16-bit wchar_t, tiarmclang uses 32 bits, and driverlib does
 * not use wchar_t.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DRIVERLIB_CM_TIARMCLANG_H
#define DRIVERLIB_CM_TIARMCLANG_H

#if !defined(__clang__) || !defined(__ti__)
#error "driverlib_cm_tiarmclang.h is for TI Arm Clang; with armcl include driverlib_cm.h directly"
#endif

#if defined(DRIVERLIB_CM_H) || defined(CPU_H)
#error "include driverlib_cm_tiarmclang.h before any other driverlib header"
#endif

#include <stdint.h>

static inline uint32_t driverlibCm_getPRIMASK(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask" : "=r"(primask));
    return primask;
}

static inline uint32_t driverlibCm_disableInterrupts(void)
{
    uint32_t faultmask;
    __asm volatile("mrs %0, faultmask\n"
                   "cpsid f" : "=r"(faultmask) : : "memory");
    return faultmask;
}

static inline uint32_t driverlibCm_enableInterrupts(void)
{
    uint32_t faultmask;
    __asm volatile("mrs %0, faultmask\n"
                   "cpsie f" : "=r"(faultmask) : : "memory");
    return faultmask;
}

static inline uint32_t driverlibCm_disableIRQ(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n"
                   "cpsid i" : "=r"(primask) : : "memory");
    return primask;
}

static inline uint32_t driverlibCm_enableIRQ(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n"
                   "cpsie i" : "=r"(primask) : : "memory");
    return primask;
}

static inline uint32_t driverlibCm_setInterruptPriority(uint32_t priority)
{
    uint32_t basepri;
    __asm volatile("mrs %0, basepri\n"
                   "msr basepri, %1" : "=&r"(basepri) : "r"(priority) : "memory");
    return basepri;
}

#define __get_PRIMASK           driverlibCm_getPRIMASK
#define _disable_interrupts     driverlibCm_disableInterrupts
#define _enable_interrupts      driverlibCm_enableInterrupts
#define _disable_IRQ            driverlibCm_disableIRQ
#define _enable_IRQ             driverlibCm_enableIRQ
#define _set_interrupt_priority driverlibCm_setInterruptPriority

#define interrupt
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wti-pragmas"
#pragma clang diagnostic ignored "-Wti-intrinsics"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"   /* i2c.h */
#include "driverlib_cm.h"
#include "cm.h"
#pragma clang diagnostic pop
#undef interrupt

#pragma GCC poison CPU_getFAULTMASK CPU_getBASEPRI Interrupt_getPriorityMask
#pragma GCC poison Flash_setWaitstates Flash_setBankPowerMode Flash_setPumpPowerMode
#pragma GCC poison Flash_enableProgramCache Flash_disableProgramCache
#pragma GCC poison Flash_enableDataCache Flash_disableDataCache Flash_enableECC
#pragma GCC poison CM_init CM_enableAllPeripherals

#endif /* DRIVERLIB_CM_TIARMCLANG_H */
