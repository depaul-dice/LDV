#!/bin/sh

N=1
cwd=`pwd`

rm time.exp.txt time.run.txt

if [ "`grep N= exp.sh`" != "`grep N= rcde3.sh`" ]
then
  echo "Don't bother, check N in exp.sh and rcde3.sh"
  exit
fi

#==== case 3 ====

echo run3.sh
for i in `seq $N`; do
  ###./run3.sh
  true
done
#tar  cf run3.N$N.TPCH$TPCH.tar cde-package *.txt
rm -rf cde-package *.dblog dblog.txt
tar xzf sizepkg/size.run3.q$TPCH.tgz
cp time.exp.txt time.exp.ptumode31.txt
rm time.exp.txt time.run.txt

echo rcde3.sh
rm cde-package/cde-root/$cwd/time.exp.txt
for i in `seq $N`; do
  ./rcde3.sh
  true
done
tar  cf rcde3.N$N.TPCH$TPCH.tar *.txt
mv cde-package/cde-root/$cwd/time.exp.txt time.exp.ptumode32.txt
rm time.exp.txt time.run.txt


#==== case 0 ====
echo normal.sh
for i in `seq $N`; do
  ###./normal.sh
  true
done
mv time.exp.txt time.exp.origin.txt

#==== case 00 - pure ptu ====
echo normal.sh
for i in `seq $N`; do
  ###./ptu.sh
  true
done
mv time.exp.txt time.exp.ptupure.txt

#==== case 2 ====
echo run.sh
for i in `seq $N`; do
  ###./run.sh
  true
done
#tar  cf run.N$N.TPCH$TPCH.tar cde-package *.txt
rm -rf cde-package *.dblog dblog.txt
tar xzf sizepkg/size.run2.q$TPCH.tgz
cp time.exp.txt time.exp.ptumode21.txt
echo > time.exp.txt

echo rcde.sh
rm cde-package/cde-root/$cwd/time.exp.txt
for i in `seq $N`; do
  ./rcde.sh
  true
done
tar  cf rcde.N$N.TPCH$TPCH.tar *.txt
mv cde-package/cde-root/$cwd/time.exp.txt time.exp.ptumode22.txt


#~ echo rcdeconn.sh
#~ for i in `seq 5`; do
  #~ ./rcdeconn.sh
#~ done
#~ mv time.exp.txt time.exp.ptumode22conn.txt
