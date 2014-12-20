#!/bin/sh
POS=3
Q1=30
Q2=47
#PERM=/home/quanpt/myapps/perm/
#PGDATA=/home/quanpt/myapps/perm/data/
FLAGS=tvf
echo Query#, PTU package, \
	Open-Source DBS LDV package, Proprietary DBS LDV package, \
	Perm Binary OS, Perm Data OS, PERM Capture OS, PERM Capture P
for j in `seq $Q1 $Q2`; do
#for j in `seq 30 47`; do
i=2
FULLPKG2=`tar $FLAGS size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
PERMBIN2=`tar $FLAGS size.run$i.q$j.tgz cde-package/cde-root$PERM | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
PERMDATA2=`tar $FLAGS size.run$i.q$j.tgz cde-package/cde-root$PGDATA | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
DBCAPTURE2=`tar $FLAGS size.run$i.q$j.tgz --wildcards "*dblog*" | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
i=3
FULLPKG3=`tar $FLAGS size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
PERMDATA3=`tar $FLAGS size.run$i.q$j.tgz cde-package/cde-root$PGDATA | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
DBCAPTURE3=`tar $FLAGS size.run$i.q$j.tgz --wildcards "*.dblog" | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
ERRFROM2=`tar $FLAGS size.run$i.q$j.tgz --wildcards "dblog.txt" | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
NODBPKG=`tar $FLAGS size.run$i.q$j.tgz | awk -v var="$POS" '{s+=$var} END{printf "%d", (s/1024)}'`
#echo $j-$Q1+1, $FULLPKG-$DBCAPTURE, \
#        $FULLPKG-$PERMDATA, $FULLPKG-$PERMDATA-$PERMBIN, \
#        $PERMBIN, $PERMDATA
echo `echo $j-$Q1+1 | bc`, `echo $FULLPKG2-$DBCAPTURE2 | bc`, \
	`echo $FULLPKG2-$PERMDATA2 | bc`, `echo $FULLPKG3-$PERMDATA3-$ERRFROM2 | bc`, \
	$PERMBIN2, $PERMDATA2, $DBCAPTURE2, \
	$DBCAPTURE3
#	#`bc <<< $FULLPKG-$PERMDATA`, $NODBPKG, \
done
