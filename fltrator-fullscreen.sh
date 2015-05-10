#!/bin/sh

module=xrandr_change_screen_res
local=0
if [ $(echo ${0} | cut -c1-2) = "./" ]
then
local=1
prefix="./"
fi

ARGS=$1

if [ $local ]
then
echo "compiling $module module..."
gcc -o $module $module.cxx -lX11 -lXrandr -lstdc++
fi

# switch to 800x600
"$prefix"$module 800x600

ORIG_MODE=$?
echo "original screen mode was $ORIG_MODE"

# start in fullscreen mode, no cursor
"$prefix"fltrator $1 -f -c -e

# change back to original mode
"$prefix"$module $ORIG_MODE
