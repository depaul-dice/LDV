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

echo "=== Start perm ==="
cd $PERM
bin/pg_ctl start -D data -l logfile > /dev/null 2>&1
cd $oldpath
sleep 3

# start clients
echo "=== Start experiment ==="
python ./extractkml.py
#/usr/bin/google-earth $PWD/output.kml >/dev/null 2>&1
#rm $PWD/output.kml

# stop perm

echo "=== Stop perm ==="
sleep 1
cd $PERM
bin/pg_ctl stop -D data

echo "=== Done experiment ==="
