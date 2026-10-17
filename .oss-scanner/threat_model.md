<!--
SPDX-FileCopyrightText: ENCRYPTED SUPPORT LLC <adrelanos@kicksecure.com>
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# sdwdate threat model

## What sdwdate is

sdwdate (Secure Distributed Web Date) is the default network time
synchronization daemon shipped in every Whonix and Kicksecure installation. It
is a Tor-friendly replacement for ntpdate/rdate: at randomized intervals it
connects over Tor (SOCKS) to a configured pool of remote webservers, reads the
`Date` HTTP header (RFC 2616) from their responses, and gradually adjusts the
system clock via the `sclockadj` helper.

Correct, attack-resistant time keeping is security-critical here: the clock
gates TLS certificate validity and expiry checks, Tor consensus freshness, and
anonymity. A clock an attacker can push off true can expire or un-expire
certificates, break or weaken Tor, and enable time-based correlation and
fingerprinting of an otherwise anonymous user.

## Trust boundary and untrusted input

The primary boundary is the network response from remote time sources. sdwdate
treats those servers as untrusted and attacker-influenced:

- A malicious or compromised time source can return arbitrary bytes, malformed
  or adversarial `Date` headers, oversized or slow responses, and can collude
  across the pool to bias the computed time.
- The transport is Tor; a hostile exit/onion path or a network attacker can
  tamper with or withhold responses.

In scope (highest priority), the code paths that parse and act on this
untrusted input:

- `usr/lib/python3/dist-packages/sdwdate/remote_times.py` - launches the
  per-source fetch, collects results, and parses the returned time.
- `usr/lib/python3/dist-packages/sdwdate/sdwdate.py` - the daemon: pool
  selection, randomization, aggregation of per-source times, decision logic,
  and the call into clock adjustment.
- `usr/lib/python3/dist-packages/sdwdate/timesanitycheck.py` - consensus and
  static sanity bounds on any candidate time before it is applied.
- `usr/lib/python3/dist-packages/sdwdate/config.py`,
  `proxy_settings.py` - configuration and proxy wiring.
- `usr/src/sdwdate/sclockadj.c` - the C helper that performs the gradual clock
  adjustment (memory/format/bounds correctness of a privileged time setter).

The `url_to_unixtime` fetch path (invoked by the daemon) is the live wire that
turns a remote HTTP response into a number the daemon trusts; parsing and
bounds there are of particular interest.

## Out of scope

- Compromise of the local root account or the system on which sdwdate runs.
- Debian packaging metadata, AppArmor profile text, man pages, and GUI/log
  viewer helpers that do not parse remote input.
- Deliberate misconfiguration by the local administrator (e.g. pointing sdwdate
  at an attacker-run pool on purpose). sdwdate's defense against a bad source is
  pool diversity plus the sanity checks, not trusting any single source.

## What an exploit would look like

Highest severity: remote input that leads to code execution, memory corruption
(notably in `sclockadj.c`), or a large undetected clock skew that bypasses the
sanity checks and is applied to the system clock. Medium: a remote source (or
pool collusion within the tolerated failure budget) that forces a crash or hang
of the daemon (time stops updating), or a skew within but abusing the tolerance
window. Lower: resource exhaustion from oversized/slow responses.

## Exercising the code offline

The build image installs every dependency and can run the Python modules with
no network (the fetch layer can be driven against local/stubbed responses). The
parsing and sanity-check functions in `remote_times.py` and
`timesanitycheck.py` are the natural targets for feeding crafted header/time
inputs, and `sclockadj.c` can be compiled (it is, during the build) and
exercised against adversarial numeric input.
