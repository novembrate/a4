
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>

#include "chord.h"
#include "chord_impl.h"
#include "chord_arg_parser.h"
#include "chord.pb-c.h"

/**
 * @brief Sends a buffer to a Chord node over UDP.
 * 
 * This helper function constructs a socket address from the provided Node structure
 * and sends the given buffer to that node using the sendto() system call. The function
 * handles error reporting and terminates the program if the send operation fails.
 * 
 * @param node Pointer to the target Chord node to send the message to. The node's
 *             address and port are extracted to construct the destination socket address.
 *             Must not be NULL.
 * @param buffer Pointer to the buffer containing the message data to send. The buffer
 *               should contain a properly formatted Chord protocol message.
 * @param total_size The total size in bytes of the buffer to send. This should include
 *                   both the message length prefix and the actual message data.
 * @param error_msg Optional error message string to print before the system error
 *                  message. If NULL, only the system error message from perror() is
 *                  printed. The error message is printed to stderr.
 * 
 * @note This function uses the global sockfd variable for the socket file descriptor.
 *       The function will call exit(1) if sendto() fails, so it does not return on error.
 * 
 * @warning This function terminates the program on failure. Ensure proper error handling
 *          in calling code if graceful error recovery is needed.
 */
static void send_to_node(Node *node, uint8_t *buffer, size_t total_size, const char *error_msg) {
	struct sockaddr_in node_addr;
	node_addr.sin_family = AF_INET;
	node_addr.sin_port = node->port;
	node_addr.sin_addr = (struct in_addr) {.s_addr = node->address};

	if (sendto(sockfd, buffer, total_size, 0, (struct sockaddr *)&node_addr, sizeof(struct sockaddr_in)) < 0) {
		if (error_msg) {
			fprintf(stderr, "%s\n", error_msg);
		}
		perror("sendto()");
		exit(1);
	}
}

/**
 * @brief Waits for a specific Chord protocol message response with timeout.
 * 
 * This helper function polls the socket for incoming messages and processes them
 * until either the expected message type is received or a timeout occurs. The function
 * uses select() to efficiently wait for socket data availability with a 1-second timeout
 * per iteration, and continues polling for up to 1 second total.
 * 
 * @param process_msg_param An integer parameter passed to process_chord_msg() to
 *                          identify the context or message type being processed.
 *                          This is used internally by the message processing logic.
 * @param expected_type The expected ChordMessage__MsgCase type that indicates the
 *                     desired response message type. The function will continue
 *                     polling until a message of this type is received.
 * 
 * @return MessageResponse The response message received. If the expected message type
 *                         is received, response.type will match expected_type. If a
 *                         timeout occurs or an error happens, response.type will be
 *                         CHORD_MESSAGE__MSG__NOT_SET or another value, and the caller
 *                         should check response.type to determine if the expected
 *                         message was received.
 * 
 * @note This function uses the global sockfd variable for the socket file descriptor.
 *       The function implements a timeout mechanism: it will poll for up to 1 second
 *       total, checking for socket data availability every 1 second using select().
 *       If select() fails, the function breaks out of the loop and returns the current
 *       response (which will have type CHORD_MESSAGE__MSG__NOT_SET if no message was
 *       received).
 * 
 * @warning The function may return a response with a type different from expected_type
 *          if a timeout occurs or an error happens. Callers must check response.type
 *          to verify that the expected message was actually received before using
 *          the response data.
 */
static MessageResponse wait_for_response(int process_msg_param, ChordMessage__MsgCase expected_type) {
	time_t currTime = time(NULL);
	time_t timeoutTime = time(NULL) + 1;

	MessageResponse response = {.type = CHORD_MESSAGE__MSG__NOT_SET};
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
			response = process_chord_msg(process_msg_param);
		}

		currTime = time(NULL);
	} while (currTime < timeoutTime && response.type != expected_type);

	return response;
}

/**
 * @brief Packs a ChordMessage into a network-ready buffer.
 * 
 * This helper function takes a ChordMessage structure, calculates its packed size,
 * allocates a buffer with space for the message length prefix, converts the length
 * to network byte order, and packs the message into the buffer. The buffer includes
 * a 64-bit length prefix followed by the packed message data.
 * 
 * @param msg Pointer to the ChordMessage structure to pack. The message must be
 *            properly initialized with all required fields set before calling
 *            this function.
 * @param total_size Output parameter that will be set to the total size of the
 *                   allocated buffer (including the length prefix).
 * 
 * @return uint8_t* Pointer to the allocated buffer containing the packed message.
 *                  The buffer contains:
 *                  - First 8 bytes: message length in network byte order
 *                  - Remaining bytes: the packed ChordMessage data
 *                  The caller is responsible for freeing this buffer with free().
 * 
 * @note The function allocates memory using malloc(). The caller must free the
 *       returned buffer to avoid memory leaks.
 * 
 * @warning If memory allocation fails, malloc() will return NULL and the function
 *          will return NULL. Callers should check for NULL return values before
 *          using the buffer.
 */
