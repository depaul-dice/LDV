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
#~ bin/pg_ctl start -D data -l logfile > /dev/null 2>&1
#~ cd $oldpath

### start
#~ export PTU_DB_REPLAY=dblog.txt
#~ cp `ls *.dblog | tail -n 1` dblog.txt

export PTU_DB_MODE=32
N=10000

for i in `seq 3`; do
  export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
  echo $PTU_DB_REPLAY $N $i
  time -p ./single "host=localhost dbname=single" 9$i $N
done

#~ ./exp.sh
exit

# real re-run
cd cde-package
sh cde.log
