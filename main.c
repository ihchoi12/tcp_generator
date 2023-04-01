#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "util.h"
#include "tcp_util.h"
#include "dpdk_util.h"

// Application parameters
uint64_t rate;
uint32_t seed;
uint64_t duration;
uint64_t nr_flows;
uint64_t nr_queues;
uint32_t min_lcores;
uint32_t frame_size;
uint32_t tcp_payload_size;

// Server parameters
uint64_t srv_distribution;
uint64_t srv_instructions;

// General variables
uint64_t TICKS_PER_US;
uint16_t **flow_indexes_array;
uint64_t **interarrival_array;
//
uint64_t **randomness_array;
uint64_t **instructions_array;

// Heap and DPDK allocated
node_t **incoming_array;
uint64_t *incoming_idx_array;
struct rte_mempool *pktmbuf_pool;
tcp_control_block_t *tcp_control_blocks;

// Internal threads variables
volatile uint8_t quit_rx = 0;
volatile uint8_t quit_tx = 0;
volatile uint32_t ack_dup = 0;
volatile uint32_t ack_empty = 0;
volatile uint8_t quit_rx_ring = 0;
volatile uint64_t *nr_never_sent = 0;
lcore_param lcore_params[RTE_MAX_LCORE];
struct rte_ring *rx_rings[RTE_MAX_LCORE];

// Connection variables
uint16_t dst_tcp_port;
uint32_t dst_ipv4_addr;
uint32_t src_ipv4_addr;
struct rte_ether_addr dst_eth_addr;
struct rte_ether_addr src_eth_addr;

// Process the incoming TCP packet
int process_rx_pkt(struct rte_mbuf *pkt, node_t *incoming, uint64_t *incoming_idx) {
	// process only TCP packets
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	if(unlikely(ipv4_hdr->next_proto_id != IPPROTO_TCP)) {
		return 0;
	}

	// get TCP header
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + (ipv4_hdr->version_ihl & 0x0f)*4);

	// retrieve the index of the flow from the NIC (NIC tags the packet according the 5-tuple using DPDK rte_flow)
	uint32_t flow_id = pkt->hash.fdir.hi;

	// get control block for the flow
	tcp_control_block_t *block = &tcp_control_blocks[flow_id];

	// update receive window from the packet
	rte_atomic16_set(&block->tcb_rwin, tcp_hdr->rx_win);

	// get TCP payload size
	uint32_t packet_data_size = rte_be_to_cpu_16(ipv4_hdr->total_length) - ((ipv4_hdr->version_ihl & 0x0f)*4) - ((tcp_hdr->data_off >> 4)*4);

	// do not process empty packets
	if(unlikely(packet_data_size == 0)) {
		return 0;
	}

	// do not process retransmitted packets
	uint32_t seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
	if(SEQ_LT(block->last_seq_recv, seq)) {
		block->last_seq_recv = seq;
	} else {
		return 0;
	}

	// update ACK number in the TCP control block from the packet
	uint32_t ack_cur = rte_be_to_cpu_32(rte_atomic32_read(&block->tcb_next_ack));
	uint32_t ack_hdr = rte_be_to_cpu_32(tcp_hdr->sent_seq) + (packet_data_size);
	if(SEQ_LEQ(ack_cur, ack_hdr)) {
		rte_atomic32_set(&block->tcb_next_ack, tcp_hdr->sent_seq + rte_cpu_to_be_32(packet_data_size));
	}

	// obtain both timestamp from the packet
	uint64_t *payload = (uint64_t *)(((uint8_t*) tcp_hdr) + ((tcp_hdr->data_off >> 4)*4));
	uint64_t t0 = payload[0];
	uint64_t t1 = payload[1];

	// fill the node previously allocated
	node_t *node = &incoming[(*incoming_idx)++];
	node->timestamp_tx = t0;
	node->timestamp_rx = t1;

	// node->flow_id = payload[2];
	// node->thread_id = payload[3];

	return 1;
}

