#include "util.h"

int distribution;
char output_file[MAXSTRLEN];

// Sample the value using Exponential Distribution
double sample(double lambda) {
	double u = rte_drand();
	return -log(1 - u) / lambda;
}

// Convert string type into int type
static uint32_t process_int_arg(const char *arg) {
	char *end = NULL;

	return strtoul(arg, &end, 10);
}

// Allocate all nodes for incoming packets
void allocate_incoming_nodes() {
	uint64_t rate_per_queue = rate/nr_queues;
	uint64_t nr_elements_per_queue = (2 * rate_per_queue * duration);

	incoming_array = (node_t**) rte_malloc(NULL, nr_queues * sizeof(node_t*), 64);
	if(incoming_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		incoming_array[i] = (node_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(node_t), 64);
		if(incoming_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
		}
	}

	incoming_idx_array = (uint64_t*) rte_malloc(NULL, nr_queues * sizeof(uint64_t), 64);
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
		lambda = (1.0/rate_per_queue) * 1000000000.0;
	} else if(distribution == EXPONENTIAL_VALUE) {
		lambda = 1.0/(1000000000.0/rate_per_queue);
	} else {
		rte_exit(EXIT_FAILURE, "Cannot define the interarrival distribution.\n");
	}

	uint64_t nr_elements_per_queue = 2 * rate_per_queue * duration;

	interarrival_array = (uint64_t**) rte_malloc(NULL, nr_queues * sizeof(uint64_t*), 64);
	if(interarrival_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
	}

	nr_never_sent = (uint64_t*) rte_malloc(NULL, nr_queues * sizeof(uint64_t), 64);
	if(nr_never_sent == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the nr_never_sent array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		nr_never_sent[i] = 0;

		interarrival_array[i] = (uint64_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(uint64_t), 64);
		if(interarrival_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
		}
		
		uint64_t *interarrival_gap = interarrival_array[i];
		if(distribution == UNIFORM_VALUE) {
			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
				interarrival_gap[j] = lambda * TICKS_PER_NS;
			}
		} else {
			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
				// double val = sample(lambda);
				// if(val < 5000)
				// 	val = 5000;
				// interarrival_gap[j] = val * TICKS_PER_NS;

				interarrival_gap[j] = sample(lambda) * TICKS_PER_NS;
			}
		}
	} 
}

