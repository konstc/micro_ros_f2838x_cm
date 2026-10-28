# Examples

| Example | Short description |
| --- | --- |
| [01_publisher_udp](01_publisher_udp/README.md) | Timer and publisher functionality |
| [02_ping_pong_udp](02_ping_pong_udp/README.md) | Subscription answered by publisher (ping/pong), time synchronization with the agent |
| [03_services_parameters](03_services_parameters/README.md) | Service and the rclc parameter server |

All of them use Ethernet/UDP and share `common/`: `main()`, the micro-ROS
task and the network settings.

## Using micro-ROS in your CCS project

How to build a CCS project of your own for the F2838x Connectivity Manager
(CM) with micro-ROS: either starting from one of the examples, or adding
micro-ROS to an existing CM project. It assumes the tools listed in the
[README](../README.md#used-versions) and a library built as in
[step 1](../README.md#1-build-libmicrorosa) there.

### Core assignment

- **CPU1** sets up the clocks (CMCLK 125 MHz) and the Ethernet clock, routes
  the CM's pins (Ethernet MII and MDIO, PHY reset and power-down, LED D1) and
  boots the CM from flash. [cpu1_bootstrap](../boards/tmdscncd28388d/cpu1_bootstrap/main_cpu1.c)
  does only that. If CPU1 runs an application of yours, add these steps to it,
  before the CM is booted.
- **CM** runs FreeRTOS, from flash. micro-ROS runs in one task of the
  application; the port adds a network task that owns lwIP.

### Starting from an example

Keep this repository next to your project, e.g. as a git submodule, and link
its sources as the examples do.

1. Copy an example's `.projectspec` into your project:
   [01_publisher_udp](01_publisher_udp/CCS/01_publisher_udp.projectspec)
   for topics and timers,
   [03_services_parameters](03_services_parameters/CCS/03_services_parameters.projectspec)
   for services and parameters. Change:
   - the project `name`;
   - `MICROROS_F2838X_ROOT`: the path from the `.projectspec` to this
     repository, e.g. `../third_party/micro_ros_f2838x_cm`;
   - `MICROROS_LIB`: the library build to link (distribution and profile);
   - the application: `src/app.c` and the include path `examples/<example>/src`
     become your own files.
2. Copy [udp_config.c](common/udp_config.c) into your project and
   set the addresses. `microros_app.c` and `freertos_hooks.c` can stay linked
   from this repository.
3. Write `app.c` against [microros_app.h](common/microros_app.h):
   the node name, the number of executor handles, and `app_create()` /
   `app_destroy()` for your publishers, subscriptions, timers and services.
   [01_publisher_udp/src/app.c](01_publisher_udp/src/app.c) is the
   smallest example. Declare timer callbacks with
   `MICROROS_F2838X_TIMER_CALLBACK_ARGS` from
   [rcl_compat.h](../port/include/microros_f2838x/rcl_compat.h): rcl passes
   them an extra argument since Lyrical.
4. Import, build and load as in the
   [README](../README.md#2-import-the-projects-into-ccs).

`microros_app.c` provides `main()` and the micro-ROS task: it waits for the
agent, creates the entities through `app_create()`, spins the executor, and
starts over when the agent is lost (agent restart, unplugged cable). LED D1
shows the state.

### What a project needs

For adding micro-ROS to an existing CM project, or for checking a changed one.
The example `.projectspec` files contain every item below.

#### Compiler

These settings must match the library (`build_info.json` records the flags it
was built with):

- `-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -mlittle-endian`;
- `-D__TI_TIME_USES_64=1`: the library uses a 64-bit `time_t`;
  `posix_shim/time.h` and the port stop the build otherwise;
- include paths: `<library>/posix_shim` before any other, then
  `<library>/include`.

CM code includes `driverlib_cm_tiarmclang.h` instead of `driverlib_cm.h`; see
the comment at its top.

#### Sources

| Part | Files |
| --- | --- |
| micro-ROS | `libmicroros.a` of the library build |
| Port | `port/src/*.c`, include path `port/include` |
| Board | `boards/tmdscncd28388d/cm/`: `board.c`, `board_eth.c`, `lwipopts.h`, `FreeRTOSConfig.h` |
| Device | `device/f2838x/cm/`: `startup/startup_cm.c`, `lwip/*.c`, include path `include`; C2000Ware's prebuilt `driverlib_cm.lib` |
| FreeRTOS | kernel from C2000Ware (`tasks.c`, `queue.c`, `list.c`, `timers.c`, `stream_buffer.c`, `portable/MemMang/heap_4.c`) and `device/f2838x/cm/freertos_port/port.c` |
| lwIP | core from C2000Ware: IPv4, ARP, ICMP and UDP, as listed in the example `.projectspec` |

The port uses the board only through
[board_api.h](../port/include/microros_f2838x/board_api.h) (time base and
Ethernet). For another board, implement those functions, and `board.h` if you
use `microros_app.c` (`board_init()`, the LED functions, `BOARD_TASK_STACK`).

With startup code of your own, the vector table must send SVCall, PendSV and
SysTick straight to the FreeRTOS handlers, as `startup_cm.c` does; FreeRTOS
checks this when the scheduler starts.

#### Linker

- `2838x_cm_flash.cmd` and `2838x_cm_lwip.cmd` from
  `boards/tmdscncd28388d/cm/linker/`, or a command file of your own with the
  same sections: `.task_stacks` (task stacks, in CM-local RAM),
  `.freertos_heap`, and `.lwip.*` and the lwIP receive pool
  in RAM that the Ethernet DMA can reach, which C0/C1 RAM is not.
- `--heap_size=0 --diag_suppress=10473`: micro-ROS allocates from the FreeRTOS
  heap, so the C library heap is unused (10473 is the linker's warning about
  it). If your own code calls `malloc()`, give the C heap a size.
- `--stack_size=0x800`: the main stack serves interrupts, and `main()` before
  the scheduler starts.

#### FreeRTOS

Start from the board's [FreeRTOSConfig.h](../boards/tmdscncd28388d/cm/FreeRTOSConfig.h).
The port needs:

- static and dynamic allocation, mutexes, task notifications and stream
  buffers (`configUSE_STREAM_BUFFERS`);
- `INCLUDE_vTaskSuspend`, `INCLUDE_xTaskGetCurrentTaskHandle` and
  `INCLUDE_uxTaskGetStackHighWaterMark`;
- `heap_4.c` (or `heap_5.c`): the port reads the heap's low-water mark;
- `configCPU_CLOCK_HZ` equal to the CMCLK that CPU1 sets up (125 MHz with
  `cpu1_bootstrap`).

Interrupts that call FreeRTOS functions need a priority from 5 to 7
(`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`); the board uses 6 for
Ethernet and 7 for its time base. The port's own settings (network task stack
and priority, UDP receive buffer) are in
[microros_f2838x_config.h](../port/include/microros_f2838x/microros_f2838x_config.h)
and can be overridden with `-D`.

### Writing the micro-ROS task

Without `microros_app.c`, keep its order:

1. In the micro-ROS task, call `microros_f2838x_init()` once (the scheduler
   must be running). It makes the FreeRTOS heap the rcl allocator, starts the
   network and registers the UDP transport.
2. Wait for the agent with `rmw_uros_ping_agent()`.
3. Create support, node, executor and entities. The `rcl_allocator_t` passed
   to `rclc_support_init()` and `rclc_executor_init()` must stay alive (e.g. be
   static): both keep a pointer to it.
4. Call `rclc_executor_prepare()`, then `microros_f2838x_lock_allocations(true)`.
   From then on nothing should allocate;
   `microros_f2838x_stats.allocs_while_locked` counts allocations that happen
   anyway.
5. Spin with `rclc_executor_spin_some()` and ping the agent now and then. When
   the agent is lost: unlock the allocations, set the destroy timeout to 0
   (`rmw_uros_set_context_entity_destroy_session_timeout()`), finalize
   everything in reverse order and go back to step 2.

Call micro-ROS only from this one task. lwIP belongs to the port's network
task: do not call lwIP from other tasks.

### Sizing

- **Entities**: the library profile sets how many nodes, publishers,
  subscriptions, services and clients there can be; see
  [profiles](../library_generation/profiles/README.md). The rclc parameter
  server alone takes 6 services and 1 publisher. Beyond the limits, the
  `*_init()` call fails; with `microros_app.c`, LED D1 then keeps blinking.
  For other limits, copy a `.meta` file to `library_generation/profiles/<name>.meta`
  and build with `--profile <name>`.
- **FreeRTOS heap**: `configTOTAL_HEAP_SIZE`, 4 KB by default.
  03_services_parameters sets 8 KB for the parameter server, which needs about
  6.4 KB. Check `microros_f2838x_stats.heap_free_min` on the board.
- **micro-ROS task stack**: `APP_UROS_STACK_WORDS` with `microros_app.c`,
  1536 words by default. Check `microros_f2838x_stats.uros_stack_free_min_words`.
- **RAM**: `tools/ram_report.py <map file> --min-margin 8K` shows the use of
  each RAM region and fails below the margin. In Release builds,
  01_publisher_udp leaves about 31 KB free, 03_services_parameters about 16 KB.

### Messages of your own

Build them into the library: `build_library.sh --extra-packages DIR` (in
Docker mode, DIR must be inside this repository), or list their repositories in
[extra_packages.repos](../library_generation/extra_packages/extra_packages.repos);
package folders next to that file are built in as well.

### Debugging

- `microros_f2838x_stats` (Expressions view): session state, reconnects, UDP
  counters, heap and stack use.
- After a fault the CM stops in `faultISR`; `g_startupFault` holds the fault
  status registers and the stacked PC and LR.
- A stack overflow or a failed FreeRTOS allocation stops in the hooks of
  `freertos_hooks.c`; `g_stackOverflowTask` names the task.
- To look at a running board without resetting it, use the attach
  configuration described in the [README](../README.md#3-run).
