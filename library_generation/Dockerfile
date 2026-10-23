# Build environment for libmicroros.a: Ubuntu 24.04, TI Arm Clang, and the host
# Python tools in a venv. No ROS 2 installation is needed (see build_library.sh).
#
# Built and run by build_library.sh, which passes the compiler version, URL and
# checksum as build arguments.
FROM ubuntu:24.04

ARG TIARMCLANG_VERSION
ARG TIARMCLANG_URL
ARG TIARMCLANG_SHA256

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential ca-certificates curl git python3 python3-venv rsync \
 && rm -rf /var/lib/apt/lists/*

RUN test -n "$TIARMCLANG_VERSION" && test -n "$TIARMCLANG_URL" && test -n "$TIARMCLANG_SHA256" \
 && curl -fsSL -o /tmp/tiarmclang.bin "$TIARMCLANG_URL" \
 && echo "$TIARMCLANG_SHA256  /tmp/tiarmclang.bin" | sha256sum -c - \
 && chmod +x /tmp/tiarmclang.bin \
 && /tmp/tiarmclang.bin --mode unattended --prefix /opt/ti \
 && rm /tmp/tiarmclang.bin \
 && test -x /opt/ti/ti-cgt-armllvm_${TIARMCLANG_VERSION}/bin/tiarmclang
ENV TIARMCLANG_ROOT=/opt/ti/ti-cgt-armllvm_${TIARMCLANG_VERSION}

COPY requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --no-cache-dir --disable-pip-version-check -r /tmp/requirements.txt
ENV PATH=/opt/venv/bin:$PATH

WORKDIR /project
