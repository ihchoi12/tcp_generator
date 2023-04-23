# TCP_generator

Follow these instructions to build the TCP generator using DPDK 22.11 and CloudLab nodes

## Building

> **Make sure that `PKG_CONFIG_PATH` is configured properly.**

```bash
git clone https://github.com/carvalhof/tcp_generator
cd tcp_generator
git checkout dev
make
```

## Running

> **Make sure that `LD_LIBRARY_PATH` is configured properly.**

```bash
sudo ./build/tcp-generator -a 41:00.0 -n 4 -c 0xff -- -d $DISTRIBUTION -r $RATE -f $FLOWS -s $SIZE -t $DURATION -q $QUEUES -e $SEED -c $ADDR_FILE -o $OUTPUT_FILE -D $SRV_DISTRIBUTION -i $SRV_ITERATIONS0 -j $SRV_ITERATIONS1 -m $SRV_MODE
```

> **Example**

```bash
sudo ./build/tcp-generator -a 41:00.0 -n 4 -c 0xff -- -d exponential -r 50000 -f 16 -s 128 -t 5 -q 1 -e 1646203793 -c addr.cfg -o output.dat -D bimodal -i 533 -j 54956 -m 0.99
```

### Parameters

- `$DISTRIBUTION` : interarrival distribution (_e.g.,_ uniform or exponential)
- `$RATE` : packet rate in _pps_
- `$FLOWS` : number of flows
- `$SIZE` : packet size in _bytes_
- `$DURATION` : duration of execution in _seconds_ (we double for warming up)
- `$QUEUES` : number of RX/TX queues
- `$SEED` : seed number
- `$ADDR_FILE` : name of address file (_e.g.,_ 'addr.cfg')
- `$OUTPUT_FILE` : name of output file containg the latency for each packet
- `$SRV_DISTRIBUTION` : instruction distribution on the server (_e.g.,_ uniform, exponential, or bimodal)
- `$SRV_ITERATIONS0` : instructions for `$SRV_DISTRIBUTION`
- `$SRV_ITERATIONS1` : instructions of the _mode1_ for bimodal distribution
- `$SRV_MODE` : mode for bimodal distribution (_e.g.,_ 0.0 < `$SRV_MODE` < 1.0)

### _addresses file_ structure

```
[ethernet]
src = 0c:42:a1:8c:db:1c
dst = 0c:42:a1:8c:dc:54

[ipv4]
src = 192.168.1.2
dst = 192.168.1.1

[tcp]
dst = 12345
```
