#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Not enough args";
    exit 0;
fi

if ! [ -d output_sr  ]; then
    mkdir output_sr;
fi

folder=output_sr/`date "+%F:%H:%m:%S"`

mkdir $folder

./sr -r rtable.EMPTY --verbose -s localhost -v r1 -P 2301 $2 > $folder/1 &

for (( c=2; c<=$1; c++ ))
do
    v=$((2300 + $c))
    ./sr -r rtable.EMPTY -s localhost --verbose -v r$c -P $v > $folder/$c &
done