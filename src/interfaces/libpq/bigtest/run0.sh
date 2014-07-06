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
rm -rf data 2>/dev/null
tar xzf $oldpath/data.startup.tgz
cd $oldpath

### start
export LD_LIBRARY_PATH=$PERM/lib
unset PTU_DB_REPLAY
export PTU_DB_MODE=0
rm *.dblog

time -p -a -o time.run.txt ./exp.sh
echo time.exp.txt
tail -n 9 time.exp.txt | grep real
