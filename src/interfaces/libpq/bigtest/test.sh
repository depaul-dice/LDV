#!/bin/sh

rm time.exp.txt time.run.txt
N=100

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
