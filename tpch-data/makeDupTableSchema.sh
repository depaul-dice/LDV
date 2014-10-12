#!/bin/bash
# Create a modified TCP-H schema with several replicas of relation lineitem

# check input
if [ ! $# = 2 ]
then
    echo "wrong number of parameters."
    echo "correct syntax is:"
    echo "makeDupTableSchema.sh dbsize numberOfReplicas"
    exit 1
fi

# Get parameters
dbSize=${1}
numRepl=${2}
inFullPath="templates/pre.sql"
lineItemPath="templates/lineitemone.sql"
outFile="ddlrep_r${numRepl}_s${dbSize}.sql.in"
outPath="scriptIn/${outFile}"

# apply checks
if [ ! -f ${inFullPath} ]
then
   echo "could not find file ${inFullPath}"
   exit 1
fi

# createScripts
echo "create script ${outPath}"

cat ${inFullPath} | sed -e "s/DBSIZE/${dbSize}/g" > ${outPath}

for (( i=1; i<=${numRepl}; i++ ))
do
    cat ${lineItemPath} | sed -e "s/DBSIZE/${dbSize}/g" | sed -e "s/REPLICA/${i}/g" >> ${outPath}
done
