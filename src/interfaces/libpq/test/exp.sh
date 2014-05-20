#!/bin/sh
./single dbname=single 100 2>100.log &
./single dbname=single 99 2>99.log &
sleep 6 && ./query dbname=single 0 2>0.log
