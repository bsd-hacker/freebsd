#!/bin/sh

UNIDATA=/home/edwin/unicode/UNIDATA/5.2.0/UnicodeData.txt
CHARMAPS=/home/edwin/svn/edwin/locale/tools/charmaps
UTF8=~/unicode/cldr/1.7.1/posix/UTF-8.cm

if [ -z "$1" ]; then
	echo "Usage: $0 <unicode string>"
	exit
fi

UCS=$*
UCS_=$(echo $* | sed -e 's/ /./g')
echo UCS: ${UCS}

echo UTF-8.cm:
grep "${UCS_}" ${UTF8} | sed -e 's/   */	/g'

echo UNIDATA:
grep "${UCS_}" ${UNIDATA}
L=$(grep "${UCS_}" ${UNIDATA})

echo UCC:
grep "${UCS_}" ${UNIDATA} | awk -F\; '{ print $1 }'


echo CHARMAPS:
grep ${UCS_} ${CHARMAPS}/* | sed -e "s|${CHARMAPS}/||g"
grep ${UCC} ${CHARMAPS}/* | sed -e "s|${CHARMAPS}/||g"
