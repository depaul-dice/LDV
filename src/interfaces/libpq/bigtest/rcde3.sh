#!/bin/sh
N=1000

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

export PTU_DB_MODE=32
export LD_LIBRARY_PATH=../

cd cde-package/cde-root/$oldpath
for i in `seq 3`; do
  export PTU_DB_REPLAY=`ls 1001.*.dblog | sort -n | head -n $i | tail -n 1`
  echo $PTU_DB_REPLAY $N $i
  time -p -a -o time.exp.txt $oldpath/cde-package/cde-exec ./single "host=localhost dbname=single" 9$i $N
done
echo time.exp.txt
tail -n 12 time.exp.txt | grep real



