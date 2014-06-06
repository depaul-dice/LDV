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

cd $PERM
rm -rf data
tar xzf $oldpath/data.startup.tgz
cd $oldpath

### start
~/assi/cde/ptu $@ ./exp.sh

### post-process db for indirect (spawn) links
cat 100.log | python insertdb.py
cat 99.log | python insertdb.py
cat 0.log | python insertdb.py
#~/assi/cde/scripts/db2timeline.py -f cde-package/provenance.cde-root.1.log -d gv1 >/dev/null 2>&1
