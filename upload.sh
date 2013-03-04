#!/bin/sh -e
set -e

# usage: sh -e upload.sh

# Load configuration
. build.conf

# Upload files
echo "`date`: Starting upload"
( cd ${STATEDIR}/stage && tar -cf- bp f s t *.gz *.ssl ) |
    ssh -i ${UPLOAD_KEY} ${UPLOAD_ACCT} tar -xf- -C ${UPLOAD_DIR}

# Remove files which we've uploaded
find ${STATEDIR}/stage -type f | xargs rm

# Report success
echo "`date`: Finished"

