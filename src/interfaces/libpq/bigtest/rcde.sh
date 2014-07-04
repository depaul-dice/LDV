#!/bin/sh

# fake re-run
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

# prepare empty database
#~ cd $PERM
#~ killall psql >/dev/null 2>&1 
#~ bin/pg_ctl stop -D data >/dev/null 2>&1 
#~ rm -rf data >/dev/null 2>&1 
#~ bin/initdb -D data >/dev/null 2>&1 
#~ chmod -R go-rx data
#~ cd $oldpath

####
#~ remember to add to cde.options
#~ ignore_environment_var=PTU_DB_MODE
#~ ignore_environment_var=PTU_DBSESSION_ID
#~ ignore_environment_var=PTU_DB_REPLAY

### start
export PTU_DB_REPLAY=/dblog.txt
export PTU_DB_MODE=22
#~ ./exp.sh

# real re-run
cd cde-package
sh cde.log
