#!/bin/sh
rm -rf tpch1/ tpch2/ tpch3/ time.exp*.txt
for i in `seq 3 -1 1`; do
  export TPCH=$i
  echo TPCH=$i
  ./test.sh
  mkdir -p tpch$i
  mv time.* tpch$i
done
