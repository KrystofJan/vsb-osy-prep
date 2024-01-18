#!/bin/bash

angle=0
for h in 00 01 02 03 04 05 06 07 08 09 10 11; do
    for m in 00 10 20 30 40 50; do
        echo $h $m $angle
        convert hours.png -distort SRT $angle hour$h$m.png 
        angle=$[ $angle + 5 ]
    done
done
