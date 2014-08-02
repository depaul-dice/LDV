#!/bin/sh

### prepare clean db and newest binary
rm -rf cde-package 2>/dev/null
cp single query
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

# prepare original database
cd $PERM
killall psql 2>/dev/null
bin/pg_ctl stop -D data 2>/dev/null
killall postgres
rm -rf data 2>/dev/null
tar xzf $oldpath/data.tpch.notpoked.tgz

rm logfile
bin/pg_ctl start -D data -l logfile
sleep 6

cd $oldpath

### start
export PTU_DBSESSION_ID=1001
export PTU_DB_MODE=21
export LD_LIBRARY_PATH=../
unset PTU_DB_REPLAY
rm *.dblog 2>/dev/null

NC=`grep real time.exp.txt | wc -l`
NC=`expr $NC + 1`
time -p -a -o time.exp.txt ./single "host=localhost dbname=quanpt" 95 1
grep real time.exp.txt | tail -n +$NC

# stop perm
cd $PERM
bin/pg_ctl stop -D data > /dev/null 2>&1
