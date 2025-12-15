#ifndef CHORD_IMPL_H
#define CHORD_IMPL_H

#include <inttypes.h>
#include "chord.h"
#include "chord.pb-c.h"

// External declarations for global variables used by these functions
extern uint64_t hash;
extern Node predecessor;
extern Node successor;
extern Node *finger_table;
extern Node *successor_list;
extern Node self;
extern int sockfd;
extern struct chord_arguments chord_args;
extern int succListIndex;
extern int fixIndex;

// External declarations for functions used by these functions
extern int element_of(uint64_t curr_var, uint64_t r1, uint64_t r2, int is_inclusive);
extern MessageResponse process_chord_msg(int i);
extern void notify(void);

// Function declarations
Node find_successor(uint64_t id);
Node closest_preceding_node(uint64_t id);
void stabilize(void);
void fix_fingers(void);
void fix_successor_list(void);

#endif // CHORD_IMPL_H

