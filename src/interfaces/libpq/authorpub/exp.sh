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
bin/pg_ctl start -D data -l logfile > /dev/null 2>&1
cd $oldpath
sleep 3

# start clients
python ./prepareData.py
python ./dispatch.py
python ./queryData.py

# stop perm
sleep 1
cd $PERM
bin/pg_ctl stop -D data
