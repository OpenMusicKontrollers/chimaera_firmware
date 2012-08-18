#!/bin/bash

if [ -e fifo ] && [ -p fifo ]
then
	echo 'fifo already exists'
else
	echo 'fifo is being created'
	mkfifo fifo
fi

oscdump 5555 | cut -f 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 -d ' ' - > fifo

