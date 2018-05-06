#!/bin/sh

set -e

users=
tmpfile="$(mktemp $HOME/tmp.XXXXXXXXXX)"

main() {
	cd $(realpath $(dirname $(basename ${0})))
	userlist="$(ldapsearch -x -b \
		ou=users,dc=freebsd,dc=org \
		-s children \
		'(&(objectClass=freebsdAccount)(cn=*)(uid=*)(sshPublicKey=*)(loginShell=*)(!(loginShell=/usr/sbin/nologin))(!(uid=*test))(!(uid=socsvn-import)))' \
		uid uidNumber loginShell sshPublicKey)"
	printf "${userlist}" > ${tmpfile}
	echo "Output written to: ${tmpfile}"

	./genuserlist.pl ${tmpfile} > ./users.txt

	echo "Final output written to: users.txt"

}

main "$@"
