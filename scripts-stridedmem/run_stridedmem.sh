#!/bin/bash

SEED_LIST=(1646203793 986508091 193720917 15335381 633882127 1093215650 772188468 711307909 645856549 1127581467 765061083 1050115427 4231379 1000215989 1382853168 1927405477 306097907 1344972625 2098183364 323989894)
PLACEMENTS=(i1 i2)
WORKLOADS=(w0)
DELAYS=(50000)
RATES=(1000 3000 5000 7000 9000 11000 13000 15000 17000 19000 21000 23000 25000 27000 29000 31000 33000 35000 37000 39000 41000 43000)

CLI_DURATION=10
PERCENTILE_1="50.0"
PERCENTILE_2="99.0"
PERCENTILE_3="99.9"

PCI_NIC="ca:00.0"
CLI_CORES="19,21,23,25,27,29,31,33,35,37,39"
SERVER_IP="130.127.134.50"

OUTPUT_FILE="output.dat"
SRV_TIMEOUT=25
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

ssh ${SERVER_IP} "echo 0 | sudo tee /proc/sys/kernel/nmi_watchdog" 1>/dev/null 2>/dev/null

for p in ${PLACEMENTS[@]}; do
    for w in ${WORKLOADS[@]}; do
        f="cfgs/stridedmem/${p}_${w}.cfg"
        if [ ! -f $f ]; then
            continue
        fi

        . $f

        for d in ${DELAYS[@]}; do
            STOP=0

            for r in ${RATES[@]}; do
                if [ ${STOP} == 1 ]; then
                    break
                fi

                DIR="results_stridedmem/${p}_${w}_${d}/$r"
                rm -rf $DIR 1>/dev/null 2>/dev/null
                mkdir -p $DIR 1>/dev/null 2>/dev/null

                for j in `seq 0 $RUNS`; do
                    echo "$p $w $d $r $j/$RUNS"

                    # Run the server
                    SRV_ARGS="${SRV_HTs} ${SRV_APPLICATION} ${SRV_DISTRIBUTION} $d"
                    ssh ${SERVER_IP} "cd ${SRV_ARCH}/demikernel; sh ./run_server.sh '${SRV_CORES}' '${SRV_ARGS}'" 1>/dev/null 2>/dev/null &

                    # Sleep a while for finish the server
                    sleep 4
                    SEED=${SEED_LIST[j]}

                    # Run the generator
                    cd .. 1>/dev/null 2>/dev/null
                    sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu timeout 35 ./build/tcp-generator --iova-mode=pa --no-telemetry -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -n ${CLI_SERVERS} -t ${CLI_DURATION} -q ${CLI_QUEUES} -c addr.cfg -o ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
                    cd - 1>/dev/null 2>/dev/null

                    # Sleep a while
                    sleep ${TIMEOUT}
                    sleep 2

                    # Process the result
                    if [ ! -f ../${OUTPUT_FILE} ]; then
                        continue
                    fi
                    mv -f ../${OUTPUT_FILE} . 1>/dev/null 2>/dev/null
                    EXP_N=$(( (r * CLI_DURATION * 99) / 100 ))
                    REAL_N=`wc -l ${OUTPUT_FILE} | cut -d' ' -f1`

                    if [[ ${REAL_N} -le ${EXP_N} ]]; then
                        STOP=1
                    break

                    fi
                    ssh ${SERVER_IP} "sudo pkill -9 perf" 1>/dev/null 2>/dev/null
                    scp ${SERVER_IP}:~/inline/demikernel/output.perf output$j.perf 1>/dev/null 2>/dev/null
                    mv output$j.perf $DIR 1>/dev/null 2>/dev/null


                    process ${OUTPUT_FILE} $DIR $r $j
                    echo 3 | sudo tee /proc/sys/vm/drop_caches 1>/dev/null 2>/dev/null
                done

                if [[ ${STOP} == 0 ]]; then
                    OUTPUT_DIR="output_stridedmem/${p}_${w}_${d}"
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

ssh ${SERVER_IP} "echo 1 | sudo tee /proc/sys/kernel/nmi_watchdog" 1>/dev/null 2>/dev/null

rm -rf .tmp 1>/dev/null 2>/dev/null
rm -rf ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
