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

# make a connection and restore db
time -p -a -o time.exp.txt ./single "host=localhost dbname=single" 95 1

# stop perm
cd $PERM
bin/pg_ctl stop -D data > /dev/null 2>&1
