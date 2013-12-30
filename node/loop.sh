# $FreeBSD$
#
# Copyright 2013 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# \file loop.sh
# Executes iterate.sh on one or more configurations in an infinite loop.
#
# This script implements a "daemon mode" so that it can easily be run from
# within an rc.d script at system startup.

shtk_import bool
shtk_import cleanup
shtk_import cli
shtk_import process


# Paths to installed files.
#
# Can be overriden for test purposes only.
: ${AUTOTEST_BINDIR="__AUTOTEST_BINDIR__"}
: ${AUTOTEST_ETCDIR="__AUTOTEST_ETCDIR__"}


# Creates a pidfile for this shell interpreter.
#
# \param path Path to the pidfile to create.
#
# \post Installs a cleanup hook to remove the pidfile on exit.
create_pidfile() {
    local path="${1}"; shift

    local pid="$(sh -c 'echo ${PPID}')"
    echo "${pid}" >"${path}" || shtk_cli_error "Cannot create ${path}"
    eval "remove_pidfile() { rm '${path}'; }"
    shtk_cleanup_register remove_pidfile
}


# The main loop to iterate over all configs.
#
# This function does not return.
#
# \param delay Seconds to pause between iterations.
# \param ... Configuration files to process.
the_loop() {
    local delay="${1}"; shift

    local autoconfigs=false
    [ ${#} -gt 0 ] || autoconfigs=true

    while :; do
        shtk_cli_info "Iteration started on $(date)"

        if shtk_bool_check "${autoconfigs}"; then
            set -- $(echo "${AUTOTEST_ETCDIR}/*.conf")
            shtk_cli_info "Discovered config files: ${*}"
        fi

        for config in "${@}"; do
            shtk_process_run "${AUTOTEST_BINDIR}/iterate" -c "${config}" \
                all -q || true
        done

        shtk_cli_info "Iteration finished on $(date)"

        if [ ${delay} -gt 0 ]; then
            shtk_cli_info "Sleeping for ${delay} seconds until next iteration"
            local remaining=${delay}
            while [ ${remaining} -gt 0 ]; do
                sleep 1
                remaining=$((${remaining} - 1))
            done
        fi
    done
}


# Entry point to the program.
#
# \param ... Command-line arguments to be processed.
#
# \return An exit code to be returned to the user.
main() {
    local background=false
    local delay=0
    local logfile=
    local pidfile=

    local OPTIND
    while getopts ':bd:l:p:' arg "${@}"; do
        case "${arg}" in
            b)  # Enter background (daemon) mode.
                [ -n "${_AUTOTEST_BACKGROUNDED}" ] || background=true
                ;;

            d)  # Delay, in seconds, between iterations.
                delay="${OPTARG}"
                ;;

            l)  # Path to the log file; suppresses output.
                [ -n "${_AUTOTEST_BACKGROUNDED}" ] || logfile="${OPTARG}"
                ;;

            p)  # Enable pidfile creation.
                pidfile="${OPTARG}"
                ;;

            \?)
                shtk_cli_usage_error "Unknown option -${OPTARG}"
                ;;
        esac
    done

    if shtk_bool_check "${background}"; then
        if [ -n "${logfile}" ]; then
            _AUTOTEST_BACKGROUNDED=true \
                "$(shtk_cli_dirname)/$(shtk_cli_progname)" "${@}" \
                >"${logfile}" 2>&1 &
        else
            _AUTOTEST_BACKGROUNDED=true \
                "$(shtk_cli_dirname)/$(shtk_cli_progname)" "${@}" &
        fi
    else
        shift $((${OPTIND} - 1))

        [ -z "${logfile}" ] || exec >"${logfile}" 2>&1
        [ -z "${pidfile}" ] || create_pidfile "${pidfile}"
        the_loop "${delay}" "${@}"
    fi
}
