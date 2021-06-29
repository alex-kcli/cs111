#!/bin/bash

echo | ./lab4b --bogus 2> /dev/null
if [[ $? -eq 1 ]]
then
    echo "Correct: Bogus option recognized."
else
    echo "Error: Failed to recognize bogus option."
fi


./lab4b --period=20 --scale=F --log=temp.txt <<-EOF
SCALE=C
PERIOD=10
STOP
START
LOG mylog
OFF
EOF

if [[ $? -eq 0 ]]
then
    echo "Correct: Correct return code for valid option."
else
    echo "Error: Incorrect return code for valid option."
fi

grep "SCALE=C" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: SCALE command."
else
    echo "Error: SCALE command."
fi

grep "PERIOD=10" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: PERIOD command."
else
    echo "Error: PERIOD command."
fi

grep "STOP" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: STOP command."
else
    echo "Error: STOP command."
fi

grep "START" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: START command."
else
    echo "Error: START command."
fi

grep "LOG mylog" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: LOG command."
else
    echo "Error: LOG command."
fi

grep "OFF" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: OFF command."
else
    echo "Error: OFF command."
fi

grep "SHUTDOWN" temp.txt 1> /dev/null
if [[ $? -eq 0 ]]
then
    echo "Correct: SHUTDOWN printed."
else
    echo "Error: SHUTDOWN printed."
fi

rm -f temp.txt
