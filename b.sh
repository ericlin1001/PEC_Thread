#!/bin/bash
dir=../Run/$RANDOM
logFile=runLog.log
mpic++ -DUSE_MPI main.cpp
echo Run:$dir
mkdir $dir
cp -r . $dir/
echo sleep 3
sleep 3
echo Start Run...........................
cd $dir
echo "mpirun -f machinefile -n $1 ./a.out|tee $logFile" > RunCmd.txt
mpirun -f machinefile -n $1 ./a.out|tee $logFile
echo "Results are saved in $dir"

