#!/bin/sh

export PERM=/local/perm/install/aggperm
export DBNAME=tpch_1
export PGDATA=/local/perm/cluster/qperm
export PTUBIN=/home/quan/ptu/ptu

rm -rf time.exp*.txt
#for i in `seq 3 -1 1`; do
i=30
  export TPCH=$i
  echo TPCH=$i
  ./poke.sh
