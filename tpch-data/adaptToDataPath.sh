#!/bin/bash
echo $#
if [ $# -eq 0 ] 
then
    echo "give path as parameter"
    exit 1
fi

# get output path and escape it
outPath=${1}
outPath=`echo ${outPath} | sed -e 's/\\//\\\\\\//g'`
echo ${outPath}
# replace path in sql files
for script in `find scriptIn -name \*.sql.in | sed -e 's/scriptIn\///g'` 
do
    echo "adapt ${script}"
    outscript=`echo ${script} | sed -e 's/.in//g'`
    sed -e "s/DATAPATH\//${outPath}/g" scriptIn/${script} > scripts/${outscript}
done
