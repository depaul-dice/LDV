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
export PTU_DBSESSION_ID=1001
export PTU_DB_MODE=31
unset PTU_DB_REPLAY
rm *.dblog
#~ ./exp.sh
~/assi/cde/ptu $@ ./exp.sh

# prepare minimal database
cd cde-package/cde-root/$PERM
bin/pg_ctl stop -D data >/dev/null 2>&1
rm -rf data
$PERM/bin/initdb -D data >/dev/null 2>&1
cd $oldpath

### post-process db for indirect (spawn) links
#~ cat *.dblog > dblog.txt
#~ ln dblog.txt cde-package/cde-root/
#~ cat 100.log | python insertdb.py
#~ cat 99.log | python insertdb.py
#~ cat 0.log | python insertdb.py
#~/assi/cde/scripts/db2timeline.py -f cde-package/provenance.cde-root.1.log -d gv1 >/dev/null 2>&1
