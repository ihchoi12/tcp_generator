#include "util.h"

int distribution;
char output_file[MAXSTRLEN];

// Sample the value using Exponential Distribution
double sample(double lambda) {
   	// double u = ((double) rte_rand()) / ((uint64_t) -1);
	double u = ((double) rand()) / RAND_MAX;
	return -log(1 - u) / lambda;
}

// Convert string type into int type
static uint32_t process_int_arg(const char *arg) {
	char *end = NULL;

	return strtoul(arg, &end, 10);
}

// Allocate all nodes for incoming packets (+ 20%)
void allocate_incoming_nodes() {
	uint64_t rate_per_queue = rate/nr_queues;
	uint64_t nr_elements_per_queue = (2 * rate_per_queue * duration) * 1.2;

	incoming_array = (node_t**) malloc(nr_queues * sizeof(node_t*));
	if(incoming_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		incoming_array[i] = (node_t*) malloc(nr_elements_per_queue * sizeof(node_t));
		if(incoming_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
		}
	}

	incoming_idx_array = (uint64_t*) malloc(nr_queues * sizeof(uint64_t));
	if(incoming_idx_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the incoming_idx array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		incoming_idx_array[i] = 0;
	}
} 

// Allocate and create an array for all interarrival packets for rate specified.
void create_interarrival_array() {
	uint64_t rate_per_queue = rate/nr_queues;
	double lambda;
	if(distribution == UNIFORM_VALUE) {
		lambda = (1.0/rate_per_queue) * 1000000.0;
	} else if(distribution == EXPONENTIAL_VALUE) {
		lambda = 1.0/(1000000.0/rate_per_queue);
	} else {
		rte_exit(EXIT_FAILURE, "Cannot define the interarrival distribution.\n");
	}
	
	uint64_t nr_elements_per_queue = 2 * rate_per_queue * duration;

	interarrival_array = (uint64_t**) malloc(nr_queues * sizeof(uint64_t*));
	if(interarrival_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		interarrival_array[i] = (uint64_t*) malloc(nr_elements_per_queue * sizeof(uint64_t));
		if(interarrival_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
		}
		
		uint64_t *interarrival_gap = interarrival_array[i];
		if(distribution == UNIFORM_VALUE) {
			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
				interarrival_gap[j] = lambda * TICKS_PER_US;
			}
		} else {
			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
				interarrival_gap[j] = sample(lambda) * TICKS_PER_US;
			}
		}
	} 
}

// Allocate and create an array for all flow indentier to send to the server
void create_flow_indexes_array() {
	uint64_t rate_per_queue = rate/nr_queues;
	uint64_t nr_elements_per_queue = 2 * rate_per_queue * duration;

	flow_indexes_array = (uint16_t**) malloc(nr_queues * sizeof(uint16_t*));
	if(flow_indexes_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the flow_indexes array.\n");
	}

	for(uint64_t q = 0; q < nr_queues; q++) {
		flow_indexes_array[q] = (uint16_t*) malloc(nr_elements_per_queue * sizeof(uint16_t));
		if(flow_indexes_array[q] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the flow_indexes array.\n");
		}
	}

	uint32_t last[nr_queues];
	memset(last, 0, nr_queues * sizeof(uint32_t));

	for(uint64_t f = 0; f < nr_flows; f++) {
		uint32_t idx = f % nr_queues;
		flow_indexes_array[idx][last[idx]++] = f;
	}

	for(uint64_t q = 0; q < nr_queues; q++) {
		for(uint32_t i = last[q]; i < nr_elements_per_queue; i++) {
			flow_indexes_array[q][i] = flow_indexes_array[q][i % last[q]];
		}
	}
}

// Clean up all allocate structures
void clean_heap() {
	free(incoming_array);
	free(incoming_idx_array);
	free(flow_indexes_array);
	free(interarrival_array);
}

// Usage message
static void usage(const char *prgname) {
	printf("%s [EAL options] -- \n"
		"  -d DISTRIBUTION: <uniform|exponential>\n"
		"  -r RATE: rate in pps\n"
		"  -f FLOWS: number of flows\n"
		"  -q QUEUES: number of queues\n"
		"  -s SIZE: frame size in bytes\n"
		"  -t TIME: time in seconds to send packets\n"
		"  -c FILENAME: name of the configuration file\n"
		"  -o FILENAME: name of the output file\n",
		prgname
	);
}

// Parse the argument given in the command line of the application
int app_parse_args(int argc, char **argv) {
	int opt, ret;
	char **argvopt;
	char *prgname = argv[0];

	argvopt = argv;
	while ((opt = getopt(argc, argvopt, "d:r:f:s:q:p:t:c:o:")) != EOF) {
		switch (opt) {
		// distribution
		case 'd':
			if(strcmp(optarg, "uniform") == 0) {
				// Uniform distribution
				distribution = UNIFORM_VALUE;
			} else if(strcmp(optarg, "exponential") == 0) {
				// Exponential distribution
				distribution = EXPONENTIAL_VALUE;
			} else {
				usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
			}
			break;
			
		// rate (pps)
		case 'r':
			rate = process_int_arg(optarg);
			break;

		// flows
		case 'f':
			nr_flows = process_int_arg(optarg);
			break;

		// frame size (bytes)
		case 's':
			frame_size = process_int_arg(optarg);
			if (frame_size < MIN_PKTSIZE) {
				rte_exit(EXIT_FAILURE, "The minimum packet size is %d.\n", MIN_PKTSIZE);
			}
			tcp_payload_size = (frame_size - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr) - sizeof(struct rte_tcp_hdr));
			break;

		// queues
		case 'q':
			nr_queues = process_int_arg(optarg);
			min_lcores = 3 * nr_queues + 1;
			break;

		// duration (s)
		case 't':
			duration = process_int_arg(optarg);
			break;

		// config file name
		case 'c':
			process_config_file(optarg);
			break;
		
		// output mode
		case 'o':
			strcpy(output_file, optarg);
			break;

		default:
			usage(prgname);
			rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
		}
	}

	if(optind >= 0) {
		argv[optind - 1] = prgname;
	}

	if(nr_flows < nr_queues) {
		rte_exit(EXIT_FAILURE, "The number of flows should be bigger than the number of queues.\n");
	}

	ret = optind-1;
	optind = 1;

	return ret;
}

