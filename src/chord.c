#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>

#include "chord_arg_parser.h"
#include "chord.h"
#include "chord_impl.h"
#include "hash.h"

#include "chord.pb-c.h"

struct chord_arguments chord_args;
struct sha1sum_ctx *ctx;

int sockfd = -1;
int fixIndex = 0;
int predExists = 1;
int succListIndex = 1;

uint64_t hash;
Node predecessor;
Node successor;
Node *finger_table;
Node *successor_list;
Node self;

void create() {
	predecessor = (Node) NODE__INIT;
	predecessor.key = 0;

	successor = self;
}

void join() {
	predecessor = (Node) NODE__INIT;
	predecessor.key = 0;

	StartFindSuccessorRequest request = START_FIND_SUCCESSOR_REQUEST__INIT;
	request.key = hash;

	ChordMessage msg = CHORD_MESSAGE__INIT;
	msg.version = 417;
	msg.start_find_successor_request = &request;
	msg.msg_case = CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_REQUEST;

	uint64_t msg_len = chord_message__get_packed_size(&msg);

	size_t total_size = sizeof(msg_len) + msg_len;
	uint8_t *buffer = malloc(total_size);

	uint64_t networkLen = htobe64(msg_len);
	memcpy(buffer, &networkLen, sizeof(networkLen));
	chord_message__pack(&msg, buffer + sizeof(networkLen));

	if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&chord_args.join_address, sizeof(struct sockaddr_in)) < 0) {
		perror("Error sending get predecessor request");
		exit(1);
	}

	MessageResponse response;
	do {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

		if (ret < 0) {
				perror("Select error");
				break;
		} else if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
				response = process_chord_msg(1);
		} 
	} while (response.type != CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE);

	successor = response.node;

	free(buffer);
}

void notify() {
	NotifyRequest request = NOTIFY_REQUEST__INIT;
	request.node = &self;

	ChordMessage msg = CHORD_MESSAGE__INIT;
	msg.version = 417;
	msg.notify_request = &request;
	msg.msg_case = CHORD_MESSAGE__MSG_NOTIFY_REQUEST;

	uint64_t msg_len = chord_message__get_packed_size(&msg);

	size_t total_size = sizeof(msg_len) + msg_len;
	uint8_t *buffer = malloc(total_size);

	uint64_t networkLen = htobe64(msg_len);
	memcpy(buffer, &networkLen, sizeof(networkLen));
	chord_message__pack(&msg, buffer + sizeof(networkLen));

	struct sockaddr_in node_addr;
	node_addr.sin_family = AF_INET;
	node_addr.sin_port = successor.port;
	node_addr.sin_addr = (struct in_addr) {.s_addr = successor.address};

	if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(struct sockaddr_in)) < 0) {
		perror("Error sending notify request");
		exit(1);
	}

	free(buffer);
}

// TODO: check for failed nodes and re-fix successor list
// stabilize(), fix_successor_list(), and fix_fingers() moved to chord_impl.c

