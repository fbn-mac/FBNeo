#!/bin/bash
if [ ! -f makefile.burn_rules ]; then
    echo "Run this script from the root of FBNeo source" >&2
    exit 1
fi

if [ -z "$1" ]; then
    echo "Missing remote hostname" >&2
    exit 1
fi

trap "exit" INT

DIRNAME=`basename \`pwd\``
RHOST=$1
FLAVOR=$2

rsync \
    -avR \
    --exclude 'roms' \
    --exclude '.*' \
    --exclude 'fbneo' \
    --exclude 'fbahelpfilesrc/' \
    --exclude 'makefile.burner_win32_rules' \
    --exclude 'makefile.mamemingw' \
    --exclude 'makefile.mingw' \
    --exclude 'makefile.pi' \
    --exclude 'makefile.sdl' \
    --exclude 'makefile.vc' \
    --exclude 'obj' \
    --exclude 'projectfiles/' \
    --exclude 'sync.sh' \
    --exclude '*.a' \
    --exclude '*.chm' \
    --exclude '*.html' \
    --exclude '*.md' \
    --exclude '*.o' \
    --exclude '*.so.1' \
    --exclude '*.yml' \
    --exclude '*.zip' \
    --exclude '*/macos/' \
    --exclude '*/psp/' \
    --exclude '*/qt/' \
    --exclude '*/resource/' \
    --exclude '*/win32/' \
    --exclude '*/kaillera/' \
    --exclude '*/mingw/' \
    --exclude '*/qtcreator/' \
    --exclude '*/vc/' \
    --exclude '*/vs2010/' \
    --exclude '*/gles/' \
    --exclude '*/opengl/' \
    --exclude '*/video/sdl/' \
    --exclude '*/dep/generated/' \
    . ${RHOST}:${DIRNAME}

if [ -n "$FLAVOR" ]; then
    echo "Building..."
    ssh ${RHOST} -t "cd ${DIRNAME} && make ${FLAVOR} && cd src/dep/rgbclient/ && make"
fi