// Allocate and create an array for all flow indentier to send to the server
void create_flow_indexes_array() {
	uint64_t rate_per_queue = rate/nr_queues;
	uint64_t nr_elements_per_queue = 2 * rate_per_queue * duration;

	flow_indexes_array = (uint16_t**) rte_malloc(NULL, nr_queues * sizeof(uint16_t*), 64);
	if(flow_indexes_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the flow_indexes array.\n");
	}

	randomness_array = (uint64_t**) rte_malloc(NULL, nr_queues * sizeof(uint64_t*), 64);
	if(randomness_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the randomness array.\n");
	}

	instructions_array = (uint64_t**) rte_malloc(NULL, nr_queues * sizeof(uint64_t*), 64);
	if(instructions_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the instructions array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		flow_indexes_array[i] = (uint16_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(uint16_t), 64);
		if(flow_indexes_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the flow_indexes array.\n");
		}

		randomness_array[i] = (uint64_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(uint64_t), 64);
		if(randomness_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the randomness array.\n");
		}

		instructions_array[i] = (uint64_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(uint64_t), 64);
		if(instructions_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the instructions array.\n");
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

		for(uint32_t i = 0; i < nr_elements_per_queue; i++) {
			randomness_array[q][i] = rte_rand();
			uint64_t instructions;
			if (srv_distribution == UNIFORM_VALUE) {
				instructions = srv_instructions;
			} else {
				double z = rte_drand();
				instructions = (uint64_t) (-((double)srv_instructions) * log(z));
			}
			instructions_array[q][i] = instructions;
		}
	}
}

// Clean up all allocate structures
void clean_heap() {
	rte_free(incoming_array);
	rte_free(incoming_idx_array);
	rte_free(flow_indexes_array);
	rte_free(interarrival_array);
	rte_free(randomness_array);
	rte_free(instructions_array);
}

// Usage message
static void usage(const char *prgname) {
	printf("%s [EAL options] -- \n"
		"  -d DISTRIBUTION: <uniform|exponential>\n"
		"  -r RATE: rate in pps\n"
		"  -f FLOWS: number of flows\n"
		"  -s SIZE: frame size in bytes\n"
		"  -t TIME: time in seconds to send packets\n"
		"  -q QUEUES: number of queues\n"
		"  -e SEED: seed\n"
		"  -i INSTRUCTIONS: number of instructions on the server\n"
		"  -j DISTRIBUTION: <uniform|exponential> on the server\n"
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
	while ((opt = getopt(argc, argvopt, "d:r:f:s:q:p:t:c:o:e:i:j:")) != EOF) {
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

		// distribution on the server
		case 'j':
			if(strcmp(optarg, "uniform") == 0) {
				// Uniform distribution
				srv_distribution = UNIFORM_VALUE;
			} else if(strcmp(optarg, "exponential") == 0) {
				// Exponential distribution
				srv_distribution = EXPONENTIAL_VALUE;
			} else {
				usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
			}
			break;
			
		// instructions on the server
		case 'i':
			srv_instructions = process_int_arg(optarg);
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

		// duration (s)
		case 't':
			duration = process_int_arg(optarg);
			break;
		
		// queues
		case 'q':
			nr_queues = process_int_arg(optarg);
			min_lcores = 3 * nr_queues + 1;
			break;

		// seed
		case 'e':
			seed = process_int_arg(optarg);
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
	uint32_t remaining_in_s = 5;
	rte_delay_us_sleep((2 * duration + remaining_in_s) * 1000000);
	// uint64_t t0 = rte_rdtsc();
	// while((rte_rdtsc() - t0) < ((2 * duration + remaining_in_s) * 1000000 * TICKS_PER_US)) { }

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
	// open the file
	FILE *fp = fopen(output_file, "w");
	if(fp == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot open the output file.\n");
	}

	uint32_t idx_per_queue = (rate * duration)/nr_queues;

	uint64_t total = 0;
	for(uint32_t i = 0; i < nr_queues; i++) {
		// get the pointers
		uint32_t incoming_idx = incoming_idx_array[i];
		uint32_t never_sent = nr_never_sent[i];

		total += (incoming_idx + never_sent);
	}

	if(total != 2 * rate * duration) {
		fclose(fp);
		return;
	}

	for(uint32_t i = 0; i < nr_queues; i++) {
		// get the pointers
		node_t *incoming = incoming_array[i];
		uint32_t incoming_idx = incoming_idx_array[i];
		uint32_t never_sent = nr_never_sent[i];

		// get the last RATE * DURATION packets considering the 'nr_never_sent' packets
		printf("incoming_idx = %d -- idx_per_queue = %d -- never_sent = %d\n", incoming_idx, idx_per_queue, never_sent);
		uint64_t j = 0;//incoming_idx - idx_per_queue;

		// print the RTT latency in (ns)
		node_t *cur;
		for(; j < incoming_idx; j++) {
			cur = &incoming[j];

			// fprintf(fp, "%lu\n", 
			// 	((uint64_t)((cur->timestamp_rx - cur->timestamp_tx)/((double)TICKS_PER_US/1000)))
			// );

			fprintf(fp, "%lu\t%lu\t%lu\t%lu\t%lu\n", 
				((uint64_t)((cur->timestamp_rx - cur->timestamp_tx)/((double)TICKS_PER_US/1000))),
				cur->flow_id,
				cur->thread_id,
				cur->sequence_nr,
				cur->batch_size
			);

		}
	}

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

	// close the file
	rte_cfgfile_close(file);
}

// Fill the data into packet payload properly
inline void fill_payload_pkt(struct rte_mbuf *pkt, uint32_t idx, uint64_t value) {
	uint8_t *payload = (uint8_t*) rte_pktmbuf_mtod_offset(pkt, uint8_t*, PAYLOAD_OFFSET);

	((uint64_t*) payload)[idx] = value;
}
