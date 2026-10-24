/*
 * micro-ROS port for the F2838x Connectivity Manager: initialisation.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_f2838x/microros_f2838x.h"

#include <time.h>

#include <rcutils/allocator.h>
#include <rmw_microros/rmw_microros.h>

#include "allocators.h"
#include "microros_f2838x_build_info.h"
#include "net_lwip.h"
#include "transports.h"

/* The project must use the same time_t as libmicroros.a (posix_shim/time.h checks the define). */
_Static_assert(MICROROS_F2838X_LIB_TIME_T_64 && (sizeof(time_t) == 8U),
               "libmicroros.a uses a 64-bit time_t");

static bool init_allocator(void)
{
    rcutils_allocator_t allocator = rcutils_get_zero_initialized_allocator();

    allocator.allocate = microros_f2838x_allocate;
    allocator.deallocate = microros_f2838x_deallocate;
    allocator.reallocate = microros_f2838x_reallocate;
    allocator.zero_allocate = microros_f2838x_zero_allocate;

    return rcutils_set_default_allocator(&allocator);
}

bool microros_f2838x_init(const microros_f2838x_cfg_t* cfg)
{
    if ((cfg == NULL) || !init_allocator())
    {
        return false;
    }

    switch (cfg->transport)
    {
        case MICROROS_F2838X_TRANSPORT_UDP:
            return net_start(cfg->udp.ip, cfg->udp.netmask, cfg->udp.gateway, cfg->udp.mac) &&
                   udp_transport_configure(cfg->udp.agent_ip, cfg->udp.agent_port) &&
                   (rmw_uros_set_custom_transport(false, NULL,
                                                  udp_transport_open, udp_transport_close,
                                                  udp_transport_write, udp_transport_read)
                    == RMW_RET_OK);

        default:
            return false;
    }
}

void microros_f2838x_lock_allocations(bool locked)
{
    microros_f2838x_allocator_lock(locked);
}
