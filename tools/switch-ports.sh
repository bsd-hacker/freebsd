#!/bin/sh

cd /c/gnats/gnats-adm
sed -i .bak -e 's/freebsd-ports/freebsd-ports-bugs/' categories responsible

cd /c/gnats
categ="`cat gnats-adm/categories | grep -v '^[[:space:]]*#' | cut -d: -f1`"

for cname in ${categ} ;do
	echo "==> ${cname}"
	cd "${cname}"
	grep -r '^>State:' . | grep -v closed |\
	cut -d: -f1 | cut -d/ -f2 |\
	xargs grep '^>Responsible:[[:space:]][[:space:]]*freebsd-ports$' |\
	cut -d: -f1 | cut -d/ -f2 |\
	xargs echo "sed -i .bak -e '/^>Responsible:[[:space:]][[:space:]]*freebsd-ports/ s/$/-bugs/'"
	cd ..
done
