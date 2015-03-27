#!/bin/sh

### prepare clean db and newest binary
rm -rf cde-package *.dblog output.kml
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
echo "=== Prepare db ==="
cd $PERM
killall psql
bin/pg_ctl stop -D data
rm -rf data logfile
tar xzf $oldpath/data.gitdb.tgz
cd $oldpath

### start
export PTU_DBSESSION_ID=1001
export PTU_DB_MODE=21
export LD_LIBRARY_PATH=../
unset PTU_DB_REPLAY

echo "=== Run experiment script ==="
#./exp.sh
~/assi/cde/ptu $@ ./exp.sh

echo "=== Done experiment script ==="

# prepare minimal database
echo "=== Prepare db for package ==="
cd cde-package/cde-root/$PERM
bin/pg_ctl stop -D data >/dev/null 2>&1
rm -rf data
$PERM/bin/initdb -D data >/dev/null 2>&1
chmod -R go-rx data
cd $oldpath

### post-process db for indirect (spawn) links
echo "=== Post-process leveldb and perm ==="
cat *.dblog > dblog.txt
ln dblog.txt cde-package/cde-root/
cat dblog.txt | python insertdb.py
#~ cat 100.log | python insertdb.py
#~ cat 99.log | python insertdb.py
#~ cat 0.log | python insertdb.py
# ./db2timeline.py -f cde-package/provenance.cde-root.1.log -d gv2 >/dev/null 2>&1
# ./db2dot.py -f cde-package/provenance.cde-root.1.log -d gv1 > /dev/null 2>&1

echo "=== Done packaging ==="