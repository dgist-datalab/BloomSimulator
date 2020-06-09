#!/bin/bash

#echo "./simulator --ratio=1 -S 2 -f 0.000052 1>4096_filter.res &"
#./simulator --ratio=1 -S 2 -f 0.000052 1>4096_filter3.res &
#
#echo "./simulator --ratio=1 -S 4 -f 0.000026 2>8192_filter.res &"
#./simulator --ratio=1 -S 4 -f 0.000026 1>8192_filter3.res &
#
#echo "./simulator --ratio=1 -S 8 -f 0.000013 3>16384_filter.res &"
#./simulator --ratio=1 -S 8 -f 0.000013 1>16384_filter3.res &
#
#echo "./simulator --ratio=1 -S 16 -f 0.000006 3>32768_filter.res &"
#./simulator --ratio=1 -S 16 -f 0.000006 1>32768_filter3.res &

./simulator --ratio=1 -S 2 -f 0.000052 1>./result/4096_1x.res &
./simulator --ratio=1 -S 4 -f 0.000026 1>./result/8192_1x.res &
./simulator --ratio=1 -S 8 -f 0.000013 1>./result/16384_1x.res &
./simulator --ratio=1 -S 16 -f 0.000006 1>./result/32768_1x.res 

./simulator --ratio=0.025 -S 2 -f 0.000052 1>./result/4096_4x.res &
./simulator --ratio=0.025 -S 4 -f 0.000026 1>./result/8192_4x.res &
./simulator --ratio=0.025 -S 8 -f 0.000013 1>./result/16384_4x.res &
./simulator --ratio=0.025 -S 16 -f 0.000006 1>./result/32768_4x.res 

./simulator --ratio=0.0125 -S 2 -f 0.000052 1>./result/4096_8x.res &
./simulator --ratio=0.0125 -S 4 -f 0.000026 1>./result/8192_8x.res &
./simulator --ratio=0.0125 -S 8 -f 0.000013 1>./result/16384_8x.res &
./simulator --ratio=0.0125 -S 16 -f 0.000006 1>./result/32768_8x.res &

