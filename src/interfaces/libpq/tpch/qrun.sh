#!/bin/sh

### prepare clean db and newest binary
rm -rf cde-package

### start
$PTUHOME/ptu $@ ./query dbname=single -1

### post-process db for indirect (spawn) links
$PTUHOME/scripts/db2dot.py -f cde-package/provenance.cde-root.1.log -d gv1 > /dev/null 2>&1