void check_predecessor() {
	// if (!predExists) {
	// //	printf("Tralalaeo tralalala: %" PRIu64 "\n", predecessor.key);
	// 	predecessor.key = 0;
	// 	predecessor.address = 0;
	// 	predecessor.port = 0;
	// }

	// predExists = 0;

	// if (predecessor.key != 0) {
	// 	CheckPredecessorRequest request = CHECK_PREDECESSOR_REQUEST__INIT;
	// 	ChordMessage msg = CHORD_MESSAGE__INIT;
	// 	msg.version = 417;
	// 	msg.check_predecessor_request = &request;
	// 	msg.msg_case = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST;

	// 	uint64_t msg_len = chord_message__get_packed_size(&msg);

	// 	size_t total_size = sizeof(msg_len) + msg_len;
	// 	uint8_t *buffer = malloc(total_size);

	// 	uint64_t networkLen = htobe64(msg_len);
	// 	memcpy(buffer, &networkLen, sizeof(networkLen));
	// 	chord_message__pack(&msg, buffer + sizeof(networkLen));

	// 	struct sockaddr_in node_addr;
	// 	node_addr.sin_family = AF_INET;
	// 	node_addr.sin_port = predecessor.port;
	// 	node_addr.sin_addr = (struct in_addr) {.s_addr = predecessor.address};

	// 	if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(struct sockaddr_in)) < 0) {
	// 		perror("Error sending check predecessor request");
	// 		exit(1);
	// 	}

	// 	free(buffer);
	// }
	// else {
	// 	predExists = 1;
	// }
	 
	if (predecessor.key != 0) {
		CheckPredecessorRequest request = CHECK_PREDECESSOR_REQUEST__INIT;
		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.check_predecessor_request = &request;
		msg.msg_case = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST;

		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);
		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));

		struct sockaddr_in node_addr;
		node_addr.sin_family = AF_INET;
		node_addr.sin_port = predecessor.port;
		node_addr.sin_addr = (struct in_addr) {.s_addr = predecessor.address};

		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(struct sockaddr_in)) < 0) {
			perror("Error sending check predecessor request");
			exit(1);
		}

		time_t currTime = time(NULL);
		time_t timeoutTime = time(NULL) + 1;

		MessageResponse response;
		do {
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(sockfd, &readfds);

			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

			if (ret < 0) {
					perror("Select error");
					break;
			} else if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
					response = process_chord_msg(6);
			}

			currTime = time(NULL);
		} while (currTime < timeoutTime && response.type != CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE);

		if (response.type != CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE) {
		  //printf("Tralalaeo tralalala: %" PRIu64 "\n", predecessor.key);
			predecessor.key = 0;
			predecessor.address = 0;
			predecessor.port = 0;
		}
		
		free(buffer);
	} 
}

// find_successor() and closest_preceding_node() moved to chord_impl.c

