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

# \file setup.sh
# Configures the current machine as an autotest node.

shtk_import cli
shtk_import config


# List of valid configuration variables.
#
# Please remember to update sysbuild.conf(5) if you change this list.
SETUP_CONFIG_VARS="ATF_REMOTE ATF_REVISION \
                   AUTOTEST_SVNROOT AUTOTEST_REVISION \
                   ROOT \
                   SHTK_REMOTE SHTK_REVISION"


# Paths to installed files.
#
# Can be overriden for test purposes only.
: ${SETUP_ETCDIR="__AUTOTEST_ETCDIR__"}
: ${SETUP_ROOT="__AUTOTEST_ROOT__"}


# Packages needed to bootstrap autotest.
PACKAGES="automake autoconf kyua libtool qemu-devel"


# Sets defaults for configuration variables and hooks that need to exist.
#
# This function should be before the configuration file has been loaded.  This
# means that the user can undefine a required configuration variable, but we let
# him shoot himself in the foot if he so desires.
setup_set_defaults() {
    shtk_config_set ATF_REMOTE "https://github.com/jmmv/atf/"
    shtk_config_set ATF_REVISION "HEAD"
    shtk_config_set AUTOTEST_SVNROOT \
        "svn://svn.FreeBSD.org/base/user/jmmv/autotest/"
    shtk_config_set AUTOTEST_REVISION "HEAD"
    shtk_config_set ROOT "${SETUP_ROOT}"
    shtk_config_set SHTK_REMOTE "https://github.com/jmmv/shtk/"
    shtk_config_set SHTK_REVISION "HEAD"
}


# Prints the source directory of a specific component.
#
# \param component Name of the source component.
srcdir() {
    local component="${1}"; shift

    echo "$(shtk_config_get ROOT)/src/${component}"
}


# Prints the directory of the installed built programs.
localdir() {
    echo "$(shtk_config_get ROOT)/local"
}


# Fetches a repository from git and syncs it to a specific revision.
#
# \param remote Address of the repository.
# \param revision Revision to sync to.
# \param target Directory into which to fetch the sources.
fetch_git() {
    local remote="${1}"; shift
    local revision="${1}"; shift
    local target="${1}"; shift

    if [ ! -d "${target}" ]; then
        mkdir -p "$(dirname "${dir}")"
        git clone "${remote}" "${target}"
    fi

    cd "${target}"
    git fetch
    git checkout "${revision}"
    cd -
}


# Builds a source package.
#
# \param dir Directory in which the source code lives.
build() {
    local dir="${1}"; shift

    (
        cd "${dir}"
        PATH="$(localdir)/bin:$(localdir)/sbin:${PATH}"
        PKG_CONFIG_PATH="$(localdir)/share/pkgconfig:/usr/libdata/pkgconfig"; \
            export PKG_CONFIG_PATH
        autoreconf -is -I "$(srcdir atf)/atf-c" -I "$(srcdir atf)/atf-sh"
        ./configure --prefix "$(localdir)"
        gmake -j4
        gmake install
    )
}


