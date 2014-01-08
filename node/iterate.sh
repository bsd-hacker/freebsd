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

# \file iterate.sh
# Creates a FreeBSD release and executes its tests within a VM.

shtk_import bool
shtk_import cleanup
shtk_import cli
shtk_import config
shtk_import process


# List of valid configuration variables.
#
# Please remember to update sysbuild.conf(5) if you change this list.
AUTOTEST_CONFIG_VARS="CHROOTDIR DATADIR IMAGE MKVARS SRCBRANCH SVNROOT \
                      TARGET TARGET_ARCH"


# Paths to installed files.
#
# Can be overriden for test purposes only.
: ${AUTOTEST_ETCDIR="__AUTOTEST_ETCDIR__"}
: ${AUTOTEST_ROOT="__AUTOTEST_ROOT__"}


# Sets defaults for configuration variables and hooks that need to exist.
#
# This function should be before the configuration file has been loaded.  This
# means that the user can undefine a required configuration variable, but we let
# him shoot himself in the foot if he so desires.
autotest_set_defaults() {
    # Please remember to update autotest(1) if you change any default values.
    shtk_config_set CHROOTDIR "${AUTOTEST_ROOT}/head/build"
    shtk_config_set DATADIR "${AUTOTEST_ROOT}/data"
    shtk_config_set IMAGE "${AUTOTEST_ROOT}/image.disk"
    shtk_config_set SRCBRANCH "base/head"
    shtk_config_set SVNROOT "svn://svn.freebsd.org"
    shtk_config_set TARGET "amd64"
    shtk_config_set TARGET_ARCH "amd64"
}


