//
// Created by Michael Xusheng Chen on 11/4/2018.
//

#ifndef LEADER_ELECTION_NODE_H
#define LEADER_ELECTION_NODE_H

// api for leader election

#include "include/metadata.h"


char**makeCharArray(int size);

void setArrayString(char **a, char *s, int n);

void freeCharArray(char **a, int size);

// Term_t* New_Node_fake(int offset, uint64_t start_blk, uint64_t term_len);

Term_t* New_Node(char **ipstrs, int *ports, int offset, char *my_account, int commitee_count, uint64_t start_blk, uint64_t term_len);

int elect(Term_t *term, uint64_t blk, uint64_t *rand, int timeoutMs);

int killGroup(Term_t *term);

#endif //LEADER_ELECTION_NODE_H
