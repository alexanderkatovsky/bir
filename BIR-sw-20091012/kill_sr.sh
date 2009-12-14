#!/bin/bash

get_procs=`ps -e -o pid,command | grep "./sr" | gawk -F" " '{ print $1 }' | tr "\n" " "`

kill -9 $get_procs 2>/dev/null



