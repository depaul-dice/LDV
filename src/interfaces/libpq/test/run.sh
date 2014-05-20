#!/bin/sh

### prepare clean db and newest binary
rm -rf cde-package
cp single query

### start
~/assi/cde/ptu $@ ./exp.sh

### post-process db for indirect (spawn) links
cat 100.log | python insertdb.py
cat 99.log | python insertdb.py
~/assi/cde/scripts/db2dot.py -f cde-package/provenance.cde-root.1.log -d gv1 > /dev/null 2>&1
