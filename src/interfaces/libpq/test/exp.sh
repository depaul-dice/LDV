#!/bin/sh

# start perm
oldpath=`pwd`
if [ "$PERM" = "" ]
then
	if [ `hostname` = "qpc" ]
	then
	  PERM=/home/quanpt/myapps/perm
	else
	  PERM=/var/tmp/quanpt/froot/perm
	fi
fi
cd $PERM
bin/pg_ctl start -D data --log logfile
cd $oldpath
sleep 3

# start clients
#./single "host=localhost dbname=single" 100 2>100.log &
./single "host=localhost dbname=single" 99 2>99.log &
./single "host=localhost dbname=single" 98 2>100.log &

# start query
sleep 6 && ./query "host=localhost dbname=single" 0 2>0.log

# stop perm
sleep 1
cd $PERM
bin/pg_ctl stop -D data