MessageResponse process_chord_msg(int i) {
	uint64_t messageLenSize;
	struct sockaddr_in node_addr;
  socklen_t addrLen = sizeof(node_addr);

	MessageResponse response = {.type = CHORD_MESSAGE__MSG__NOT_SET};
	
	// int available = 0;
	// ioctl(sockfd, FIONREAD, &available);

	// if (available < sizeof(uint64_t)) {
	// 	// Drain partial data from the socket to prevent select() loop
	// 	char drain_buf[4096];
	// 	recvfrom(sockfd, drain_buf, sizeof(drain_buf), 0, (struct sockaddr *)&node_addr, &addrLen);
		
	// 	return response;
	// }

	if (recvfrom(sockfd, &messageLenSize, sizeof(uint64_t), MSG_PEEK, (struct sockaddr *)&node_addr, &addrLen) < 0) {
		perror("Peek error");
		return response;
	}

	messageLenSize = be64toh(messageLenSize);

	size_t total_size = sizeof(uint64_t) + messageLenSize;
	uint8_t *buffer = malloc(total_size);

	if (recvfrom(sockfd, buffer, total_size, MSG_DONTWAIT, (struct sockaddr *)&node_addr, &addrLen) < 0) {
		return response;
	}

	ChordMessage *message = chord_message__unpack(NULL, messageLenSize, buffer + sizeof(uint64_t));
	
	// Find successor
	if (message->msg_case == CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE) {
		FindSuccessorResponse *successorResponse = message->find_successor_response;

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE, .node = *successorResponse->node};
	}
	else if (message->msg_case == CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST) {
		FindSuccessorRequest *successorRequest = message->find_successor_request;

		Node predNode = element_of(successorRequest->key, hash, successor.key, 1) ? // id ∈ (n, successor]
										successor : closest_preceding_node(successorRequest->key);

		FindSuccessorResponse successorResponse = FIND_SUCCESSOR_RESPONSE__INIT;
		successorResponse.node = &predNode;

		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.find_successor_response = &successorResponse;
		msg.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE;
		
		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);
		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));

		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
			perror("Error sending find successor response");
			exit(1);
		}

		free(buffer);
		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST};
	}

	// Start find successor
	else if (message->msg_case == CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE) {
		StartFindSuccessorResponse *startResponse = message->start_find_successor_response;

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE, .node = *startResponse->node};
	}
	else if (message->msg_case == CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_REQUEST) {
		StartFindSuccessorRequest *startRequest = message->start_find_successor_request;

		Node predNode = find_successor(startRequest->key);

		StartFindSuccessorResponse startResponse = START_FIND_SUCCESSOR_RESPONSE__INIT;
		startResponse.node = &predNode;

		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.start_find_successor_response = &startResponse;
		msg.msg_case = CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE;
		
		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);
		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));

		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
			perror("Error sending start find successor response");
			exit(1);
		}

		free(buffer);
		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_REQUEST};
	}

	// Get predecessor
	else if (message->msg_case == CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE) {
		GetPredecessorResponse *predecessorResponse = message->get_predecessor_response;

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE, .node = *predecessorResponse->node};
	}
	else if (message->msg_case == CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST) {
		GetPredecessorResponse predecessorResponse = GET_PREDECESSOR_RESPONSE__INIT;
		predecessorResponse.node = &predecessor;

		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.get_predecessor_response = &predecessorResponse;
		msg.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE;

		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);

		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));
		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
			perror("Error sending get predecessor response");
			exit(1);
		}

		free(buffer);
		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST};
	}

	// Notify
	else if (message->msg_case == CHORD_MESSAGE__MSG_NOTIFY_REQUEST) {
		NotifyRequest *notifyRequest = message->notify_request;
		Node senderNode = *notifyRequest->node; 

		if (predecessor.key == 0 || element_of(senderNode.key, predecessor.key, hash, 0)) {
			predecessor = senderNode;
		}

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_NOTIFY_REQUEST};
	}

	// Check predecessor
	else if (message->msg_case == CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE) {
		predExists = 1;

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE};
	}
	else if (message->msg_case == CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST) {
		CheckPredecessorResponse checkResponse = CHECK_PREDECESSOR_RESPONSE__INIT;

		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.check_predecessor_response = &checkResponse;
		msg.msg_case = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE;
		
		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);
		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));

		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
			perror("Error sending check predecessor response");
			exit(1);
		}

		free(buffer);
		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST};
	}

	// Get successor list
	else if (message->msg_case == CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE) {
		GetSuccessorListResponse *listResponse = message->get_successor_list_response;

		Node *successors = malloc(sizeof(Node) * listResponse->n_successors);
		for (size_t i = 0; i < listResponse->n_successors; ++i) {
			successors[i] = *(listResponse->successors[i]);
		}

		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE,
																	.n_successors = listResponse->n_successors,
																	.successors = successors};
	}
	else if (message->msg_case == CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST) {
		GetSuccessorListResponse listResponse = GET_SUCCESSOR_LIST_RESPONSE__INIT;

		// Copy successor list and remove last entry
		size_t num_entries = chord_args.num_successors - 1;
		Node **successors = malloc(sizeof(Node*) * num_entries);
		
		for (size_t i = 0; i < num_entries; ++i) {
			successors[i] = &successor_list[i];
		}
	
		listResponse.n_successors = num_entries;
		listResponse.successors = successors;

		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.get_successor_list_response = &listResponse;
		msg.msg_case = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE;
		
		uint64_t msg_len = chord_message__get_packed_size(&msg);

		size_t total_size = sizeof(msg_len) + msg_len;
		uint8_t *buffer = malloc(total_size);

		uint64_t networkLen = htobe64(msg_len);
		memcpy(buffer, &networkLen, sizeof(networkLen));
		chord_message__pack(&msg, buffer + sizeof(networkLen));

		if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
			perror("Error sending get sucessor list response");
			exit(1);
		}

		free(buffer);
		free(successors);
		response = (MessageResponse) {.type = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST};
	}

	free(buffer);
	chord_message__free_unpacked(message, NULL);
	return response;
}