// Wait for the duration parameter
void wait_timeout() {
	uint64_t t0 = rte_rdtsc();
	while((rte_rdtsc() - t0) < (2 * duration * 1000000 * TICKS_PER_US)) { }

	// wait for remaining
	t0 = rte_rdtsc_precise();
	while((rte_rdtsc() - t0) < (5 * 1000000 * TICKS_PER_US)) { }

	// set quit flag for all internal cores
	quit_rx = 1;
	quit_tx = 1;
	quit_rx_ring = 1;
}

// Compare two double values (for qsort function)
int cmp_func(const void * a, const void * b) {
	double da = (*(double*)a);
	double db = (*(double*)b);

	return (da - db) > ( (fabs(da) < fabs(db) ? fabs(db) : fabs(da)) * EPSILON);
}

// Print stats into output file
void print_stats_output() {
	fprintf(stderr,"\n\n[Print STAT]\n");
	// open the file
	FILE *fp = fopen(output_file, "w");
	if(fp == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot open the output file.\n");
	}
	uint64_t latencies[1000001] = {0}, total_count=0;
	for(uint32_t i = 0; i < nr_queues; i++) {
		// get the pointers
		node_t *incoming = incoming_array[i];
		uint32_t incoming_idx = incoming_idx_array[i];

		// drop the first 50% packets for warming up
		uint64_t j = 0.5 * incoming_idx;

		// print the RTT latency in (ns)
		node_t *cur;
		for(; j < incoming_idx; j++) {
			cur = &incoming[j];
			uint64_t lat_us = ((uint64_t)((cur->timestamp_rx - cur->timestamp_tx)/((double)TICKS_PER_US/1000)/1000));
			fprintf(fp, "%lu\n", lat_us);
			latencies[lat_us] += 1;
			total_count += 1;
		}
	}
	uint64_t count = 0, median = 0, p90=0, p95=0, p99=0, p99dot9=0;
	for(uint32_t i = 0; i < 1000001; i++) {
		if(latencies[i] == 0){
			continue;
		}
		
		count += latencies[i];
		// fprintf(stderr, "%u %lu %lu %lu\n", i, count, total_count, total_count * 99 / 100);
		if(median == 0 && count >= total_count/2){
			median = i;
		}
		if(p90 == 0 && count >= (total_count * 90 / 100)){
			p90 = i;
		}
		if(p95 == 0 && count >= (total_count * 95 / 100)){
			p95 = i;
		}
		if(p99 == 0 && count >= (total_count * 99 / 100)){
			p99 = i;
		}
		if(p99dot9 == 0 && count >= (total_count * 999 / 1000)){
			p99dot9 = i;
		}
	}
	printf("#flows,frame_size,#queues,Rate,Throughput,Median,p90,p95,p99,p99.9\n");
	
	printf("[RESULT]%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", nr_flows, frame_size, nr_queues, rate, total_count/duration, median, p90, p95, p99, p99dot9);
	// close the file
	fclose(fp);
}

// Process the config file
void process_config_file(char *cfg_file) {
	// open the file
	struct rte_cfgfile *file = rte_cfgfile_load(cfg_file, 0);
	if(file == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot load configuration profile %s\n", cfg_file);
	}

	// load ethernet addresses
	char *entry = (char*) rte_cfgfile_get_entry(file, "ethernet", "src");
	if(entry) {
		rte_ether_unformat_addr((const char*) entry, &src_eth_addr);
	}
	entry = (char*) rte_cfgfile_get_entry(file, "ethernet", "dst");
	if(entry) {
		rte_ether_unformat_addr((const char*) entry, &dst_eth_addr);
	}

	// load ipv4 addresses
	entry = (char*) rte_cfgfile_get_entry(file, "ipv4", "src");
	if(entry) {
		uint8_t b3, b2, b1, b0;
		sscanf(entry, "%hhd.%hhd.%hhd.%hhd", &b3, &b2, &b1, &b0);
		src_ipv4_addr = IPV4_ADDR(b3, b2, b1, b0);
	}
	entry = (char*) rte_cfgfile_get_entry(file, "ipv4", "dst");
	if(entry) {
		uint8_t b3, b2, b1, b0;
		sscanf(entry, "%hhd.%hhd.%hhd.%hhd", &b3, &b2, &b1, &b0);
		dst_ipv4_addr = IPV4_ADDR(b3, b2, b1, b0);
	}

	// load TCP destination port
	entry = (char*) rte_cfgfile_get_entry(file, "tcp", "dst");
	if(entry) {
		uint16_t port;
		sscanf(entry, "%hu", &port);
		dst_tcp_port = port;
	}

	// local server info
	entry = (char*) rte_cfgfile_get_entry(file, "server", "nr_servers");
	if(entry) {
		uint16_t n;
		sscanf(entry, "%hu", &n);
		nr_servers = n;
	}

	// close the file
	rte_cfgfile_close(file);
}

// Fill the data into packet payload properly
inline void fill_payload_pkt(struct rte_mbuf *pkt, uint32_t idx, uint64_t value) {
	uint8_t *payload = (uint8_t*) rte_pktmbuf_mtod_offset(pkt, uint8_t*, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr));

	((uint64_t*) payload)[idx] = value;
}
