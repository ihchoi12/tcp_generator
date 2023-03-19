#!/bin/bash

PLACEMENTS=(i2 i4 d2_11_ns d2_11_ns_N d4_22_s d4_22_ns d4_22_ns_N)
WORKLOADS=(w0)
ITERATIONS=(50000)
RATES=(1000 3000 5000 7000 9000 11000 13000 15000 17000 19000 21000 23000 25000 27000 29000 31000 33000 35000 37000 39000 41000 43000 45000 47000 49000 51000 53000 55000 57000)

#RATES=(100000 200000 300000 400000 500000 600000 700000 800000 900000 1000000 1100000 1200000)

PERCENTILE_1="50.0"
PERCENTILE_2="99.0"
PERCENTILE_3="99.9"

rm -rf plot_2c 1>/dev/null 2>/dev/null

for d in ${ITERATIONS[@]}; do
    for w in ${WORKLOADS[@]}; do
        DIR="plot_2c/${d}_${w}"
        mkdir -p $DIR 1>/dev/null 2>/dev/null

        for p in ${PLACEMENTS[@]}; do
            OUTPUT_DIR="output_2c/${p}_${w}_${d}"

            for r in ${RATES[@]}; do
                P5=`cat ${OUTPUT_DIR}/p${PERCENTILE_1}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P9=`cat ${OUTPUT_DIR}/p${PERCENTILE_2}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P99=`cat ${OUTPUT_DIR}/p${PERCENTILE_3}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`

                echo -ne "$r\t$P5\t$P9\t$P99\n" >> $DIR/$p.txt
            done
        done
    done
done
