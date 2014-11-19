#!/bin/sh

export PERM=/local/perm/install/aggperm
export DBNAME=tpch_0_1
export PGDATA=/local/perm/cluster/qperm
export PTUBIN=/home/quan/ptu/ptu

rm -rf tpch1/ tpch2/ tpch3/ time.exp*.txt
#for i in `seq 3 -1 1`; do
for i in `seq 30 33`; do
  export TPCH=$i
  echo TPCH=$i
  ./test.sh
  rm -rf tpch$i
  mkdir -p tpch$i
  mv time.* tpch$i
done
