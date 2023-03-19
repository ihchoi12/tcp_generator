#!/bin/bash

PLACEMENTS=(i4 i8 d4_22_ns d4_22_ns_N d4_13_ns d4_13_ns_N d8_44_s d8_44_ns d8_44_ns_N d8_26_ns d8_26_ns_N)
WORKLOADS=(w0)
ITERATIONS=(50000)
RATES=(10000 20000 30000 40000 50000 60000 70000 80000 90000 100000 110000)

#RATES=(200000 400000 600000 800000 1000000 1200000 1400000 1600000 1800000 2000000)

PERCENTILE_1="50.0"
PERCENTILE_2="99.0"
PERCENTILE_3="99.9"

rm -rf plot_4c 1>/dev/null 2>/dev/null

for d in ${ITERATIONS[@]}; do
    for w in ${WORKLOADS[@]}; do
        DIR="plot_4c/${d}_${w}"
        mkdir -p $DIR 1>/dev/null 2>/dev/null

        for p in ${PLACEMENTS[@]}; do
            OUTPUT_DIR="output_4c/${p}_${w}_${d}"

            for r in ${RATES[@]}; do
                P5=`cat ${OUTPUT_DIR}/p${PERCENTILE_1}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P9=`cat ${OUTPUT_DIR}/p${PERCENTILE_2}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P99=`cat ${OUTPUT_DIR}/p${PERCENTILE_3}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`

                echo -ne "$r\t$P5\t$P9\t$P99\n" >> $DIR/$p.txt
            done
        done
    done
done
