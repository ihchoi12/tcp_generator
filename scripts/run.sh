#!/bin/bash

SEED_LIST=(1646203793 986508091 193720917 15335381 633882127 1093215650 772188468 711307909 645856549 1127581467 765061083 1050115427 4231379 1000215989 1382853168 1927405477 306097907 1344972625 2098183364 323989894)

CORES=("1c-constant")
# CORES=("2c-constant")
# CORES=("4c-constant")
# CORES=("8c-constant")
# CORES=("8c-bimodal")
# CORES=("8c-exponential")

PLACEMENTS=(i1 i2 i4 i8 i16)
# PLACEMENTS=(d2_11 d4_13 d8_17)
# PLACEMENTS=(d8_17_N d8_26 d8_26_N d8_44 d8_44_N)
# PLACEMENTS=(d16_88 d16_88_ns d16_214 d16_412)

MODES=("50")
# MODES=("80")
# MODES=("995")

WORKLOADS=("stridedmem:11534336:7")
# WORKLOADS=("sqrt")




CLI_DURATION=5
PERCENTILE_1="50.0"
PERCENTILE_3="99.9"
PCI_NIC="d8:00.1"
CLI_CORES="3,5,7,9,11,13,15,17,19"
SERVER_IP="10.90.0.21"
SERVER_DIR="~/SOSP"
OUTPUT_FILE="output.dat"

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
    ../percentile ${PERCENTILE_3} .tmp >> $2/percentiles_${PERCENTILE_3}.txt
}

echo 3 | sudo tee /proc/sys/vm/drop_caches 1>/dev/null 2>/dev/null

