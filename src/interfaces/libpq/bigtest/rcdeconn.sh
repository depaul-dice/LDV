#!/bin/sh

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

# prepare minimal database
cd cde-package/cde-root/$PERM
bin/pg_ctl stop -D data >/dev/null 2>&1
rm -rf data
$PERM/bin/initdb -D data >/dev/null 2>&1
cd $oldpath

####
#~ remember to add to cde.options
#~ ignore_environment_var=PTU_DB_MODE
#~ ignore_environment_var=PTU_DBSESSION_ID
#~ ignore_environment_var=PTU_DB_REPLAY

### start
export PTU_DB_REPLAY=/dblog.txt
export PTU_DB_MODE=22

# real re-run
ln dbconnect.sh cde-package/cde-root/$oldpath/ 2>/dev/null
rm cde-package/cde-root/$oldpath/single
ln single cde-package/cde-root/$oldpath/ 2>/dev/null
cd cde-package/cde-root/$oldpath
$oldpath/cde-package/cde-exec ./dbconnect.sh
tail -n 3 time.exp.txt | grep real
