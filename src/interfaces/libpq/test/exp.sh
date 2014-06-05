#!/bin/sh

# start perm
oldpath=`pwd`
cd ~/myapps/perm
bin/pg_ctl start -D data
cd $oldpath
sleep 3

# start clients
./single dbname=single 100 2>100.log &
./single dbname=single 99 2>99.log &

# start query
sleep 6 && ./query dbname=single 0 2>0.log

# stop perm
sleep 1
cd ~/myapps/perm
bin/pg_ctl stop -D data
