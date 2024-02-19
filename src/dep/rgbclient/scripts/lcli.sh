#!/bin/sh
cd $(dirname `find . -name rgbclient.c`)
ls
./rgbclient $@ \
    --led-rows=64 \
    --led-cols=64 \
    --led-chain=5 \
    --led-parallel=2 \
    --led-slowdown-gpio=4 \
    --led-gpio-mapping=regular \
    --led-limit-refresh=60