for c in ${CORES[@]}; do
    for w in ${WORKLOADS[@]}; do
        for d in ${MODES[@]}; do
            PLOT_DIR="plot/${c}/${w}/${d}/"
            mkdir -p ${PLOT_DIR} 1>/dev/null 2>/dev/null

            for p in ${PLACEMENTS[@]}; do
                f="cfgs/${c}/${w}/${d}/${p}.cfg"
                if [ ! -f $f ]; then
                    continue
                fi

                . $f
                
                STOP=0
                r=${CLI_RATE_START}

                while [ ${STOP} -eq 0 ]; do
                    DIR="results1/${w}/${d}/${p}/$r"
                    rm -rf $DIR 1>/dev/null 2>/dev/null
                    mkdir -p $DIR 1>/dev/null 2>/dev/null

                    COMPLETED=0

                    for j in `seq 0 $RUNS`; do
                        sudo rm -rf /dev/hugepages/rtemap_1*; sudo rm -rf /dev/hugepages/rtemap_2*; sudo rm -rf /dev/hugepages/rtemap_3*; sudo rm -rf /dev/hugepages/rtemap_* 1>/dev/null 2>/dev/null            

                        ssh ${SERVER_IP} "sudo pkill -9 timeout; sudo killall -9 tcp-echo-multiflow.elf; sudo pkill -9 perf; sudo rm -rf /dev/hugepages/rtemap_2*; sudo rm -rf /dev/hugepages/rtemap_3*; sudo rm -rf /dev/hugepages/rtemap_4*; sudo rm -rf /dev/hugepages/rtemap_5*; sudo rm -rf /dev/hugepages/rtemap_6*; sudo rm -rf /dev/hugepages/rtemap_7*; sudo rm -rf /dev/hugepages/rtemap_8*; sudo rm -rf /dev/hugepages/rtemap_9*; sudo rm -rf /dev/hugepages/rtemap_10*; sudo rm -rf /dev/hugepages/rtemap_11* ; sudo rm -rf /dev/hugepages/rtemap_12* ; sudo rm -rf /dev/hugepages/rtemap_13*; sudo rm -rf /dev/hugepages/rtemap_*" 1>/dev/null 2>/dev/null

                        echo -ne "$w DELAY=$d $p RATE=$r -- $j/$RUNS -- "

                        # Run the server
                        SRV_ARGS="${SRV_HTs} ${SRV_APPLICATION}"
                        #ssh ${SERVER_IP} "cd SOSP/${SRV_ARCH}/demikernel; sh ./run_server.sh '${SRV_CORES}' '${SRV_ARGS}'" 1>/dev/null 2>/dev/null &
                        ssh ${SERVER_IP} "cd SOSP/${SRV_ARCH}/demikernel; sh ./run_server.sh '${SRV_CORES}' '${SRV_ARGS}'" &

                        # Sleep a while for finish the server
                        sleep 5
                        SEED=${SEED_LIST[j]}

                        # Run the generator
                        cd .. 1>/dev/null 2>/dev/null
                        if [ ${SRV_DISTRIBUTION} == "bimodal" ]; then
                            #sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu ./build/tcp-generator -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -t ${CLI_DURATION} -q ${CLI_QUEUES} -D ${SRV_DISTRIBUTION} -i ${SRV_ITERATIONS0} -j ${SRV_ITERATIONS1} -m ${SRV_MODE} -c addr.cfg -o ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
                            sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu ./build/tcp-generator -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -t ${CLI_DURATION} -q ${CLI_QUEUES} -D ${SRV_DISTRIBUTION} -i ${SRV_ITERATIONS0} -j ${SRV_ITERATIONS1} -m ${SRV_MODE} -c addr.cfg -o ${OUTPUT_FILE}
                        else 
                            #sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu ./build/tcp-generator -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -t ${CLI_DURATION} -q ${CLI_QUEUES} -D ${SRV_DISTRIBUTION} -i ${SRV_ITERATIONS} -c addr.cfg -o ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
                            sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu ./build/tcp-generator -a ${PCI_NIC} -n 4 -l ${CLI_CORES} -- -d ${CLI_DISTRIBUTION} -r $r -f ${CLI_FLOWS} -s ${CLI_SIZE} -e ${SEED} -t ${CLI_DURATION} -q ${CLI_QUEUES} -D ${SRV_DISTRIBUTION} -i ${SRV_ITERATIONS} -c addr.cfg -o ${OUTPUT_FILE}
                        fi

                        cd - 1>/dev/null 2>/dev/null

                        ssh ${SERVER_IP} "sudo pkill -9 timeout; sudo killall -9 tcp-echo-multiflow.elf; sudo pkill -9 perf" 1>/dev/null 2>/dev/null 

                        # Process the result
                        if [ ! -f ../${OUTPUT_FILE} ]; then
                            echo -ne "ERROR - Do not produce the output file\n"
                            break
                        fi

                        mv -f ../${OUTPUT_FILE} . 1>/dev/null 2>/dev/null
                        EXP_N=$(( r * CLI_DURATION ))
                        REAL_N=`wc -l ${OUTPUT_FILE} | cut -d' ' -f1`

                        echo -ne "${REAL_N}/${EXP_N} "
                        if [[ ${REAL_N} -ne ${EXP_N} ]]; then
                            echo -ne "ERROR - The number of line is not the expected\n"
                            break
                        fi
                        echo -ne "\n"

                        COMPLETED=$(( COMPLETED + 1 ))
                        process ${OUTPUT_FILE} $DIR $r $j

                        scp ${SERVER_IP}:${SERVER_DIR}/${SRV_ARCH}/demikernel/output.perf output$j.perf 1>/dev/null 2>/dev/null
                        PERF_DIR="perf/${c}/${w}/${d}/${p}/$r"
                        mkdir -p ${PERF_DIR} 1>/dev/null 2>/dev/null
                        mv output$j.perf ${PERF_DIR} 1>/dev/null 2>/dev/null
                        RAW_DIR="raw/${c}/${w}/${d}/${p}/$r"
                        mkdir -p ${RAW_DIR} 1>/dev/null 2>/dev/null
                        mv -f ${OUTPUT_FILE} ${RAW_DIR}/${OUTPUT_FILE}$j 1>/dev/null 2>/dev/null
                    done

                    OUTPUT_DIR="results2/${w}/${d}/${p}"
                    mkdir -p ${OUTPUT_DIR} 1>/dev/null 2>/dev/null

                    if [[ $COMPLETED == $(( RUNS + 1 )) ]]; then
                        error $DIR/percentiles_${PERCENTILE_1}.txt
                        echo -ne "$r\t$MEAN\t$ERROR" >> ${PLOT_DIR}/$p.txt
                        error $DIR/percentiles_${PERCENTILE_3}.txt
                        echo -ne "\t$MEAN\t$ERROR\n" >> ${PLOT_DIR}/$p.txt
                    else
                        STOP=1
                        echo -ne "$r\t99999999\t0\t99999999\t0\t99999999\t0\n" >> ${PLOT_DIR}/$p.txt
                    fi

                    r=$(( r + CLI_RATE_JUMPS ))
                done
            done
        done
    done
done

rm -rf .tmp 1>/dev/null 2>/dev/null
rm -rf results1 1>/dev/null 2>/dev/null
rm -rf results2 1>/dev/null 2>/dev/null

rm -rf ${OUTPUT_FILE} 1>/dev/null 2>/dev/null
