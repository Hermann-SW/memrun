#!/bin/sh
dn=$(dirname "$0")
sudo cp -f  "$dn"/grun /usr/local/bin
sudo ln -sf grun /usr/local/bin/g++
sudo ln -sf grun /usr/local/bin/gcc
