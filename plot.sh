#!/bin/bash

# Set data sources and output files
NEHALEM_DATA=`ls job-benchmark-nehalem.o* | head -n 1`
NEHALEM_PLOT='plot_nehalem.png'

OPTERON_DATA=`ls job-benchmark-opteron.o* | head -n 1`
OPTERON_PLOT='plot_opteron.png'

echo 'Plotting...'
gnuplot -e "data='$NEHALEM_DATA'; title='Nehalem'; set output '$NEHALEM_PLOT'" plot.gp
gnuplot -e "data='$OPTERON_DATA'; title='Opteron'; set output '$OPTERON_PLOT'" plot.gp

echo "Plots written to $NEHALEM_PLOT and $OPTERON_PLOT."
