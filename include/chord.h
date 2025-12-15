#ifndef CHORD_H
#define CHORD_H

#include <inttypes.h>

#include "chord.pb-c.h"
#include <endian.h>
#include <math.h>

// Length of a Chord Node or item key
static const uint8_t KEY_LEN = 8;

static const int M = 64;

/**
 * @brief Used to send messages to other Chord Nodes.
 * 
 * NOTE: Remember, you CANNOT send pointers over the network!
 */
typedef struct Message
{
    uint64_t len;
    void *ChordMessage;
} Message;

typedef struct MessageResponse {
    ChordMessage__MsgCase type;
    Node node;
    size_t n_successors;
    Node *successors;
} MessageResponse;

void create();

void join();

void notify();

void stabilize();

void fix_successor_list();

void fix_fingers();

void check_predecessor();

Node find_successor(uint64_t id);

Node closest_preceding_node(uint64_t id);

MessageResponse process_chord_msg(int i);

void lookup(uint64_t key);

void print_state();

int element_of(uint64_t curr_var, uint64_t r1, uint64_t r2, int is_inclusive);

uint64_t get_hash(struct sockaddr_in *addr);

void get_local_address(struct sockaddr_in *addr);

/**
 * @brief Print out the node or item key.
 * 
 * NOTE: You are not obligated to utilize this function, it is just showing
 * you how to properly print out an unsigned 64 bit integer.
 */
void printKey(uint64_t key);

void process_input(char *input);

void cleanup();

#endif // CHORD_H

