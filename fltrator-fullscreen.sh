#!/bin/sh

module=xrandr_change_screen_res
local=0
if [ $(echo ${0} | cut -c1-2) = "./" ]; then
local=1
prefix="./"
fi

ARGS=$1

if [ $local ]; then
echo "compiling $module module..."
gcc -o $module src/$module.c -lX11 -lXrandr
if [ $? = 0 ]; then
if [ "$ARGS" = "--install" ]; then
echo Installing ...
DEST=~/bin/$(echo ${0} | rev | cut -c 4- | rev)
echo cp "$0" "$DEST"
cp "$0" "$DEST"
chmod 0777 "$DEST"
echo cp "$module" ~/bin/"$module"
cp "$module" ~/bin/.
exit 0
fi
fi
fi

# switch to 800x600
"$prefix"$module 800x600

ORIG_MODE=$?
echo "original screen mode was $ORIG_MODE"

# start in fullscreen mode, no cursor
"$prefix"fltrator $ARGS -f -c -e

# change back to original mode
"$prefix"$module $ORIG_MODE
