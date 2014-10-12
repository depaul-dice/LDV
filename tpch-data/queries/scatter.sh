#!/usr/bin/env bash

sourcefile=$1
qcount=0
prefix="q"
suffix="sql"
for i in `grep -n "^\-\-" $sourcefile | cut -d ':' -f 1`; do
        lnum[$qcount]=$i
        let "qcount++"
done
let "qcount--" #the first and last line both start with "--"

qidx=0
while [ "$qidx" -lt "$qcount" ]; do
        let "qidx++"
        let "line_start=lnum[qidx-1]+1"
        let "line_end=lnum[qidx]-1"
        let "ndiff=line_end - line_start + 1"
        newfilename=$prefix$qidx.$suffix
        head -n $line_end $sourcefile |tail -n $ndiff > $newfilename
done