static uint8_t *pack_chord_message(ChordMessage *msg, size_t *total_size) {
	uint64_t msg_len = chord_message__get_packed_size(msg);
	
	*total_size = sizeof(uint64_t) + msg_len;
	uint8_t *buffer = malloc(*total_size);
	if (!buffer) {
		return NULL;
	}
	
	uint64_t networkLen = htobe64(msg_len);
	memcpy(buffer, &networkLen, sizeof(networkLen));
	chord_message__pack(msg, buffer + sizeof(networkLen));
	
	return buffer;
}


// help functions are provided above, you may use them in the following functions
// but you should not modify them

void stabilize() {
    // TODO:
    // Ask successor for its predecessor, update if necessary, and notify
	GetPredecessorRequest request = GET_PREDECESSOR_REQUEST__INIT;
	ChordMessage msg = CHORD_MESSAGE__INIT;
	MessageResponse response;
	size_t size;
	uint8_t* buffer;

	msg.version = 417;
	msg.get_predecessor_request = &request;
	msg.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST;

	buffer = pack_chord_message(&msg, &size);
	send_to_node(&successor, buffer, size, "error sending predecessor");
	response = wait_for_response(2, CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE);

	if(response.type == CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE
		&& response.node.key != 0 && element_of(response.node.key, hash, successor.key, 0) ) { 
			successor = response.node;
			successor_list[0] = successor;
	}

	notify();
	fix_successor_list();
	free(buffer);


}

void fix_successor_list() {
	// TODO:
    // 1) If id in (n, successor], return successor
    // 2) Otherwise forward request to closest preceding node

	GetSuccessorListRequest req = GET_SUCCESSOR_LIST_REQUEST__INIT;

	ChordMessage msg = CHORD_MESSAGE__INIT;
	msg.version = 417;
	msg.find_successor_request = &req;
	msg.msg_case = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST;

	uint64_t* msg_len;

	uint8_t* buffer = pack_chord_message(&msg, msg_len);
	send_to_node(&successor, buffer, msg_len, NULL);
	MessageResponse resp = wait_for_response(3, CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE);

	if (resp.type == CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE) {
		size_t num_entries = resp.n_successors;

		// Getting rid of original list
		free(successor_list);
		
		// Creating new one
		successor_list = malloc(sizeof(Node) * (num_entries + 1));
		successor_list[0] = successor; // Adding succesor to the start

		// Filling the rest out with successor node's succesors
		for (int i = 0; i < num_entries; i++) {
			successor_list[i + 1] = resp.successors[i];
		}
	}
}

void fix_fingers() {
    // TODO:
    // Periodically rebuild finger[i]
	fixIndex = fixIndex + 1;
	
    if (fixIndex >= M) {
        fixIndex = 0;
    }
    finger_table[fixIndex] = find_successor(hash + ((uint64_t)1 << fixIndex));
	
}

Node find_successor(uint64_t id) {	
    // 1) If id in (n, successor], return successor
	if (element_of(id, hash, successor.key, 1)) {
		return successor;
	}
    // 2) Otherwise forward request to closest preceding node
	else {
		Node n_bar = closest_preceding_node(id);

		// Using StartFindSuccessor should make it recursive, because it'll
		// have the node it's messaging call find_successor
		StartFindSuccessorRequest req = START_FIND_SUCCESSOR_REQUEST__INIT;
		req.key = id;
		ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.version = 417;
		msg.find_successor_request = &req;
		msg.msg_case = CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_REQUEST;

		uint64_t* msg_len;

		uint8_t* buffer = pack_chord_message(&msg, msg_len);
		send_to_node(&n_bar, buffer, msg_len, NULL);
		MessageResponse resp = wait_for_response(4, CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE);
		
		free(buffer);

		if (resp.type == CHORD_MESSAGE__MSG_START_FIND_SUCCESSOR_RESPONSE) {
			return resp.node; 
		} else {
			printf("Wrong Message Type!\n");
		}
	}
}

Node closest_preceding_node(uint64_t id) {
    // TODO:
    // Scan finger table for closest predecessor
	for (int i = M - 1; i >= 0; i--) {
        if (finger_table[i].key != 0 && element_of(finger_table[i].key, hash, id, 0)) {
            return finger_table[i];
        }
    }

	return self;
}

