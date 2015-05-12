#!/bin/bash

FILES="4"
MODE_DMA="0 1"
MODE_VECT="0 1 2"
SPU_NUMS="1 2 4 8"

mkdir results

# iterate through files
for i in $FILES;
do
    # iterate over dma modes
    for mode_dma in $MODE_DMA;
    do
        # iterate over vectorization modes
        for mode_vect in $MODE_VECT;
        do
            # iterate over SPU number
            for num in $SPU_NUMS;
            do
                ./ppu/ppu $mode_vect $mode_dma $num /export/asc/tema3_input/in$i.pgm out.cmp out.pgm > results/$i.$mode_dma.$mode_vect.$num.txt
                ../serial/compare cmp out.cmp /export/asc/tema3_output/out$i.cmp >> results/compare.txt
                ../serial/compare pgm out.pgm /export/asc/tema3_output/out$i.pgm >> results/compare.txt
            done
        done
    done
done
