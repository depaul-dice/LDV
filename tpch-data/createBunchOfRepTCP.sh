#!/bin/bash

dbsizes=${1}
replicas=${2}
mypath=`echo $0 | sed -e 's/\(.*\)\/.*/\1/g'`
for i in ${dbsizes}
do
    for j in ${replicas}
    do
	${mypath}/makeDupTableSchema.sh ${i} ${j}
    done
done