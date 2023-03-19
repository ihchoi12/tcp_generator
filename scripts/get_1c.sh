#!/bin/bash

PLACEMENTS=(i1 i2 d2_11_s)
WORKLOADS=(w0)
ITERATIONS=(50000)
RATES=(1000 3000 5000 7000 9000 11000 13000 15000 17000 19000 21000 23000 25000 27000 29000 31000 33000 35000 37000 39000 41000 43000 45000 47000 49000 51000 53000 55000 57000)

#ITERATIONS=(500 1000)
#RATES=(50000 100000 150000 200000 250000 300000 350000 400000 450000 500000 550000 600000 650000 700000)

PERCENTILE_1="50.0"
PERCENTILE_2="99.0"
PERCENTILE_3="99.9"

rm -rf plot_1c 1>/dev/null 2>/dev/null

for d in ${ITERATIONS[@]}; do
    for w in ${WORKLOADS[@]}; do
        DIR="plot_1c/${d}_${w}"
        mkdir -p $DIR 1>/dev/null 2>/dev/null

        for p in ${PLACEMENTS[@]}; do
            OUTPUT_DIR="output_1c/${p}_${w}_${d}"

            for r in ${RATES[@]}; do
                P5=`cat ${OUTPUT_DIR}/p${PERCENTILE_1}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P9=`cat ${OUTPUT_DIR}/p${PERCENTILE_2}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P99=`cat ${OUTPUT_DIR}/p${PERCENTILE_3}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`

                echo -ne "$r\t$P5\t$P9\t$P99\n" >> $DIR/$p.txt
            done
        done
    done
done
