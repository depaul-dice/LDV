#!/bin/sh

export PERM=/home/quanpt/myapps/perm

export PGDATA=$PERM/data
curpwd=`pwd`

rm -rf $PGDATA
$PERM/bin/initdb
$PERM/bin/pg_ctl start -l logfile
sleep 5
$PERM/bin/createdb gitdb
$PERM/bin/psql -d gitdb -f preparedb.sql
$PERM/bin/psql -d gitdb -c "COPY description FROM '$curpwd/chicago_landmark.csv' WITH CSV DELIMITER ',';" 
$PERM/bin/psql -d gitdb -c "COPY landmarks FROM '$curpwd/chicago_landmark_gis_id.csv' WITH CSV DELIMITER '|';"
$PERM/bin/pg_ctl stop

cd $PERM
tar czf data.gitdb.tgz data
mv data.gitdb.tgz $curpwd/
cd $curpwd