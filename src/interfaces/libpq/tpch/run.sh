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
bin/pg_ctl stop 2>/dev/null
killall postgres
rm -rf $PGDATA 2>/dev/null
cp -r $PGDATA.std $PGDATA
cd $oldpath

### start
export PTU_DBSESSION_ID=1001
export PTU_DB_MODE=21
export LD_LIBRARY_PATH=../
unset PTU_DB_REPLAY
rm *.dblog *dblog* 2>/dev/null

NC=`grep real time.exp.txt | wc -l`
NC=`expr $NC + 1`
time -p -a -o time.run.txt $PTUHOME/ptu $@ ./exp.sh
grep real time.exp.txt | tail -n +$NC

### post-process db for indirect (spawn) links
cat *.dblog > dblog.txt
ln dblog.txt cde-package/cde-root/
#~ cat 100.log | python insertdb.py
#~ cat 99.log | python insertdb.py
#~ cat 0.log | python insertdb.py
#~ $PTUHOME/scripts/db2timeline.py -f cde-package/provenance.cde-root.1.log -d gv1 >/dev/null 2>&1