// Start the client establishing all TCP connections
void start_client(uint16_t portid) {
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint64_t ts_syn;
	uint32_t nb_retransmission;
	struct rte_mbuf *pkt;
	tcp_control_block_t *block;
	struct rte_mbuf *pkts[BURST_SIZE];

	for(int i = 0; i < nr_flows; i++) {
		// get the TCP control block for the flow
		block = &tcp_control_blocks[i];
		// create the TCP SYN packet
		struct rte_mbuf *syn_packet = create_syn_packet(i);
		// insert the rte_flow in the NIC to retrieve the flow id for incoming packets of this flow
		insert_flow(portid, i);

		// send the SYN packet
		nb_tx = rte_eth_tx_burst(portid, i % nr_queues, &syn_packet, 1);
		if(nb_tx != 1) {
			rte_exit(EXIT_FAILURE, "Error to send the TCP SYN packet.\n");
		}

		// clear the counters
		nb_retransmission = 0;
		ts_syn = rte_rdtsc();

		// change the TCP state to SYN_SENT
		rte_atomic16_set(&block->tcb_state, TCP_SYN_SENT);

		// while not received SYN+ACK packet and TCP state is not ESTABLISHED
		while(rte_atomic16_read(&block->tcb_state) != TCP_ESTABLISHED) {
			// receive TCP SYN+ACK packets from the NIC
			nb_rx = rte_eth_rx_burst(portid, i % nr_queues, pkts, BURST_SIZE);

			for(int j = 0; j < nb_rx; j++) {
				// process the SYN+ACK packet, returning the ACK packet to send
				pkt = process_syn_ack_packet(pkts[j]);
				
				if(pkt) {
					// send the TCP ACK packet to the server
					nb_tx = rte_eth_tx_burst(portid, i % nr_queues, &pkt, 1);
					if(nb_tx != 1) {
						rte_exit(EXIT_FAILURE, "Error to send the TCP ACK packet.\n");
					}
				}
			}
			// free packets
			rte_pktmbuf_free_bulk(pkts, nb_rx);

			if((rte_rdtsc() - ts_syn) > HANDSHAKE_TIMEOUT_IN_US * TICKS_PER_US) {
				nb_retransmission++;
				nb_tx = rte_eth_tx_burst(portid, i % nr_queues, &syn_packet, 1);
				if(nb_tx != 1) {
						rte_exit(EXIT_FAILURE, "Error to send the TCP SYN packet.\n");
				}
				ts_syn = rte_rdtsc();

				if(nb_retransmission == HANDSHAKE_RETRANSMISSION) {
					rte_exit(EXIT_FAILURE, "Cannot establish connection.\n");
				}
			}
		}
	}

	// Discard 3-way handshake packets in the DPDK metrics
	rte_eth_stats_reset(portid);
	rte_eth_xstats_reset(portid);
	
	rte_compiler_barrier();
}

// RX processing
static int lcore_rx_ring(void *arg) {
	lcore_param *rx_conf = (lcore_param *) arg;
	uint8_t qid = rx_conf->qid;

	uint16_t nb_rx;
	uint64_t *incoming_idx = &incoming_idx_array[qid];
	node_t *incoming = incoming_array[qid];
	struct rte_mbuf *pkts[BURST_SIZE];
	struct rte_ring *rx_ring = rx_rings[qid];

	while(!quit_rx_ring) {
		// retrieve packets from the RX core
		nb_rx = rte_ring_sc_dequeue_burst(rx_ring, (void**) pkts, BURST_SIZE, NULL); 
		for(int i = 0; i < nb_rx; i++) {
			// rte_prefetch_non_temporal(rte_pktmbuf_mtod(pkts[i], void *));
			// process the incoming packet
			process_rx_pkt(pkts[i], incoming, incoming_idx);
			// free the packet
			rte_pktmbuf_free(pkts[i]);
		}
	}

	// process all remaining packets that are in the RX ring (not from the NIC)
	do{
		nb_rx = rte_ring_sc_dequeue_burst(rx_ring, (void**) pkts, BURST_SIZE, NULL);
		for(int i = 0; i < nb_rx; i++) {
			// rte_prefetch_non_temporal(rte_pktmbuf_mtod(pkts[i], void *));
			// process the incoming packet
			process_rx_pkt(pkts[i], incoming, incoming_idx);
			// free the packet
			rte_pktmbuf_free(pkts[i]);
		}
	} while (nb_rx != 0);

	return 0;
}

// Main RX processing
static int lcore_rx(void *arg) {
	lcore_param *rx_conf = (lcore_param *) arg;
	uint16_t portid = rx_conf->portid;
	uint8_t qid = rx_conf->qid;

	uint64_t now;
	uint16_t nb_rx;
	uint16_t nb_pkts;
	struct rte_mbuf *pkts[BURST_SIZE];
	struct rte_ring *rx_ring = rx_rings[qid];
	
	while(!quit_rx) {
		// retrieve the packets from the NIC
		nb_rx = rte_eth_rx_burst(portid, qid, pkts, BURST_SIZE);

		// retrive the current timestamp
		now = rte_rdtsc();
		for(int i = 0; i < nb_rx; i++) {
			// fill the timestamp into packet payload
			fill_payload_pkt(pkts[i], 1, now);
		}

		// enqueue the packets to the ring
		nb_pkts = rte_ring_sp_enqueue_burst(rx_ring, (void* const*) pkts, nb_rx, NULL);
		if(unlikely(nb_pkts != nb_rx)) {
			rte_exit(EXIT_FAILURE, "Cannot enqueue the packet to the RX thread: %s.\n", rte_strerror(errno));
		}
	}

	return 0;
}

