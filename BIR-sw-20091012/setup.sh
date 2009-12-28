#!/bin/bash

aclocal
autoconf
autoheader
automake -a
./configure --enable-CPU
