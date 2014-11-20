#!/bin/sh

filename=testsize.txt
rm $filename time*.txt
mkdir -p sizepkg

for i in `seq 30 47`; do 
  export TPCH=$i
  
  rm *.dblog dblog.txt
  ./run.sh
  tar czf size.run2.q$i.tgz cde-package *.dblog dblog*
  mv size.run2.q$i.tgz sizepkg/
  echo run2.q$i | tee -a $filename
  du -s cde-package/cde-root/$PQDATA cde-package/ >> $filename
  wc -c *.dblog | grep total >> $filename
  echo "===" >> $filename
  
  rm *.dblog dblog.txt
  ./run3.sh
  tar czf size.run3.q$i.tgz cde-package *.dblog dblog*
  mv size.run3.q$i.tgz sizepkg/
  echo run3.q$i | tee -a $filename
  du -s cde-package/cde-root/$PQDATA cde-package/ >> $filename
  wc -c *.dblog | grep total >> $filename
  echo "===" >> $filename
done

mv time.exp.txt sizepkg/
