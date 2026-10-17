## SPDX-FileCopyrightText: ENCRYPTED SUPPORT LLC <adrelanos@kicksecure.com>
## SPDX-License-Identifier: AGPL-3.0-or-later

# OSS Scanner build environment for sdwdate.
#
# Contract: this Dockerfile runs with network access. The scan then runs the
# built image OFFLINE. So every dependency the build, the C helper, and the
# import smoke test need must be fetched HERE.
#
# sdwdate's remote-attack surface (the reason it is a good scan target): it
# connects out over Tor to arbitrary onion webservers and parses the HTTP
# Date header (RFC 2616) from responses it does not control. The parsing paths
# live in remote_times.py / timesanitycheck.py / sdwdate.py.

FROM debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive

# Build toolchain + every runtime dependency that Debian ships.
#  - build-essential: gcc, make, libc6-dev for the sclockadj C helper.
#  - python3-stem/-dateutil/-socks/-sdnotify/-requests: sdwdate's third-party
#    Python imports.
#  - python3-yaml: pulled in transitively via helper-scripts' guimessages.
#  - bc, tor, util-linux-extra: runtime Depends that may appear in code paths.
# Kicksecure-only Depends (helper-scripts, msgcollector, privleap) are not in
# Debian: helper-scripts is cloned below for the two modules sdwdate imports
# (guimessages, sanitize_string); privleap is a runtime-only dependency with no
# effect on building or importing the code under analysis.
RUN apt-get update && apt-get install --yes --no-install-recommends \
        build-essential \
        git \
        ca-certificates \
        python3 \
        python3-stem \
        python3-dateutil \
        python3-socks \
        python3-sdnotify \
        python3-requests \
        python3-yaml \
        bc \
        tor \
        util-linux-extra \
    && rm -rf /var/lib/apt/lists/*

# helper-scripts supplies the guimessages and sanitize_string modules that
# sdwdate imports. Fetch it now (network is available only during build).
RUN git clone --depth 1 https://github.com/Kicksecure/helper-scripts.git \
        /opt/helper-scripts

# Resolve both sdwdate's own package and helper-scripts' modules offline.
ENV PYTHONPATH=/src/usr/lib/python3/dist-packages:/opt/helper-scripts/usr/lib/python3/dist-packages

# Scanner contract: the checkout lives inside the image at /src (COPY . /src),
# with its .git attached there -- do not nest it in a subdirectory.
COPY . /src
WORKDIR /src

# Build the C helper as the maintainers build it (the project Makefile compiles
# sclockadj with debug info plus a hardened flag set). A build step that fails
# fails the build, per the scanner contract.
RUN make -C usr/src/sdwdate

# Cheap offline smoke test. sdwdate's real suite (dist-ai tests, ClusterFuzzLite
# fuzzers, the integration scripts) needs Tor / network, which is gone after the
# build, so the offline check is: byte-compile every module and import the full
# package, proving the whole dependency graph (third-party + ecosystem) resolves.
# Per the scanner contract a failing test must NOT abort the build.
RUN { python3 -m py_compile usr/lib/python3/dist-packages/sdwdate/*.py \
      && python3 -c "import sdwdate.sdwdate, sdwdate.remote_times, sdwdate.config, sdwdate.timesanitycheck, sdwdate.proxy_settings" ; } \
    || echo "WARNING: sdwdate offline smoke test reported failures"
