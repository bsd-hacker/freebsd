#!/bin/sh

REGION_TO=$1
while read REGION_FROM SZ AMI; do
	if [ ${REGION_FROM} = ${REGION_TO} ]; then
		continue;
	fi
	echo "Copying ${AMI} from ${REGION_FROM} to ${REGION_TO}..."

	AMI_FREEBSD=`aws ec2 copy-image --region ${REGION_TO}		\
	    --source-region ${REGION_FROM} --source-image-id ${AMI}	\
	    --output text`
	while aws ec2 describe-images --region ${REGION_TO} \
	    --image-ids ${AMI_FREEBSD} | grep -q pending; do
		sleep 15;
	done
	echo "${REGION_TO} ${SZ} ${AMI_FREEBSD}" >> ami.log
done < ami-built.log
