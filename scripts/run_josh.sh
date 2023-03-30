#!/bin/bash

SEED_LIST=(1646203793 986508091 193720917 15335381 633882127 1093215650 772188468 711307909 645856549 1127581467 765061083 1050115427 4231379 1000215989 1382853168 1927405477 306097907 1344972625 2098183364 323989894)
PLACEMENTS=(i6 d7_16_ns)
WORKLOADS=("stridedmem:1024:7")

#DELAYS=(50000)
#RATES=(5000 7000 9000 11000 13000 15000 17000 19000 21000 23000 25000 27000 29000 31000 33000 35000 37000 39000 41000 43000 45000 47000 49000 51000 53000 55000 57000 59000 61000)

DELAYS=(30000)
RATES=(50000 75000 100000 125000 150000 175000 200000 225000 250000 275000 300000 325000 350000 375000 400000 425000 450000 475000 500000 525000 550000 575000 600000)

#DELAYS=(10000)
#RATES=(50000 100000 150000 200000 250000 300000 350000 400000 450000 500000)

#DELAYS=(1000)
#RATES=(100000 150000 200000 250000 300000 350000 400000 450000 500000 550000 600000 650000 700000 750000 800000 850000 900000 950000 1000000 1050000 1100000 1150000 1200000)

CLI_DURATION=10
PERCENTILE_1="50.0"
PERCENTILE_2="99.0"
PERCENTILE_3="99.9"

PCI_NIC="d8:00.1"
CLI_CORES="3,5,7,9,11,13,15,17,19"
SERVER_IP="10.90.0.21"

OUTPUT_FILE="output.dat"
SRV_TIMEOUT=30
TIMEOUT=$(( SRV_TIMEOUT - CLI_DURATION - CLI_DURATION ))

error () {
    local Z=1.96
    local N=`wc -l $1 | cut -d' ' -f1`

    MEAN=`awk '{sum += $1} END {printf "%.4f", (sum/NR)}' $1`
    STDEV=`awk '{sum += ($1 - '$MEAN')^2} END {printf "%.4f", sqrt(sum/'$N')}' $1`
    ERROR=`awk 'BEGIN {printf "%.4f", '$Z' * '$STDEV'/sqrt('$N')}'`
}

process() {
    local N=`wc -l $1 | cut -d' ' -f1`
    echo $N > .tmp
    cat $1 >> .tmp
    echo -ne "$3\t$4\t$N\n" >> $2/n_lines.txt
    ../percentile ${PERCENTILE_1} .tmp >> $2/percentiles_${PERCENTILE_1}.txt
    ../percentile ${PERCENTILE_2} .tmp >> $2/percentiles_${PERCENTILE_2}.txt
    ../percentile ${PERCENTILE_3} .tmp >> $2/percentiles_${PERCENTILE_3}.txt
}

echo 3 | sudo tee /proc/sys/vm/drop_caches 1>/dev/null 2>/dev/null

date

for w in ${WORKLOADS[@]}; do
    for d in ${DELAYS[@]}; do
        for p in ${PLACEMENTS[@]}; do
            f="cfgs/josh/${w}/${d}/${p}.cfg"
            if [ ! -f $f ]; then
                continue
            fi

            . $f

            STOP=0

            for r in ${RATES[@]}; do
                if [ ${STOP} == 1 ]; then
                    break
                fi

                DIR="results_josh/${w}/${d}/${p}/$r"
                rm -rf $DIR 1>/dev/null 2>/dev/null
                mkdir -p $DIR 1>/dev/null 2>/dev/null

                COMPLETED=0

                for j in `seq 0 $RUNS`; do
                    echo -ne "$w DELAY=$d $p RATE=$r -- $j/$RUNS -- "

                    # Run the server
                    SRV_ARGS="${SRV_HTs} ${SRV_APPLICATION}"
                    ssh ${SERVER_IP} "cd ${SRV_ARCH}/demikernel; sh ./run_server.sh '${SRV_CORES}' '${SRV_ARGS}'" 1>/dev/null 2>/dev/null &

                    # Sleep a while for finish the server
                    sleep 4
                    SEED=${SEED_LIST[j]}

                    # Run the generator
                    cd .. 1>/dev/null 2>/dev/null
                    sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu timeout 35 ./build/tcp-generator --iova-mode=pa --no-telemetry -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -n ${CLI_SERVERS} -t ${CLI_DURATION} -q ${CLI_QUEUES} -i ${SRV_ITERATIONS} -j ${SRV_DISTRIBUTION} -c addr.cfg -o ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
                    cd - 1>/dev/null 2>/dev/null

                    # Sleep a while
                    sleep ${TIMEOUT}
                    sleep 2

                    # Process the result
                    if [ ! -f ../${OUTPUT_FILE} ]; then
                        echo -ne "\n"
                        continue
                    fi

                    mv -f ../${OUTPUT_FILE} . 1>/dev/null 2>/dev/null
                    EXP_N=$(( r * CLI_DURATION ))
                    REAL_N=`wc -l ${OUTPUT_FILE} | cut -d' ' -f1`

                    echo -ne "${REAL_N}/${EXP_N}\n"

                    if [[ ${REAL_N} -ne ${EXP_N} ]]; then
                        continue
                    fi

                    COMPLETED=$(( COMPLETED + 1 ))
                    process ${OUTPUT_FILE} $DIR $r $j
                done

                if [[ ${COMPLETED} == 0 ]]; then
                    STOP=1
                else
                    OUTPUT_DIR="output_josh/${w}/${d}/${p}"
                    mkdir -p ${OUTPUT_DIR} 1>/dev/null 2>/dev/null

                    error $DIR/percentiles_${PERCENTILE_1}.txt
                    echo -ne "$r\t$MEAN\t$ERROR\t$STDEV\n" >> ${OUTPUT_DIR}/p${PERCENTILE_1}.txt
                    error $DIR/percentiles_${PERCENTILE_2}.txt
                    echo -ne "$r\t$MEAN\t$ERROR\t$STDEV\n" >> ${OUTPUT_DIR}/p${PERCENTILE_2}.txt
                    error $DIR/percentiles_${PERCENTILE_3}.txt
                    echo -ne "$r\t$MEAN\t$ERROR\t$STDEV\n" >> ${OUTPUT_DIR}/p${PERCENTILE_3}.txt
                fi
            done
        done
    done
done

rm -rf .tmp 1>/dev/null 2>/dev/null
rm -rf ${OUTPUT_FILE} 1>/dev/null 2>/dev/null

for w in ${WORKLOADS[@]}; do
    for d in ${DELAYS[@]}; do
        DIR="plot_josh/${w}/${d}/"
        #rm -rf $DIR 1>/dev/null 2>/dev/null
        mkdir -p $DIR 1>/dev/null 2>/dev/null

        for p in ${PLACEMENTS[@]}; do
            OUTPUT_DIR="output_josh/${w}/${d}/${p}"

            for r in ${RATES[@]}; do
                P5=`cat ${OUTPUT_DIR}/p${PERCENTILE_1}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P9=`cat ${OUTPUT_DIR}/p${PERCENTILE_2}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`
                P99=`cat ${OUTPUT_DIR}/p${PERCENTILE_3}.txt | grep -P "^$r\t" | cut -d$'\t' -f2,3`

                echo -ne "$r\t$P5\t$P9\t$P99\n" >> $DIR/$p.txt
            done
        done
    done
done

rm -rf results_josh 1>/dev/null 2>/dev/null
rm -rf output_josh 1>/dev/null 2>/dev/null

echo 3 | sudo tee /proc/sys/vm/drop_caches 1>/dev/null 2>/dev/null

date
