#!/bin/bash

echo "./simulator --ratio=1 -S 2 -f 0.000052 1>4096_filter.res &"
./simulator --ratio=1 -S 2 -f 0.000052 1>4096_filter2.res &

echo "./simulator --ratio=1 -S 4 -f 0.000026 2>8192_filter.res &"
./simulator --ratio=1 -S 4 -f 0.000026 1>8192_filter2.res &

echo "./simulator --ratio=1 -S 8 -f 0.000013 3>16384_filter.res &"
./simulator --ratio=1 -S 8 -f 0.000013 1>16384_filter2.res &

echo "./simulator --ratio=1 -S 16 -f 0.000013 3>32768_filter.res &"
./simulator --ratio=1 -S 8 -f 0.000013 1>32768_filter2.res &


