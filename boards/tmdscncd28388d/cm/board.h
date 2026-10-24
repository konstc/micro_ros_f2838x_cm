/*
 * Board support for the TMDSCNCD28388D controlCARD, CM side.
 *
 * Besides these functions the board implements the port's board interface
 * (port/include/microros_f2838x/board_api.h): time base here in board.c,
 * Ethernet in board_eth.c.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

/* Places a statically allocated task stack in CM-local RAM (C0/C1), which
 * keeps the shared S0..S3 RAM for data, buffers and the heap:
 *     static StackType_t stack[N] BOARD_TASK_STACK; */
#define BOARD_TASK_STACK   __attribute__((section(".task_stacks"), aligned(8)))

/* LEDs the CM owns (CPU1 hands them over in cpu1_bootstrap). */
/* D1, GPIO31 */
#define BOARD_LED_STATUS                                                   (0U)

/* Interrupt priorities (0 highest .. 7 lowest). ISRs that call FreeRTOS
 * "FromISR" functions must use 5..7 (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY). */
#define BOARD_IRQ_PRIORITY_ETH                                             (6U)
#define BOARD_IRQ_PRIORITY_TIME                                            (7U)

/* Flash wait states, RAM functions, peripheral clocks, vector table, time
 * base. Call first in main(), before any other driverlib call. */
void board_init(void);

void board_led_set(unsigned id, bool on);
void board_led_toggle(unsigned id);

#endif /* BOARD_H */
