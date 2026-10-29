# micro-ROS for the TI F2838x Connectivity Manager

micro-ROS on the Cortex-M4 Connectivity Manager (CM) of the TI F2838x, with CCS
example projects for the TMDSCNCD28388D controlCARD on the TMDSHSECDOCK docking
station. CPU1 sets up clocks, pins and boots the CM; the CM runs FreeRTOS and
the micro-ROS client, which connects to a micro-ROS agent over Ethernet/UDP.

> **Status:** the examples
> [01_publisher_udp](examples/01_publisher_udp/README.md),
> [02_ping_pong_udp](examples/02_ping_pong_udp/README.md) and
> [03_services_parameters](examples/03_services_parameters/README.md) run on the
> board. A UART transport is planned.
>
> CM code uses C2000Ware's prebuilt `driverlib_cm.lib`; include
> `driverlib_cm_tiarmclang.h` (in `device/f2838x/cm/include/`) instead of
> `driverlib_cm.h` / `cm.h`, see the comment at its top.

## Used versions

| Component | Version |
| --- | --- |
| Code Composer Studio | 21.0.1 |
| TI Arm Clang (CM) | 5.1.1.LTS (installed with CCS) |
| TI C2000 code generation tools (CPU1) | 25.11.1.LTS (installed with CCS) |
| C2000Ware | 26.02.00.00, with F2838x support |
| FreeRTOS, lwIP | from C2000Ware (FreeRTOS V11.3.0, lwIP 2.1.2) |
| ROS 2 / micro-ROS | Jazzy (default), Lyrical, Rolling |

## Quick start

### 1. Build libmicroros.a

With Docker (Linux; on Windows run it from Git Bash with Docker Desktop running):

```
./library_generation/build_library.sh --distro jazzy --profile default
```

It builds with micro_ros_setup in micro-ROS's library builder image
(`microros/micro_ros_static_library_builder:<distro>`, pulled on the first run).
TI Arm Clang is downloaded once into the Docker volume
`micro_ros_f2838x_cm_cache`; the micro-ROS sources are fetched on every run.
`--distro` also takes `lyrical` and `rolling`.

Without Docker (Jazzy only), on Linux or in WSL2:

```
./library_generation/build_library.sh --native --distro jazzy --profile default
```

The first native build downloads TI Arm Clang for Linux and the micro-ROS
sources into `~/.cache/micro_ros_f2838x_cm`. The result is located in `microros_static_library/jazzy/default/`: `libmicroros.a`,
`include/`, `posix_shim/` and `build_info.json`. Profiles (`small`, `default`,
`large`) are described in
[library_generation/profiles/README.md](library_generation/profiles/README.md).

### 2. Import the projects into CCS

The projects find C2000Ware through the `COM_TI_C2000WARE_INSTALL_DIR`
variable, which CCS defines when it has registered C2000Ware as a product. If
CCS has not found C2000Ware, register its installation folder in CCS's
product settings.

Import `boards/tmdscncd28388d/cpu1_bootstrap/CCS/cpu1_bootstrap.projectspec`
and one CM project, e.g. `examples/01_publisher_udp/CCS/01_publisher_udp.projectspec`.
The CM projects link `microros_static_library/jazzy/default`; for another
build, change the project's `MICROROS_LIB` path variable, and for another
distribution set `MICROROS_DISTRO` for the agent (see
`tools/agent/docker-compose.yml`). For a project of your own, see
[examples/README.md](examples/README.md#using-micro-ros-in-your-own-ccs-project).

CCS reads a `.projectspec` only when importing; the project in the workspace
keeps its own copy of the settings. After a `.projectspec` changes (e.g. after
`git pull`), delete the project from the workspace and import it again. The
sources are linked from this repository, so deleting the workspace project
removes only CCS's project files and build output.

Headless build (Git Bash on Windows, or Linux), for CI or a quick check:

```
tools/ccs_build.sh --c2000ware C:/ti/c2000/C2000Ware_26_02_00_00 \
    boards/tmdscncd28388d/cpu1_bootstrap/CCS/cpu1_bootstrap.projectspec \
    examples/01_publisher_udp/CCS/01_publisher_udp.projectspec
```

### 3. Run

Board connections, switches and LEDs:
[boards/tmdscncd28388d](boards/tmdscncd28388d/README.md). With the
`TMS320F28388D.ccxml` target configuration (on-card XDS100v2, S1:A switch 1
ON):

1. Connect to `C28xx_CPU1`, load `cpu1_bootstrap.out` and run it. CPU1 boots the
   CM from flash and blinks LED D2.
2. Connect to `Cortex_M4_0`, load the CM program (CCS programs the CM flash),
   then restart and run. LED D1 blinks at 1 Hz until the board reaches the
   agent and stays on while connected; each example's README has the set-up.

The CM always runs from flash: the micro-ROS stack is larger than the CM's RAM.
For standalone operation, build `cpu1_bootstrap` in the `CPU1_FLASH`
configuration, program both cores and set the boot mode to flash.

TI's GEL scripts reset the device and clear RAM when CCS connects. To attach to
a running board instead, start a project-less debug session with
`boards/tmdscncd28388d/targetConfigs/TMDSCNCD28388D_attach.ccxml`, connect to
`Cortex_M4_0` (CCS connects CPU1 as well), load only the symbols of the CM
program (Load Symbols) and resume. Do not load or flash programs with this
configuration: CM flash programming relies on TI's reset.

## Repository layout

| Path | Contents |
| --- | --- |
| `library_generation/` | Docker/native build of `libmicroros.a` with tiarmclang: toolchain file, profiles, POSIX shim headers, extra message packages |
| `port/` | Board-independent port layer: UDP transport and lwIP net task, time, allocator, libc shims |
| `device/f2838x/cm/` | F2838x CM support shared by all boards: startup code, FreeRTOS port, driverlib header wrapper for tiarmclang |
| `boards/tmdscncd28388d/` | controlCARD support: CPU1 bootstrap, CM board code, linker command files, FreeRTOS config |
| `examples/` | CCS example projects, and how to set up a project of your own |
| `tests/` | `link_check` (full micro-ROS API surface in a CM image, build only), `port` (host unit tests) |
| `tools/` | Headless CCS build, RAM report, host tests, repository checks, round-trip histogram, micro-ROS agent (Docker Compose), CCS image for CI |

## License

Apache-2.0, see [LICENSE](LICENSE). Third-party components are listed in
[3rd-party-licenses.txt](3rd-party-licenses.txt).