void lookup(uint64_t key) {
	Node key_succ = find_successor(key);

	char ip[INET_ADDRSTRLEN];

	struct in_addr succ_addr = {.s_addr = key_succ.address};
	inet_ntop(AF_INET, &succ_addr, ip, sizeof(ip));

	printf("< %" PRIu64 " %s %" PRIu32 "\n", key_succ.key, ip, ntohs(key_succ.port));
}

void print_state() {
	char ip[INET_ADDRSTRLEN];
	int i;

	struct in_addr self_addr = {.s_addr = self.address};
	inet_ntop(AF_INET, &self_addr, ip, sizeof(ip));

	printf("< Self %" PRIu64 " %s %" PRIu32 "\n", self.key, ip, ntohs(self.port));

	/* struct in_addr pred_addr = {.s_addr = predecessor.address};
	inet_ntop(AF_INET, &pred_addr, ip, sizeof(ip));

	printf("< Predecessor %" PRIu64 " %s %" PRIu32 "\n", predecessor.key, ip, ntohs(predecessor.port));

	struct in_addr succ_addr = {.s_addr = successor.address};
	inet_ntop(AF_INET, &succ_addr, ip, sizeof(ip));

	printf("< Successor %" PRIu64 " %s %" PRIu32 "\n", successor.key, ip, ntohs(successor.port)); */

	i = 0;
	while (i < chord_args.num_successors && successor_list[i].key != 0) {
		struct in_addr addr = {.s_addr = successor_list[i].address};
		inet_ntop(AF_INET, &addr, ip, sizeof(ip));

		printf("< Successor [%d] %" PRIu64 " %s %" PRIu32 "\n",
					 i + 1, successor_list[i].key, ip, ntohs(successor_list[i].port));
		i++;
	}

	i = 0;
	while (i < M && finger_table[i].key != 0) {
		struct in_addr addr = {.s_addr = finger_table[i].address};
		inet_ntop(AF_INET, &addr, ip, sizeof(ip));

		printf("< Finger [%d] %" PRIu64 " %s %" PRIu32 "\n",
					 i + 1, finger_table[i].key, ip, ntohs(finger_table[i].port));
		i++;
	}
}

// curr_var ∈ (r1, r2);
int element_of(uint64_t curr_var, uint64_t r1, uint64_t r2, int is_inclusive) {
	int bound1 = curr_var > r1;
	int bound2 = is_inclusive ? curr_var <= r2 : curr_var < r2;

  
	if (r1 < r2) {
		return bound1 && bound2;
	} else if (r1 > r2) {
		return bound1 || bound2;
	} else { // r1 == r2
		return curr_var != r1;
	}
}

uint64_t get_hash(struct sockaddr_in *addr) {
	uint8_t buffer[6];
	memcpy(buffer, &addr->sin_addr.s_addr, 4);
	memcpy(buffer + 4, &addr->sin_port, 2);

	uint8_t checksum[20];
	sha1sum_finish(ctx, buffer, sizeof(buffer), checksum);

	uint64_t head = sha1sum_truncated_head(checksum);

	sha1sum_reset(ctx);

	return head;
}

// Uses a dummy socket along with getsockname() to get our IP address
void get_local_address(struct sockaddr_in *addr) {
	int tempfd;
	struct sockaddr_in temp_addr;
	socklen_t addr_size = sizeof(*addr);

	tempfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	temp_addr.sin_family = AF_INET;
	temp_addr.sin_port = htons(44);
	inet_pton(AF_INET, "12.34.56.78", &temp_addr.sin_addr);

	connect(tempfd, (struct sockaddr*)&temp_addr, sizeof(temp_addr));

	getsockname(tempfd, (struct sockaddr*)addr, &addr_size);

	addr->sin_port = chord_args.my_address.sin_port;

	close(tempfd);
}

void print_key(uint64_t key) {
	printf("%" PRIu64, key);
	printf("\n");
}

