#!/bin/bash

## SPDX-FileCopyrightText: ENCRYPTED SUPPORT LLC <adrelanos@kicksecure.com>
## SPDX-License-Identifier: AGPL-3.0-or-later

#### meta start
#### project Whonix
#### category networking and time
#### description
## hook to run <code>/usr/libexec/sdwdate/suspend-post</code>
## in Qubes-Whonix.
#### meta end

set -o errexit
set -o nounset
set -o pipefail
set -o errtrace
shopt -s inherit_errexit
shopt -s shift_verbose

/usr/libexec/sdwdate/suspend-post
