#!/bin/sh
POS=3
Q1=30
Q2=47
#PERM=/home/quanpt/myapps/perm/
#PGDATA=/home/quanpt/myapps/perm/data/

echo Query#, PTU package, Open-Source DBS LDV package, Proprietary DBS LDV package, Perm Binary, Perm Data
for j in `seq $Q1 $Q2`; do
#for j in `seq 30 45`; do
i=2
FULLPKG=`tar tzvf size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{print (s/1024)}'`
PERMBIN=`tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PERM | awk -v var="$POS" '{s+=$var} END{print (s/1024)}'`
PERMDATA=`tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PGDATA | awk -v var="$POS" '{s+=$var} END{print (s/1024)}'`
DBCAPTURE=0
#DBCAPTURE=`tar tzvf size.run$i.q$j.tgz --wildcards "*.dblog" | awk -v var="$POS" '{s+=$var} END{print (s/1024)}'`
i=3
NODBPKG=`tar tzvf size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{print (s/1024)}'`
echo `bc <<< $j-$Q1+1`, `bc <<< $FULLPKG-$DBCAPTURE`, \
	`bc <<< $FULLPKG-$PERMDATA`, `bc <<< $FULLPKG-$PERMDATA-$PERMBIN`, \
	$PERMBIN, $PERMDATA
	#`bc <<< $FULLPKG-$PERMDATA`, $NODBPKG, \
done