void process_input(char *input) {
	char *cmd = malloc(128 * sizeof(char));
  char *arg= malloc(128 * sizeof(char));

  memset(cmd, 0, 128);
  memset(arg, 0, 128);

  sscanf(input, "%s %[^\n]", cmd, arg); // Scans command string, and then until newline

	if ((strcmp(cmd, "Lookup") == 0) && (strlen(arg) > 0)) {
		uint8_t checksum[20];
		sha1sum_finish(ctx, (const uint8_t*)arg, strlen(arg), checksum);

		uint64_t head = sha1sum_truncated_head(checksum);

		sha1sum_reset(ctx);

		printf("< %s %" PRIu64 "\n", arg, head);

		lookup(head);
	} else if ((strcmp(cmd, "PrintState") == 0) && (strlen(arg) == 0)) {
		print_state();
	}

	free(cmd);
	free(arg);
}

void cleanup() {
	free(finger_table);
	free(successor_list);
	sha1sum_destroy(ctx);
	close(sockfd);
	exit(0);
}

int main(int argc, char *argv[]) {
	printf("> ");
	fflush(stdout);

	char input[256];

	signal(SIGSTOP, cleanup);
	signal(SIGINT, cleanup);

	chord_args = chord_parseopt(argc, argv);

	// Initialize checksum
	ctx = sha1sum_create(NULL, 0);
	if (!ctx) {
		perror("Error creating checksum");
		exit(1);
	}
	
	// Initialize socket
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("Failed to create socket");
		exit(1);
	}

	// Set socket to reuseable
	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Set socket to non-blocking
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	
	if (bind(sockfd, (struct sockaddr*)&chord_args.my_address, sizeof(chord_args.my_address)) < 0) {
		perror("Bind failed");
		close(sockfd);
		exit(1);
	}

	// Get our IP address
	struct sockaddr_in hash_addr;
	get_local_address(&hash_addr);

	hash = get_hash(&hash_addr);
	
	self = (Node) NODE__INIT;

	self.address = hash_addr.sin_addr.s_addr;
	self.port = hash_addr.sin_port;
	self.key = hash;

	if (chord_args.join_address.sin_family != AF_INET) { // New Chord ring
		create();
	} else { // Join existing Chord ring
		join();
	}

	finger_table = malloc(sizeof(Node) * M);
	for (int i = 0; i < M; ++i) {
		finger_table[i] = (Node) NODE__INIT;
	}

	successor_list = malloc(sizeof(Node) * chord_args.num_successors);

	successor_list[0] = successor;
	for (int i = 1; i < chord_args.num_successors; ++i) {
		successor_list[i] = (Node) NODE__INIT;
	}

	struct timeval timeout;
	time_t curr = time(NULL);
	time_t stabilize_time = curr + chord_args.stablize_period / 10;
	time_t fix_fingers_time = curr + chord_args.fix_fingers_period / 10;
	time_t check_predecessor_time = curr + chord_args.check_predecessor_period / 10;

	timeout.tv_sec = 1; // Prevents select from blocking so periodic stuff can happen
	timeout.tv_usec = 0;

	while (1) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(STDIN_FILENO, &read_fds);
		FD_SET(sockfd, &read_fds);

		int maxfd = STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd;

		int ret = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);

		if (ret < 0) {
			perror("Select error");
			break;
		}

		if (FD_ISSET(STDIN_FILENO, &read_fds)) {
			if (!fgets(input, sizeof(input), stdin)) {
				break;
			}
			process_input(input);
			
			printf("> ");
			fflush(stdout);
		}

	    if (FD_ISSET(sockfd, &read_fds)) {
			process_chord_msg(10);
		} 

		curr = time(NULL);
		if (curr >= stabilize_time) {
		//	printf("Stabilize\n");
			stabilize();
			stabilize_time = curr + chord_args.stablize_period / 10;
		}
		if (curr >= fix_fingers_time) {
		//    printf("Fix fingers\n");
			fix_fingers();
			fix_fingers_time = curr + chord_args.fix_fingers_period / 10;
		}
		if (curr >= check_predecessor_time) {
		//   printf("Check pred\n");
			check_predecessor();
			check_predecessor_time = curr + chord_args.check_predecessor_period / 10;
		}

	//print_state();
	}

	cleanup();
	return 0;
}
