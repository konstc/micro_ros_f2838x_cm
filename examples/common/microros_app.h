/*
 * Shared main() and micro-ROS task for the examples (microros_app.c).
 *
 * The task keeps a session with the agent alive: it waits for the agent,
 * creates support, node and executor, calls app_create(), spins, and on
 * losing the agent destroys everything and starts over.
 * LED D1: 1 Hz blink = waiting for the agent, on = connected,
 * 5 Hz blink = initialization failed.
 *
 * An example provides the definitions below.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_APP_H
#define MICROROS_APP_H

#include <stdbool.h>

#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>

#include "microros_f2838x/microros_f2838x.h"
#include "microros_f2838x/rcl_compat.h"

/* Transport configuration. */
extern const microros_f2838x_cfg_t app_config;

/* Node name, and the number of executor handles app_create() adds. */
extern const char app_node_name[];
extern const unsigned app_executor_handles;

/* Create the example's entities and add them to the executor. */
bool app_create(rcl_node_t* node, rclc_support_t* support, rclc_executor_t* executor);

/* Destroy what app_create() created (the session may already be gone). */
void app_destroy(rcl_node_t* node);

#endif /* MICROROS_APP_H */
