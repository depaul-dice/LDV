#!/bin/sh
POS=3
Q1=30
Q2=47
#PERM=/home/quanpt/myapps/perm/
#PGDATA=/home/quanpt/myapps/perm/data/

echo Query#, PTU package, Open-Source DBS LDV package, Proprietary DBS LDV package, Perm Binary, Perm Data
for j in `seq $Q1 $Q2`; do
#for j in `seq 30 47`; do
i=2
FULLPKG=`tar tzvf size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
PERMBIN=`tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PERM | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
PERMDATA=`tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PGDATA | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
DBCAPTURE=0
DBCAPTURE=`tar tzvf size.run$i.q$j.tgz --wildcards "*.dblog" | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
i=3
NODBPKG=`tar tzvf size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
#echo $j-$Q1+1, $FULLPKG-$DBCAPTURE, \
#        $FULLPKG-$PERMDATA, $FULLPKG-$PERMDATA-$PERMBIN, \
#        $PERMBIN, $PERMDATA
echo `echo $j-$Q1+1 | bc`, `echo $FULLPKG-$DBCAPTURE | bc`, \
	`echo $FULLPKG-$PERMDATA | bc`, `echo $FULLPKG-$PERMDATA-$PERMBIN | bc`, \
	$PERMBIN, $PERMDATA
#	#`bc <<< $FULLPKG-$PERMDATA`, $NODBPKG, \
done