# Dumps the loaded configuration.
#
# \params ... The options and arguments to the command.
setup_config() {
    [ ${#} -eq 0 ] || shtk_cli_usage_error "config does not take any arguments"

    for var in ${SETUP_CONFIG_VARS}; do
        if shtk_config_has "${var}"; then
            echo "${var} = $(shtk_config_get "${var}")"
        else
            echo "${var} is undefined"
        fi
    done
}


# Installs a cron job to periodically run setup.
setup_enable_cron() {
    local dir="$(cd $(shtk_cli_dirname) && pwd)"

    local timespec="30 */1 * * *"
    local entry="( cd '${dir}'"
    entry="${entry}; svnlite update"
    entry="${entry}; make"
    entry="${entry}; ./setup all"
    entry="${entry} ) >/dev/null 2>/dev/null # AUTOTEST"

    crontab -l | awk "
/# AUTOTEST/ {
    next
}

END {
    print \"${timespec} ${entry}\"
}
" | crontab -
}


# Sets up rc.conf to start autotest_node on boot.
setup_enable_daemon() {
    local dir="$(srcdir autotest)/node"

    grep "local_startup.*${dir}/rc.d" /etc/rc.conf \
        || echo "local_startup=\"\${local_startup} ${dir}/rc.d\"" \
        >>/etc/rc.conf
    grep "autotest_node_enable=yes" /etc/rc.conf \
        || echo "autotest_node_enable=yes" >>/etc/rc.conf

    #"${dir}/rc.d/autotest_node" start
}


# Fetches the source code of ATF to have access to its autoconf files.
# TODO(jmmv): Remove once we install the atf-*.m4 files in the base system.
setup_sync_atf() {
    local dir="$(srcdir atf)"

    fetch_git "$(shtk_config_get ATF_REMOTE)" \
        "$(shtk_config_get ATF_REVISION)" "${dir}"
}


# Syncs and builds autotest to a specific revision.
setup_sync_autotest() {
    local dir="$(srcdir autotest)"
    mkdir -p "$(dirname "${dir}")"

    local svnroot="$(shtk_config_get AUTOTEST_SVNROOT)"
    local revision="$(shtk_config_get AUTOTEST_REVISION)"
    if [ -d "${dir}" ]; then
        ( cd "${dir}" && svnlite update -r "${revision}" )
    else
        svnlite co "${svnroot}@${revision}" "$(dirname "${dir}")"
    fi

    make -C "${dir}/node" clean
    make -C "${dir}/node" SHTK="$(localdir)/bin/shtk" all "${@}"
}


# Installs any required packages and ensures they are up-to-date.
setup_sync_packages() {
    pkg update
    pkg install -y ${PACKAGES}
    pkg upgrade -y
}


# Syncs and builds shtk to a specific revision.
setup_sync_shtk() {
    local dir="$(srcdir shtk)"

    fetch_git "$(shtk_config_get SHTK_REMOTE)" \
        "$(shtk_config_get SHTK_REVISION)" "${dir}"
    build "${dir}"
}


# Performs the host setup.
#
# This operation can be run both to set up a new host or to update an existing
# host to newer autotest sources or configurations.
setup_all() {
    setup_sync_packages
    setup_sync_atf
    setup_sync_shtk
    setup_sync_autotest
    setup_enable_daemon
    setup_enable_cron
}


# Program entry.
main() {
    local config_file="${SETUP_ETCDIR}/host.conf"
    local expert_mode=false

    shtk_config_init ${SETUP_CONFIG_VARS}

    local OPTIND
    while getopts ':c:o:X' arg "${@}"; do
        case "${arg}" in
            c)  # Name of the configuration to load.
                config_file="${OPTARG}"
                ;;

            o)  # Override for a particular configuration variable.
                shtk_config_override "${OPTARG}"
                ;;

            X)  # Enable expert-mode.
                expert_mode=true
                ;;

            \?)
                shtk_cli_usage_error "Unknown option -${OPTARG}"
                ;;
        esac
    done
    shift $((${OPTIND} - 1))

    [ ${#} -ge 1 ] || shtk_cli_usage_error "No command specified"

    local exit_code=0

    local command="${1}"; shift
    local function="setup_$(echo ${command} | tr - _)"
    case "${command}" in
        all|config)
            setup_set_defaults
            shtk_config_load "${config_file}"
            "${function}" "${@}" || exit_code="${?}"
            ;;

        enable-cron|enable-daemon|sync-atf|sync-autotest|sync-packages|sync-shtk)
            shtk_bool_check "${expert_mode}" \
                || shtk_cli_usage_error "Using ${command} requires expert" \
                "mode; try passing -X"
            setup_set_defaults
            shtk_config_load "${config_file}"
            "${function}" "${@}" || exit_code="${?}"
            ;;

        *)
            shtk_cli_usage_error "Unknown command ${command}"
            ;;
    esac

    return "${exit_code}"
}
