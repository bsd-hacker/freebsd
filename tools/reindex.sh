#!/bin/sh

genindex=/usr/local/libexec/gnats/gen-index
tmpindex=/home/gnats/gnats-adm/!$$!index
index=/home/gnats/gnats-adm/index

if ${genindex} -n >${tmpindex} ; then
    mv ${tmpindex} ${index}
else
    rm ${tmpindex}
    echo Failed
fi

