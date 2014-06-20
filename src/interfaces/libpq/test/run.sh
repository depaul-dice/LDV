#!/bin/sh

### prepare clean db and newest binary
rm -rf cde-package
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
killall psql
bin/pg_ctl stop -D data
rm -rf data
tar xzf $oldpath/data.startup.tgz
cd $oldpath

### start
export PTU_SESSION_ID=1001
unset PTU_DB_REPLAY
rm *.dblog
#~ ./exp.sh
~/assi/cde/ptu $@ ./exp.sh

# prepare minimal database
#~ cd $PERM
#~ bin/pg_ctl stop -D data
#~ rm -rf data
#~ bin/initdb -D data

### post-process db for indirect (spawn) links
cat *.dblog > dblog.txt
#~ cat 100.log | python insertdb.py
#~ cat 99.log | python insertdb.py
#~ cat 0.log | python insertdb.py
#~/assi/cde/scripts/db2timeline.py -f cde-package/provenance.cde-root.1.log -d gv1 >/dev/null 2>&1
