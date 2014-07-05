#!/bin/sh

rm time.exp.txt time.run.txt

if [ "`grep N= exp.sh`" != "`grep N= rcde3.sh`" ]
then
  echo "Don't bother, check N in exp.sh and rcde3.sh"
  exit
fi

N=10

#==== case 3 ====

for i in `seq $N`; do
  ./run3.sh
done
mv time.exp.txt time.exp.ptumode31.txt
mv time.run.txt time.run.ptumode31.txt


for i in `seq $N`; do
  ./rcde3.sh
done
mv time.exp.txt time.exp.ptumode32.txt
mv time.run.txt time.run.ptumode32.txt

exit
#==== case 2 ====
for i in `seq $N`; do
  ./run0.sh
done
mv time.exp.txt time.exp.origin.txt
mv time.run.txt time.run.origin.txt


for i in `seq $N`; do
  ./run.sh
done
cp time.exp.txt time.exp.ptumode21.txt
echo > time.exp.txt
mv time.run.txt time.run.ptumode21.txt


for i in `seq $N`; do
  ./rcde.sh
done
mv time.exp.txt time.exp.ptumode22.txt
mv time.run.txt time.run.ptumode22.txt
