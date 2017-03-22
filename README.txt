#
# $FreeBSD$
#

1) Configuration Files:

   - Each architecture and individual kernel have their own configuration file
     used by release.sh.  Each branch has its own defaults-X.conf file which
     contains entries common throughout each architecture, where overrides or
     special variables are set and/or overridden in the per-build files.

     The per-build configuration file naming scheme is in the form of:

     ${revision}-${TARGET_ARCH}-${KERNCONF}-${type}.conf

     where the uppercase variables are equivalent to what make(1) uses in the
     build system, and lowercase variables are set within the configuration
     files, mapping to the major version of the respective branch.

   - Each branch also has its own builds-X.conf file, which is used by
     thermite.sh.  The thermite.sh script iterates through each ${revision},
     ${TARGET_ARCH}, ${KERNCONF}, and ${type} value, creating a master list
     of what to build.  However, a given combination from the list will only
     be built if the respective configuration file exists, which is where the
     naming convention above is relevant.

   - There are two paths of file sourcing:

     - builds-X.conf -> main.conf:
       This controls thermite.sh behavior.

     - X-arch-KERNCONF-type.conf -> defaults-X.conf -> main.conf
       This controls release/release.sh behavior.

2) Filesystem Layout:

   - The official release build machines have a specific filesystem layout,
     which using ZFS, thermite.sh takes heavy advantage of with clones,
     snapshots, etc., ensuring a pristine build environment.

   - The build scripts reside in /releng/scripts-snapshot/scripts or
     /releng/scripts-release/scripts respectfully, to avoid collisions between
     an RC build from a releng branch versus a STABLE snapshot from the
     respective stable branch.

   - A separate dataset exists for the final build images, /snap/ftp.  This
     directory contains both snapshots and releases directories.  They are
     only used if the EVERYTHINGISFINE variable is defined in main.conf.

   - As thermite.sh iterates through the master list of combinations and
     locates the per-build configuration file, a zfs dataset is created under
     the /releng directory, such as /releng/12-amd64-GENERIC-snap.  The src,
     ports, and doc trees are checked out to separate zfs datasets, such as
     /releng/12-src-snap, which are then cloned into the respective build
     datasets.  This is done to avoid checking out a given tree more than
     once.

3) Helper Scripts:

   - To avoid repetition and possible human error, a few scripts were written
     to help keep things as automated as possible:

     - zfs-setup.sh:
       Destroys and creates pristine zfs datasets for each build.

     - setrev.sh:
       Retrieves the 'Last Changed Revision' from the target branch and writes
       the version to a 'svnrev_src' file, and outputs the date in YYYYMMDD
       format to a builddate file.

     - get-checksums.sh:
       When all builds have completed, this script will iterate through and
       generate a list of sha512 and sha256 checksums for all builds.

     - generate-email.pl:
       This script generates the snapshot announcement email text.

4) Example Usage:

   root@builder:~ # mkdir -p /releng/scripts-snapshot/scripts
   root@builder:~ # cd /releng/scripts-snapshot/scripts
   root@builder:~ # svn co svn://svn.freebsd.org/base/user/gjb/thermite .
   root@builder:~ # ./zfs-setup.sh -c ./builds-12.conf
   root@builder:~ # ./setrev.sh -b head
   root@builder:~ # ./thermite.sh -c ./builds-12.conf
   root@builder:~ # ./get-checksums.sh -c ./builds-12.conf | ./generate-email.pl \
                    > 12-snap-mail