# Dumps the loaded configuration.
#
# \params ... The options and arguments to the command.
autotest_config() {
    [ ${#} -eq 0 ] || shtk_cli_usage_error "config does not take any arguments"

    for var in ${AUTOTEST_CONFIG_VARS}; do
        if shtk_config_has "${var}"; then
            echo "${var} = $(shtk_config_get "${var}")"
        else
            echo "${var} is undefined"
        fi
    done
}


# Prints the contents of the src.conf file.
#
# The output of this is the collection of variables and their values as defined
# in MKVARS.
_generate_src_conf() {
    for varvalue in $(shtk_config_get MKVARS); do
        echo "${varvalue}"
    done
}


# Builds a full release for later testing.
autotest_release() {
    [ ${#} -eq 0 ] || shtk_cli_usage_error "release does not take any arguments"

    local tmpdir="$(mktemp -d -t autotest)"
    eval "remove_tmpdir() { rm -rf '${tmpdir}'; }"
    shtk_cleanup_register remove_tmpdir

    local src_conf="${tmpdir}/src.conf"
    _generate_src_conf >"${src_conf}"

    local release_conf="${tmpdir}/release.conf"
    cat >"${release_conf}" <<EOF
CHROOTDIR="$(shtk_config_get CHROOTDIR)"
MAKE_FLAGS=  # Disable default silent mode.
NODOC=yes
NODVD=yes
NOPORTS=yes
SRCBRANCH="$(shtk_config_get SRCBRANCH)"
SRC_CONF="${src_conf}"
SVNROOT="$(shtk_config_get SVNROOT)"
TARGET="$(shtk_config_get TARGET)"
TARGET_ARCH="$(shtk_config_get TARGET_ARCH)"
EOF

    local svnroot="$(shtk_config_get SVNROOT)/$(shtk_config_get SRCBRANCH)"
    shtk_cli_info "Fetching release.sh from ${svnroot}"
    local release_sh="${tmpdir}/release.sh"
    svn export "${svnroot}/release/release.sh" "${release_sh}" \
        || shtk_cli_error "Fetch of release.sh failed"
    chmod +x "${release_sh}"

    local ret=0
    shtk_process_run "${release_sh}" -c "${release_conf}" || ret=${?}
    rm -rf "${tmpdir}"; eval "remove_tmpdir() { true; }"
    [ ${ret} -eq 0 ] || shtk_cli_error "release build failed"
}


# Creates a disk image with an fresh FreeBSD installation to run Kyua.
#
# This uses the results of autotest_release, which must have been invoked
# beforehand.
#
# When run, the image will automatically log in as root, execute the full test
# suite from /usr/tests using Kyua, and shut the system down.
autotest_mkimage() {
    [ ${#} -eq 0 ] || shtk_cli_usage_error "mkimage does not take any arguments"

    local chrootdir="$(shtk_config_get CHROOTDIR)"
    local image="$(shtk_config_get IMAGE)"

    rm -f "${image}"
    touch "${image}"
    truncate -s 4G "${image}"

    cleanup_mkimage() {
        # This is our function to clean up any system-wide resources that we
        # acquire during the process.  Because we cannot get all of these
        # atomically, and because the order in which they are released matters,
        # we redefine this cleanup function as we go to cover all the needed
        # details.
        true
    }
    shtk_cleanup_register cleanup_mkimage

    shtk_cli_info "Preparing image"
    local mddev="$(mdconfig -a -t vnode -f "${image}")"
    eval "cleanup_mkimage() {
        mdconfig -d -u '${mddev}' || true;
    }"
    gpart create -s gpt "${mddev}"
    gpart add -t freebsd-boot -s 512k -l bootfs "${mddev}"
    gpart bootcode -b "${chrootdir}/boot/pmbr" -p "${chrootdir}/boot/gptboot" \
        -i 1 "${mddev}"
    gpart add -t freebsd-swap -s 1G -l swapfs "${mddev}"
    gpart add -t freebsd-ufs -l rootfs "${mddev}"
    newfs "${mddev}p3"

    shtk_cli_info "Mounting image file systems"
    mkdir -p "${chrootdir}/vmimage/mnt"
    eval "cleanup_mkimage() {
        umount '${chrootdir}/vmimage/mnt' || true; \
        mdconfig -d -u '${mddev}' || true; \
        umount '${chrootdir}/dev' || true; \
    }"
    mount "/dev/${mddev}p3" "${chrootdir}/vmimage/mnt"
    mount -t devfs devfs "${chrootdir}/dev"

    shtk_cli_info "Installing system into image"
    _generate_src_conf >"${chrootdir}/etc/src.conf"
    chroot "${chrootdir}" make -s -C /usr/src \
        DESTDIR=/vmimage/mnt \
        TARGET="$(shtk_config_get TARGET)" \
        TARGET_ARCH="$(shtk_config_get TARGET_ARCH)" \
        installworld installkernel distribution 2>&1 >/dev/null

    shtk_cli_info "Setting up image configuration"
    echo "-h" >"${chrootdir}/vmimage/mnt/boot.config"
    sed -i .tmp \
        '/^ttyv.*$/s/on/off/;/^ttyu0.*$/s/off/on/;/^ttyu0.*$/s/dialup/xterm/' \
        "${chrootdir}/vmimage/mnt/etc/ttys"
    sed -i .tmp '/^default:/s/:/:al=root:/' \
        "${chrootdir}/vmimage/mnt/etc/gettytab"

    cat >"${chrootdir}/vmimage/mnt/etc/fstab" <<EOF
/dev/gpt/rootfs / ufs rw 2 2
/dev/gpt/swapfs none swap sw 0 0
EOF

    cp /etc/resolv.conf "${chrootdir}/vmimage/mnt/etc"
    pkg -c "${chrootdir}/vmimage/mnt" install -y kyua
    rm "${chrootdir}/vmimage/mnt/etc/resolv.conf"

    cat >>"${chrootdir}/vmimage/mnt/root/.cshrc" <<EOF
cd /usr/tests
/usr/local/bin/kyua test
shutdown -p now
EOF

    shtk_cli_info "Unmounting image file systems"
    cleanup_mkimage
    eval "cleanup_mkimage() { true; }"  # Prevent double-execution on exit.
}


# Runs the image built by autotest_mkimage.
autotest_execute() {
    [ ${#} -eq 0 ] || shtk_cli_usage_error "execute does not take any arguments"

    local chrootdir="$(shtk_config_get CHROOTDIR)"
    local image="$(shtk_config_get IMAGE)"

    # TODO(jmmv): Add support for bhyve.  Keep in mind that we must continue to
    # support qemu so that we can test non-amd64 platforms from our test cluster
    # machines.  In other words: the selection of the VMM has to be exposed in
    # the configuration file.
    local target_arch="$(shtk_config_get TARGET_ARCH)"
    case "${target_arch}" in
        amd64)
            shtk_process_run qemu-system-x86_64 -nographic \
                -drive file="${image}"
            ;;

        i386)
            shtk_process_run qemu-system-i386 -nographic \
                -drive file="${image}"
            ;;

        *)
            shtk_cli_error "Sorry, don't know how to run tests for" \
                "${target_arch}"
            ;;
    esac
}


# Extracts the test results from the image and puts them in the given directory.
#
# \param datadir Path to the directory to hold the results of this particular
#     run.  This will contain the raw Kyua database as well as the formatted
#     HTML output.
autotest_publish() {
    [ ${#} -eq 1 ] || shtk_cli_usage_error "publish expects one argument"
    local datadir="${1}"; shift

    local chrootdir="$(shtk_config_get CHROOTDIR)"
    local image="$(shtk_config_get IMAGE)"

    shtk_cli_info "Extracting test results from image"
    local mddev="$(mdconfig -a -t vnode -f "${image}")"
    mount -o ro "/dev/${mddev}p3" "${chrootdir}/vmimage/mnt"
    mkdir -p "${datadir}"
    cp "${chrootdir}/vmimage/mnt/root/.kyua/store.db" "${datadir}/store.db"
    umount "${chrootdir}/vmimage/mnt"
    mdconfig -d -u "${mddev}"

    shtk_process_run /usr/local/bin/kyua report-html \
        --output="${datadir}/results" \
        --store="${datadir}/store.db" \
        --results-filter=
}


# Performs a single release plus test execution pass.
#
# The full log of this is logged into DATADIR.
autotest_all() {
    local quiet=false
    local OPTIND
    while getopts ':q' arg "${@}"; do
        case "${arg}" in
            q)  # Suppress all output.
                quiet=true
                ;;

            \?)
                shtk_cli_usage_error "Unknown option -${OPTARG}"
                ;;
        esac
    done
    shift $((${OPTIND} - 1))

    [ ${#} -eq 0 ] || shtk_cli_usage_error "all does not take any arguments"

    local timestamp=$(date +%Y%m%d-%H%M%S)
    local datadir="$(shtk_config_get DATADIR)/${timestamp}"

    mkdir -p "${datadir}"
    touch "${datadir}/output.log"
    if ! shtk_bool_check "${quiet}"; then
        tail -f "${datadir}/output.log" &
        local tail_pid="${!}"
        eval "kill_tail() { kill '${tail_pid}'; }"
        shtk_cleanup_register kill_tail
    fi
    exec >>"${datadir}/output.log" 2>&1

    (
        autotest_release
        autotest_mkimage
        autotest_execute
        autotest_publish "${datadir}"
    )
    ln -sf "${timestamp}" "$(shtk_config_get DATADIR)/0-LATEST"

    exec >&- 2>&-
    if ! shtk_bool_check "${quiet}"; then
        kill "${tail_pid}"
        eval "kill_tail() { true; }"
    fi
}


# Loads the configuration file specified in the command line.
#
# \param config_name Name of the desired configuration.  It can be either a
#     configuration name (no slashes) or a path.
autotest_config_load() {
    local config_name="${1}"; shift

    local config_file=
    case "${config_name}" in
        */*|*.conf)
            config_file="${config_name}"
            ;;

        *)
            config_file="${AUTOTEST_ETCDIR}/${config_name}.conf"
            [ ! -e "${config_file}" ] \
                || shtk_cli_usage_error "Cannot locate configuration named" \
                "'${config_name}'"
            ;;
    esac
    shtk_config_load "${config_file}"
}


# Entry point to the program.
#
# \param ... Command-line arguments to be processed.
#
# \return An exit code to be returned to the user.
main() {
    local config_file="${AUTOTEST_ETCDIR}/head.conf"
    local expert_mode=false

    shtk_config_init ${AUTOTEST_CONFIG_VARS}

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
    case "${command}" in
        all|config)
            autotest_set_defaults
            shtk_config_load "${config_file}"
            "autotest_${command}" "${@}" || exit_code="${?}"
            ;;

        execute|mkimage|publish|release)
            shtk_bool_check "${expert_mode}" \
                || shtk_cli_usage_error "Using ${command} requires expert" \
                "mode; try passing -X"
            autotest_set_defaults
            shtk_config_load "${config_file}"
            "autotest_${command}" "${@}" || exit_code="${?}"
            ;;

        *)
            shtk_cli_usage_error "Unknown command ${command}"
            ;;
    esac

    return "${exit_code}"
}
