#!/bin/bash

angle=0
for m in `seq 0 59`; do
    mm=`printf %02d $m`
    echo $mm $angle
    convert minutes.png -distort SRT $angle minute$mm.png 
    angle=$[ $angle + 6 ]
done
