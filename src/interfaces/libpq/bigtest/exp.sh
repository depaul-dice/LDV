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
#~ time ./single "host=localhost dbname=single" 99 1000 2>/dev/null &

N=10000
time -p -a -o time.exp.txt ./single "host=localhost dbname=single" 91 $N

time -p -a -o time.exp.txt ./single "host=localhost dbname=single" 92 $N

time -p -a -o time.exp.txt ./single "host=localhost dbname=single" 93 $N

#time -p -a -o time.exp.txt ./single "host=localhost dbname=single" 94 $N

# stop perm
cd $PERM
bin/pg_ctl stop -D data > /dev/null 2>&1
