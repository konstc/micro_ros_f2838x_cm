/*
 * Transport configuration of the UDP examples (01, 02, 03).
 *
 * Default set-up: the board on a direct cable to a PC network card that has
 * the static address 192.168.10.1/24; the micro-ROS agent runs on that PC
 * (tools/agent/docker-compose.yml) on UDP port 8888.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_app.h"

const microros_f2838x_cfg_t app_config = {
    .transport = MICROROS_F2838X_TRANSPORT_UDP,
    .udp = {
        .ip = "192.168.10.2",
        .netmask = "255.255.255.0",
        .gateway = "0.0.0.0",
        /* Locally administered address (0x02 in the first byte); change it
         * if several boards share a network. */
        .mac = { 0x02U, 0x28U, 0x38U, 0x8DU, 0x00U, 0x02U },
        .agent_ip = "192.168.10.1",
        .agent_port = 8888U,
    },
};
