#!/bin/sh
#
echo -e "\
0) []= Tiled Layout\n\
1) [M] Monocle Layout\n\
2) [\\] Dwindle Layout\n\
3) HHH Grid Layout\n\
4) ### N-Row Grid Layout\n\
5) --- Horizontal Grid Layout\n\
6) ::: Gapless Grid Layout\n\
7) |M| Centered Master Layout\n\
8) >M> Centered Floating Master Layout\n\
9) ><> Floating Layout\
" | dmenu -c -l 10 -p "LAYOUT:"
