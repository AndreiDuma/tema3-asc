#!/bin/bash

mkdir plots

gnuplot -e "data='results/res.1.txt'; title='in1.pgm'; set output 'plots/plot.1.png'" plot.gp
gnuplot -e "data='results/res.2.txt'; title='in2.pgm'; set output 'plots/plot.2.png'" plot.gp
gnuplot -e "data='results/res.3.txt'; title='in3.pgm'; set output 'plots/plot.3.png'" plot.gp
gnuplot -e "data='results/res.4.txt'; title='in4.pgm'; set output 'plots/plot.4.png'" plot.gp
