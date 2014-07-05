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
#./single "host=localhost dbname=single" 100 2>100.log &
./single "host=localhost dbname=single" 99 10000 2>/dev/null &
./single "host=localhost dbname=single" 98 10000 2>/dev/null &

while [ `ps | grep single | wc -c` -ne 0 ]
do
  sleep 1
done

# start query
./query "host=localhost dbname=single" 0

# stop perm
while [ `ps | grep query | wc -c` -ne 0 ]
do
  sleep 1
done
cd $PERM
bin/pg_ctl stop -D data
