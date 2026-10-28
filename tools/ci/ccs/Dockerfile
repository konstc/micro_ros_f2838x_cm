# CCS build image for CI's firmware job: Code Composer Studio for headless
# builds, and the parts of C2000Ware that the projects use. Built with
# build_image.sh, which passes a local C2000Ware installation as the build
# context "c2000ware".
#
# Copyright 2026 Konstantin Chernyshov
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:24.04

ARG CCS_VERSION=21.0.1.00010
ARG CCS_URL=https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-J1VdearkvK/21.0.1/CCS_21.0.1.00010_linux.zip
ARG CCS_SHA256=3d3f38e5eb62bead98dc73d470dd3e3aed67b599b707fd1b95ccc2d027d4bfb2

LABEL org.opencontainers.image.source=https://github.com/konstc/micro_ros_f2838x_cm
LABEL org.opencontainers.image.description="CCS 21 and C2000Ware 26.02 for headless builds of micro_ros_f2838x_cm"

ENV CCS_ROOT=/opt/ti/ccs2101 \
    C2000WARE_ROOT=/opt/ti/c2000/C2000Ware_26_02_00_00

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl unzip python3 \
 && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /etc/udev/rules.d \
 && printf '#!/bin/sh\nexit 0\n' > /etc/init.d/udev \
 && printf '#!/bin/sh\nexit 0\n' > /usr/local/bin/udevadm \
 && chmod +x /etc/init.d/udev /usr/local/bin/udevadm

# C2000 support (PF_C28) brings the C2000 compiler and TI Arm Clang. Removed
# afterwards, as a headless build does not use them: the IDE (Theia), Resource
# Explorer, SysConfig, the debugger and emulation, the C29 compiler.
RUN cd /tmp \
 && curl -fsSL -o ccs.zip "$CCS_URL" \
 && echo "$CCS_SHA256  ccs.zip" | sha256sum -c - \
 && unzip -q ccs.zip \
 && "./CCS_${CCS_VERSION}_linux/ccs_setup_${CCS_VERSION}.run" --mode unattended \
        --prefix "$CCS_ROOT" --enable-components PF_C28 \
 && rm -rf /tmp/* \
 && cd "$CCS_ROOT/ccs" \
 && rm -rf theia tirex4 install_logs utils/sysconfig_* \
           ccs_base/cloudagent ccs_base/emulation ccs_base/DebugServer \
           tools/compiler/ti-cgt-c29_*

# .metadata/.tirex/package.*.json make C2000Ware a product CCS can discover.
COPY --from=c2000ware .metadata/sdk.json $C2000WARE_ROOT/.metadata/sdk.json
COPY --from=c2000ware .metadata/.tirex/package.tirex.json .metadata/.tirex/package.ccs.json $C2000WARE_ROOT/.metadata/.tirex/
COPY --from=c2000ware device_support/f2838x/common $C2000WARE_ROOT/device_support/f2838x/common
COPY --from=c2000ware driverlib/f2838x $C2000WARE_ROOT/driverlib/f2838x
COPY --from=c2000ware kernel/FreeRTOS $C2000WARE_ROOT/kernel/FreeRTOS
COPY --from=c2000ware libraries/communications/Ethernet/third_party/lwip $C2000WARE_ROOT/libraries/communications/Ethernet/third_party/lwip

# Registers C2000Ware as a product.
RUN "$CCS_ROOT/ccs/eclipse/ccs-server-cli.sh" -workspace /tmp/ws -application initialize \
        -ccs.productDiscoveryPath "$(dirname "$C2000WARE_ROOT")" \
 && rm -rf /tmp/ws "/root/.config/Texas Instruments"
