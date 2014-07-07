#!/bin/sh

N=5

rm time.exp.txt time.run.txt

if [ "`grep N= exp.sh`" != "`grep N= rcde3.sh`" ]
then
  echo "Don't bother, check N in exp.sh and rcde3.sh"
  exit
fi

#==== case 3 ====

echo run3.sh
for i in `seq $N`; do
  ./run3.sh
done
cp time.exp.txt time.exp.ptumode31.txt
mv time.run.txt time.run.ptumode31.txt
rm time.exp.txt time.run.txt

echo rcde3.sh
rm cde-package/cde-root/home/quanpt/assi/perm/src/interfaces/libpq/bigtest/time.exp.txt
for i in `seq $N`; do
  ./rcde3.sh
done
mv cde-package/cde-root/home/quanpt/assi/perm/src/interfaces/libpq/bigtest/time.exp.txt time.exp.ptumode32.txt
mv time.run.txt time.run.ptumode32.txt
rm time.exp.txt time.run.txt


#==== case 0 ====
echo run0.sh
for i in `seq $N`; do
  ./run0.sh
done
mv time.exp.txt time.exp.origin.txt
mv time.run.txt time.run.origin.txt


#==== case 2 ====
echo run.sh
for i in `seq $N`; do
  ./run.sh
done
cp time.exp.txt time.exp.ptumode21.txt
echo > time.exp.txt
mv time.run.txt time.run.ptumode21.txt


echo rcdeconn.sh
for i in `seq ${N}0`; do
  ./rcdeconn.sh
done
mv time.exp.txt time.exp.ptumode22conn.txt

echo rcde.sh
for i in `seq $N`; do
  ./rcde.sh
done
mv time.exp.txt time.exp.ptumode22.txt
mv time.run.txt time.run.ptumode22.txt
