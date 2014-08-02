#!/bin/sh

for i in `seq 3 -1 1`; do
  export TPCH=$i
  ./test.sh
  mkdir -p tpch$i
  mv time.* tpch$i
done
