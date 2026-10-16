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
        bc \
        tor \
        util-linux-extra \
    && rm -rf /var/lib/apt/lists/*

# helper-scripts supplies the guimessages and sanitize_string modules that
# sdwdate imports. Fetch it now (network is available only during build).
RUN git clone --depth 1 https://github.com/Kicksecure/helper-scripts.git \
        /opt/helper-scripts

# Resolve both sdwdate's own package and helper-scripts' modules offline.
ENV PYTHONPATH=/src/sdwdate/usr/lib/python3/dist-packages:/opt/helper-scripts/usr/lib/python3/dist-packages

# Project source (the scanner clones the repo and uses it as the build context).
COPY . /src/sdwdate
WORKDIR /src/sdwdate

# Build + verify inside the image. sdwdate has no unit-test suite, so the
# verification is: compile the C helper with its hardened flags, byte-compile
# every Python module (catches syntax errors), and import the full package so
# the whole dependency graph (third-party + ecosystem) is proven resolvable
# offline. Any failure fails the build loudly.
RUN make -C usr/src/sdwdate \
    && python3 -m py_compile usr/lib/python3/dist-packages/sdwdate/*.py \
    && python3 -c "import sdwdate.sdwdate, sdwdate.remote_times, sdwdate.config, sdwdate.timesanitycheck, sdwdate.proxy_settings"
