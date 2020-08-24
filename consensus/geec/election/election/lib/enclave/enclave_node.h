//
// Created by Michael Xusheng Chen on 11/4/2018.
//

#ifndef LEADER_ELECTION_NODE_H
#define LEADER_ELECTION_NODE_H

// api for leader election

#include "../include/metadata.h"

// Term_t* New_Node_fake(int offset, uint64_t start_blk, uint64_t term_len);

void* api_new_node(Term_t* t, char **ipstrs, int *ports, int offset, char *my_account, int commitee_count, uint64_t start_blk, uint64_t term_len);

int api_elect(Term_t *t, uint64_t blk, uint64_t *rand, int timeoutMs);

int api_kill_group(Term_t *term);

int event_handle_prepare(Message *msg, void *si_other, Term_t *term, unsigned int si_len);

int event_handle_prepared(Message *msg, void *si_other, Term_t *term);

int event_handle_confirm(Message *msg, void *si_other, Term_t *term, unsigned int si_len);

int event_handle_confirmed(Message *msg, void *si_other, Term_t *term);

int event_handle_notify(Message *msg, void *si_other, Term_t *term);

int event_handle_announce(Message *msg, void *si_other, Term_t *term);


#endif //LEADER_ELECTION_NODE_H
