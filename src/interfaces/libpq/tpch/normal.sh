#!/bin/sh

### prepare clean db and newest binary
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
tar xzf $oldpath/data.tpch.tgz
cd $oldpath

### start
export PTU_DBSESSION_ID=0
export PTU_DB_MODE=0
export LD_LIBRARY_PATH=$PERM/lib
unset PTU_DB_REPLAY
rm *.dblog 2>/dev/null

N=`grep real time.exp.txt | wc -l`
./exp.sh
grep real time.exp.txt | tail -n +$N
