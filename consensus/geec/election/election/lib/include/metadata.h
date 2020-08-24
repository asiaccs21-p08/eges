//
// Created by jianyu on 5/18/18.
//

#ifndef NODE_METADATA_H
#define NODE_METADATA_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "sync_header.h"

#define DEBUG 1
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__, __FILE__, __LINE__); } while (0)

#define INFO 1
#define info_print(fmt, ...) \
            do { if (INFO) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define BUFLEN 1024
#define MSG_LEN 41

#define ELEC_PREPARE 1
#define ELEC_PREPARED 2
#define ELEC_CONFIRM 3
#define ELEC_CONFIRMED 4
#define ELEC_ANNOUNCE 5  //only for debug usage.
#define ELEC_NOTIFY 6 //Notify a proposer about the history before.

struct Message{
    uint64_t rand;
    uint64_t blockNum;
    uint8_t message_type;
    uint32_t sender_idx;    //The msg is for whom.
    char addr[20];    //Sender of the msg.
};
typedef struct Message Message;

enum state_t{
    STATE_EMPTY = 0,
    //propose
            STATE_PREPARE_SENT = 1,
    STATE_CONFIRM_SENT = 2,
    STATE_ELECTED = 3,
    STATE_TRANFERRED = 4,
    //follower
            STATE_PREPARED = 5,
    STATE_CONFIRMED = 6
};

#define __SIZEOF_PTHREAD_CONT_T 48
#define __SIZEOF_PTHREAD_MUTEX_T 40

struct instance_t{
    enum state_t state;
    uint64_t max_rand;
    uint32_t max_member_idx;
    char addr[20];
    char **prepared_addr;
    int prepared_addr_count;
    char **confirmed_addr;
    int confirmed_addr_count;
    //pthread_spinlock_t lock;
    int try_count;
    uint64_t my_rand;
    union {
        pmutex_t lock;
        char padding[__SIZEOF_PTHREAD_MUTEX_T];
    } state_lock;
    union {
        pcond_t cond;
        char padding[__SIZEOF_PTHREAD_CONT_T];
    } cond;
};


typedef struct instance_t instance_t;

struct Term_t{
    uint64_t start_block;
    uint64_t len;
    uint64_t cur_block;
    struct sockaddr_in *members;
    uint64_t member_count;
    uint32_t my_idx;
    //Protocol state.
    instance_t *instances; //size is len;'

    char my_account[21]; //trails with '\0'
    int sock;
    int should_stop;
    int is_stoped;
    pthread_id recvt;
    pmutex_t flag_lock;
};

typedef struct Term_t Term_t;

static char* serialize(const Message *msg){
    char *output = malloc(MSG_LEN * sizeof(char));
    memcpy(&output[0], &msg->rand, 8);
    memcpy(&output[8], &msg->blockNum, 8);
    memcpy(&output[16], &msg->message_type, 1);
    memcpy(&output[17], &msg->sender_idx, 4);
    memcpy(&output[21], msg->addr, 20);
    return output;
}

static Message deserialize(char *input){
    Message msg;
    memcpy(&msg.rand, &input[0], 8);
    memcpy(&msg.blockNum, &input[8], 8);
    memcpy(&msg.message_type, &input[16], 1);
    memcpy(&msg.sender_idx, &input[17], 4);
    memcpy(&msg.addr, &input[21], 20);
    return msg;
}

#endif //NODE_METADATA_H
