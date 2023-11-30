#
# Stacked histograms
#
set terminal pdf enhanced
set output 'write-ops.pdf'
set title "BeeGFS Worker Thread (avg. 100 ops)"

# Where to put the legend
# and what it should contain
#set key invert reverse Left outside
set key autotitle columnheader

set yrange [0:*]
set ylabel "Time (usec)"

# Define plot style 'stacked histogram'
# with additional settings
set style data histogram
set style histogram rowstacked
set style fill solid border -1
set boxwidth 0.75

# We are plotting columns 2, 3 and 4 as y-values,
# the x-ticks are coming from column 1
plot 'write-ops.data' using 2:xtic(1) \
    ,'' using 5 \
    ,'' using 6

