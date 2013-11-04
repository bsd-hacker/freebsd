#!/bin/sh -e

usage () {
	echo "usage: pkesh gen priv.key pub.key"
	echo "usage: pkesh enc pub.key in out"
	echo "usage: pkesh dec priv.key in out"
	exit 1
}

# gen priv.key pub.key
gen () {
	# Generate the key
	openssl genrsa -out "$D/rsakey" -f4 2048 2>/dev/null

	# Write out private and public parts
	cat "$D/rsakey" > "$1"
	openssl rsa -in "$D/rsakey" -pubout > "$2" 2>/dev/null
}

# enc pub.key in out
enc () {
	# Generate a random 256-bit AES key
	openssl rand 32 > "$D/aeskey"

	# Generate a random 128-bit IV
	openssl rand 16 > "$D/aesIV"

	# Generate the encrypted data
	KEY=`od -An -v -t x1 < "$D/aeskey" | tr -Cd '0-9a-fA-F'`
	IV=`od -An -v -t x1 < "$D/aesIV" | tr -Cd '0-9a-fA-F'`
	openssl enc -aes-256-cbc -K $KEY -iv $IV < "$2" > "$D/encdata"

	# Compute the SHA256 hash of the encrypted data
	openssl dgst -sha256 -binary "$D/encdata" > "$D/hash"

	# Generate the header
	cat "$D/aeskey" "$D/aesIV" "$D/hash" > "$D/header"

	# Generate the encrypted header
	openssl rsautl -inkey "$1" -pubin -encrypt -oaep \
	    < "$D/header" > "$D/encheader"

	# Generate the entire encrypted message
	cat "$D/encheader" "$D/encdata" | openssl enc -base64 > "$3"
}

# dec priv.key in out
dec () {
	# Base-64 decode the encrypted message
	openssl enc -d -base64 < "$2" > "$D/encmessage"

	# Make sure the message is long enough
	if [ `wc -c < "$D/encmessage"` -lt 256 ]; then
		echo "Message is corrupt or truncated" >/dev/stderr
		exit 1
	fi

	# Decrypt the header
	dd if="$D/encmessage" bs=256 count=1 of="$D/encheader" 2>/dev/null
	openssl rsautl -inkey "$1" -decrypt -oaep < "$D/encheader" > "$D/header"

	# Make sure the header is the right size
	if [ `wc -c < "$D/header"` -ne 80 ]; then
		echo "Message is corrupt" >/dev/stderr
		exit 1
	fi

	# Split header into components
	dd if="$D/header" bs=1 count=32 of="$D/aeskey" 2>/dev/null
	dd if="$D/header" bs=1 skip=32 count=16 of="$D/aesIV" 2>/dev/null
	dd if="$D/header" bs=1 skip=48 count=32 of="$D/hash" 2>/dev/null

	# Verify the encrypted data hash
	dd if="$D/encmessage" bs=256 skip=1 2>/dev/null |
	    openssl dgst -sha256 -binary > "$D/encmessage.hash"
	if ! cmp -s "$D/hash" "$D/encmessage.hash"; then
		echo "Message is corrupt or truncated" >/dev/stderr
		exit 1
	fi

	# Decrypt the message
	KEY=`od -An -v -t x1 < "$D/aeskey" | tr -Cd '0-9a-fA-F'`
	IV=`od -An -v -t x1 < "$D/aesIV" | tr -Cd '0-9a-fA-F'`
	dd if="$D/encmessage" bs=256 skip=1 2>/dev/null |
	    openssl enc -d -aes-256-cbc -K $KEY -iv $IV > "$3"
}

# Get operation type
if [ $# -lt 1 ]; then
	usage
fi
OP="$1"
shift

# Check operation type and number of operands
case "$OP" in
gen)
	if [ $# -ne 2 ]; then
		usage
	fi
	;;
enc|dec)
	if [ $# -ne 3 ]; then
		usage
	fi
	;;
*)
	usage
esac

# Create temporary working directory
D=`mktemp -d "${TMPDIR:-/tmp}/pkesh.XXXXXX"`
trap 'rm -r "$D"' EXIT

# Perform the operation
$OP "$@"