// Main TX processing
static int lcore_tx(void *arg) {
	lcore_param *tx_conf = (lcore_param *) arg;
	uint16_t portid = tx_conf->portid;
	uint8_t qid = tx_conf->qid;
	uint64_t nr_elements = tx_conf->nr_elements;

	struct rte_mbuf *pkts[BURST_SIZE];
	uint16_t *flow_indexes = flow_indexes_array[qid];
	uint64_t *interarrival_gap = interarrival_array[qid];

	uint64_t *randomness = randomness_array[qid];
	uint64_t *instructions = instructions_array[qid];

	uint64_t next_tsc = rte_rdtsc() + interarrival_gap[0];

	for(uint64_t i = 0; i < nr_elements; i++) {
		// unable to keep up with the requested rate
		if(unlikely(rte_rdtsc() > (next_tsc + 5*TICKS_PER_US))) {
			// count this batch as dropped
			nr_never_sent[qid]++;
			next_tsc += (interarrival_gap[i] + 2*TICKS_PER_US);
			continue;
		}

		// choose the flow to send
		uint16_t flow_id = flow_indexes[i];
		tcp_control_block_t *block = &tcp_control_blocks[flow_id];

		// get the packet
		pkts[0] = rte_pktmbuf_alloc(pktmbuf_pool);
		fill_tcp_packet(block, pkts[0]);

		// check the receive window for this flow
		// uint16_t rx_wnd = rte_atomic16_read(&block->tcb_rwin);
		// while(unlikely(rx_wnd < tcp_payload_size)) { 
		// 	rx_wnd = rte_atomic16_read(&block->tcb_rwin);
		// }

		// fill the timestamp into the packet payload
		fill_payload_pkt(pkts[0], 0, next_tsc);
		fill_payload_pkt(pkts[0], 2, flow_id);
		fill_payload_pkt(pkts[0], 4, instructions[i]);
		fill_payload_pkt(pkts[0], 5, randomness[i]);

		// sleep for while
		while (rte_rdtsc() < next_tsc) { }

		// send the packet
		rte_eth_tx_burst(portid, qid, pkts, 1);

		// update the counter
		next_tsc += interarrival_gap[i];
	}

	return 0;
}

// main function
int main(int argc, char **argv) {
	// init EAL
	int ret = rte_eal_init(argc, argv);
	if(ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	}
	argc -= ret;
	argv += ret;

	// parse application arguments (after the EAL ones)
	ret = app_parse_args(argc, argv);
	if(ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");
	}

	// initialize DPDK
	uint16_t portid = 0;
	init_DPDK(portid, nr_queues, seed);

	// allocate nodes for incoming packets
	allocate_incoming_nodes();

	// create flow indexes array
	create_flow_indexes_array();

	// create interarrival array
	create_interarrival_array();
	
	// initialize TCP control blocks
	init_tcp_blocks();

	// start client (3-way handshake for each flow)
	start_client(portid);

	// create the DPDK rings for RX threads
	create_dpdk_rings();

	// start RX and TX threads
	uint32_t id_lcore = rte_lcore_id();	
	for(int i = 0; i < nr_queues; i++) {
		lcore_params[i].portid = portid;
		lcore_params[i].qid = i;
		lcore_params[i].nr_elements = (rate/nr_queues) * 2 * duration;

		id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
		rte_eal_remote_launch(lcore_rx_ring, (void*) &lcore_params[i], id_lcore);

		id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
		rte_eal_remote_launch(lcore_rx, (void*) &lcore_params[i], id_lcore);

		id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
		rte_eal_remote_launch(lcore_tx, (void*) &lcore_params[i], id_lcore);
	}

	// wait for duration parameter
	wait_timeout();

	// wait for RX/TX threads
	uint32_t lcore_id;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if(rte_eal_wait_lcore(lcore_id) < 0) {
			return -1;
		}
	}

	for(int i = 0; i < nr_queues; i++) {
		printf("never_sent[%d]: %lu\n", i, nr_never_sent[i]);
	}

	// print stats
	print_stats_output();

	// print DPDK stats
	print_dpdk_stats(portid);

	// clean up
	clean_heap();
	clean_hugepages();

	return 0;
}
