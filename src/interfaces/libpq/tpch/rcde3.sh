#!/bin/sh
N=100

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

cp *.dblog cde-package/cde-root/$oldpath/
NC=`grep real time.exp.txt | wc -l`
NC=`expr $NC + 1`

export PTU_DB_MODE=32
#~ export LD_LIBRARY_PATH=../

cd cde-package/cde-root/$oldpath

i=1
export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
echo $PTU_DB_REPLAY $N $i
# make one db conn then exit (to restore db if needed)
#~ time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=quanpt" 95 1

i=1
export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
echo $PTU_DB_REPLAY $N $i
# insert
time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=quanpt" 91 $N

i=2
export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
echo $PTU_DB_REPLAY $N $i
# select heavy
time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=quanpt" 92 1 $TPCH

i=3
export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
echo $PTU_DB_REPLAY $N $i
# select light
time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=quanpt" 92 $N $TPCH

i=4
export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
echo $PTU_DB_REPLAY $N $i
# update
time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=quanpt" 93 $N

grep real time.exp.txt | tail -n +$NC
