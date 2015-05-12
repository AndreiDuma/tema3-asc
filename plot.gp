set terminal png

set grid
set title title
set xlabel 'SPUs'
set ylabel 'time'
set key right top

plot data using 1:2 title "Single Buffering, Scalar" with lines,                \
     data using 1:3 title "Single Buffering, Vectors" with lines,               \
     data using 1:4 title "Single Buffering, Vectors + Intrinsics" with lines,  \
     data using 1:5 title "Double Buffering, Scalar" with lines,                \
     data using 1:6 title "Double Buffering, Vectors" with lines,               \
     data using 1:7 title "Double Buffering, Vectors + Intrinsics" with lines

