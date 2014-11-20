#!/bin/sh
for i in 2 3; do
#for j in `seq 1 22`; do
for j in `seq 30 45`; do
echo -n `tar tzvf size.run$i.q$j.tgz | awk '{s+=$3} END{print (s/1024)}'`
if [ $i = 2 ]; then
echo -n "\t" `tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PERM | awk '{s+=$3} END{print (s/1024)}'`
echo -n "\t" `tar tzvf size.run$i.q$j.tgz cde-package/cde-root$PGDATA | awk '{s+=$3} END{print (s/1024)}'`
else
echo -n "\t" 0 "\t" 0
fi
echo "\t" `tar tzvf size.run$i.q$j.tgz --wildcards "*.dblog" | awk '{s+=$3} END{print (s/1024)}'`
done
done
