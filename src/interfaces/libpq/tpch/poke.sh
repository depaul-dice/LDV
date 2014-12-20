#!/bin/sh

export PERM=/local/perm/install/aggperm
export DBNAME=tpch_1
export PGDATA=/local/perm/cluster/qperm
export PTUBIN=/home/quan/ptu/ptu

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
rm -rf $PGDATA 2>/dev/null
cp -r $PGDATA.std $PGDATA

rm logfile
bin/pg_ctl start -D $PGDATA -l logfile
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
time -p -a -o time.exp.txt ./single "host=localhost dbname=$DBNAME" 95 1
grep real time.exp.txt | tail -n +$NC

# stop perm
cd $PERM
bin/pg_ctl stop -D $PGDATA > /dev/null 2>&1
