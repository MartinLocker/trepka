#!/bin/sh

DIR=`dirname $0`

cd $DIR

last=`ls -t ToePunch/builds/v*/firmware.version | head -1 | cut -d/ -f1-3`

mv $last/firmware.version $last/version

ls -l $last

cp -pr $last ../web/download/fw/
