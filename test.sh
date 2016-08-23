#!/bin/bash
cat /dev/null > test.log
for i in {1..100}
do
echo Test Example  '#' $i >> test.log
make >>test.log 2>&1
echo .>>test.log
echo .>>test.log
echo .>>test.log
done
