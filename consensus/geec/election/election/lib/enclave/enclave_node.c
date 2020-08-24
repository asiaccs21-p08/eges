//
// Created by Michael Xusheng Chen on 11/4/2018.
//
#include "enclave_node.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "enclave_sys.h"

static int broadcast(const Message *msg,Term_t *term);
static int insert_addr(char **addr_array, const char *addr,  int *count);
static int send_to_member(int index, const Message* msg, Term_t *term);
static void free_instance(Term_t *term, instance_t *instance);

void break_enclave() {

}
// Term_t* New_Node(int offset, uint64_t start_blk, uint64_t len){ //currently for hardcoded message.
void* api_new_node(Term_t* term, char **ipstrs, int *ports, int offset, char *my_account, int committee_count, uint64_t start_blk, uint64_t term_len){
    debug_print("Creating node with %d nodes:\n", committee_count);
    int i;
    for (i = 0; i<committee_count; i++) {
        debug_print("     node: %d, ip: %s, port: %d\n", i, ipstrs[i], ports[i]);
    }

    rand_init();

//    term = (Term_t*)malloc(sizeof(Term_t));
    term->my_idx = (uint32_t)offset;
    term->start_block = start_blk;
    term->len = term_len;
    //hard_coded;

    term->member_count = committee_count;

    memcpy(term->my_account, my_account, 21);

    return term;
}

int event_handle_prepare(Message *msg, void* s, Term_t *term, unsigned int si_len){
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    debug_print("[%d] Received Prepare message, blk = %ld, from = %d, rand = %lu, my current state = %d\n", term->my_idx, msg->blockNum, msg->sender_idx, msg->rand, instance->state);

    int socket = term->sock;
    plock(&instance->state_lock);
    if (instance->state == STATE_EMPTY || instance->state == STATE_PREPARED || instance->state == STATE_PREPARE_SENT) {
        if (msg->rand > instance->max_rand) {

            memcpy(instance->addr, msg->addr, 20);
            instance->max_rand = msg->rand;
            instance->max_member_idx = msg->sender_idx;
            //Prepared for a proposal with higher ballot.
            //If the current node is electing for this instance,
            //it should have failed.
            //Notify the electing thread with the news.

            instance->state = STATE_PREPARED;
            pcond_broadcast(&instance->cond);

            Message resp;
            resp.rand = msg->rand;
            resp.blockNum = msg->blockNum;
            resp.message_type = ELEC_PREPARED;
            resp.sender_idx = term->my_idx;
            memcpy(resp.addr, term->my_account, 20);
            char *output = serialize(&resp);
            debug_print("[%d] Sending PrepareED to [%d], rand = %lu\n", term->my_idx, msg->sender_idx, msg->rand);
            if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1) {
                perror("Failed to send resp");
            }
            free(output);
        }else{
            debug_print("[%d] Not sending PrepareD, blk = %ld, from = %d, rand = %lu, my current state = %d, my current max_rand= %lu\n", term->my_idx, msg->blockNum, msg->sender_idx, msg->rand, instance->state, instance->max_rand);
        }
    }
    if (instance->state == STATE_PREPARED && msg->sender_idx == instance->max_member_idx && msg->sender_idx != term->my_idx){
        //resend
        Message resp;
        resp.rand = msg->rand;
        resp.blockNum = msg->blockNum;
        resp.message_type = ELEC_PREPARED;
        resp.sender_idx = term->my_idx;
        memcpy(resp.addr, term->my_account, 20);
        char *output = serialize(&resp);
        debug_print("[%d] reSending PrepareED to [%d], rand = %lu\n", term->my_idx, msg->sender_idx, msg->rand);
        if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1) {
            perror("Failed to send resp");
        }
        free(output);
    }


    if (instance->state == STATE_ELECTED){
        Message resp;
        resp.rand = instance->max_rand;
        resp.message_type = ELEC_NOTIFY;
        resp.sender_idx = term->my_idx;
        memcpy(resp.addr, term->my_account, 20);
        char *output = serialize(&resp);
        debug_print("[%d] Sending Notify to [%d], rand = %lu\n",term->my_idx,  msg->sender_idx, msg->rand);
        if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1) {
            perror("Failed to send resp");
        }
        free(output);
    }
    if (instance->state == STATE_CONFIRM_SENT || instance->state == STATE_TRANFERRED){
        if (msg->rand > instance->max_rand){
            debug_print("[%d] transfering my current vote %d , blk = %lu, count = %d\n", term->my_idx, msg->sender_idx, msg->blockNum, instance->confirmed_addr_count);
            instance->state = STATE_TRANFERRED;
            instance->max_rand = msg->rand;
            instance->max_member_idx = msg->sender_idx;
            pcond_broadcast(&instance->cond);
            int i;
            Message resp;
            resp.rand = instance->max_rand;
            resp.message_type = ELEC_PREPARED;
            resp.sender_idx = term->my_idx;

            for (i = 0; i<instance->confirmed_addr_count; i++){
                debug_print("[%d] Transferring my vote[%d] to [%d], total vote = %d\n", term->my_idx, i, msg->sender_idx, instance->confirmed_addr_count);
                memcpy(resp.addr, instance->confirmed_addr[i], 20);
                char *output = serialize(&resp);
                if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1) {
                    perror("Failed to send resp");
                }
                free(output);
            }
        }
    }


    if (instance->state == STATE_TRANFERRED && msg->sender_idx == instance->max_member_idx){
        debug_print("[%d] Resending my current vote %d , blk = %lu, count = %d\n", term->my_idx, msg->sender_idx, msg->blockNum, instance->confirmed_addr_count);
        int i;
        Message resp;
        resp.rand = instance->max_rand;
        resp.message_type = ELEC_PREPARED;
        resp.sender_idx = term->my_idx;

        for (i = 0; i<instance->confirmed_addr_count; i++){
            debug_print("[%d] ReTransferring my vote[%d] to [%d], total vote = %d\n", term->my_idx, i, msg->sender_idx, instance->confirmed_addr_count);
            memcpy(resp.addr, instance->confirmed_addr[i], 20);
            char *output = serialize(&resp);
            if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1) {
                perror("Failed to send resp");
            }
            free(output);
        }
    }

    punlock(&instance->state_lock);
    return ret_broadcast;
}

int event_handle_prepared(Message *msg, void* s, Term_t *term) {
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    int socket = term->sock;
    debug_print("[%d] Received PrepareD message from %d, current state = %d, blk = %ld\n", term->my_idx, msg->sender_idx, instance->state, msg->blockNum);

    plock(&instance->state_lock);

    if (instance->state == STATE_PREPARE_SENT) {
        int ret = insert_addr(instance->prepared_addr, msg->addr, &instance->prepared_addr_count);
        debug_print("[%d] prepared count = %d\n", term->my_idx, instance->prepared_addr_count);
        if (ret != 0) {
            punlock(&instance->state_lock);
            return ret_broadcast;
        }
        if (instance->prepared_addr_count > term->member_count / 2) {
            memcpy(instance->confirmed_addr[0], term->my_account, 20);
            instance->confirmed_addr_count = 1;
            Message resp;
            memcpy(resp.addr, term->my_account, 20);
            resp.message_type = ELEC_CONFIRM;
            resp.blockNum = msg->blockNum;
            resp.rand = msg->rand;
            resp.sender_idx = term->my_idx;
            ret = broadcast(&resp, term);
            if (ret != 0) {
                perror("failed to broadcast Confirm message");
            }
            //The node is still potentially ``in-control'', No need to notify.
            instance->state = STATE_CONFIRM_SENT;
        }
    }
    punlock(&instance->state_lock);
    return ret_broadcast;
}

int event_handle_confirm(Message *msg, void* s, Term_t *term, unsigned int si_len) {
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    debug_print("[%d] Received Confirm Msg from %d, blk = %lu, my current state = %d\n", term->my_idx, msg->sender_idx, msg->blockNum, instance->state);

    int socket = term->sock;
    plock(&instance->state_lock);
    if (instance-> max_rand > msg->rand){
        debug_print("[%d] Already prepared to larger rand, Not answering confirm, blk =%lu\n", term->my_idx, msg->blockNum);
        punlock(&instance->state_lock);
        //already prepared a higher.
        return ret_broadcast;
    }


    if (instance->state == STATE_ELECTED) {
        debug_print("[%d] I am the leader, not answering confirm, blk =%lu\n", term->my_idx, msg->blockNum);
        punlock(&instance->state_lock);
        return ret_broadcast;
    }

    if (instance ->state == STATE_CONFIRMED){
        if (msg->sender_idx == instance->max_member_idx && instance->max_member_idx != term->my_idx){
            //REceived resend message.
            Message resp;
            memcpy(resp.addr, term->my_account, 20);
            resp.rand = msg->rand;
            resp.blockNum = msg->blockNum;
            resp.message_type = ELEC_CONFIRMED;
            resp.sender_idx = term->my_idx;
            //REsend my ConfirmED message
            char *output = serialize(&resp);
            debug_print("[%d] resending confirmed to %d, blknum = %lu\n", term->my_idx, msg->sender_idx, msg->blockNum);

            if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *) si_other, si_len) == -1) {
                perror("Failed to send resp");
            }
            free(output);
            // I have go other confirmed message. transfer .
            int i;
            for (i = 0; i < instance->confirmed_addr_count; i++) {
                debug_print("[%d] Resending confirm for transferred votes to %d, blk = %lu, total vote = %d", term->my_idx, msg->sender_idx, msg->blockNum, instance->confirmed_addr_count);
                memcpy(resp.addr, instance->confirmed_addr[i], 20);
                char *output = serialize(&resp);
                if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *) si_other, si_len) == -1) {
                    perror("Failed to send resp");
                }
                free(output);
            }
        }
        punlock(&instance->state_lock);
        return ret_broadcast;
    }


    else if (instance->state == STATE_CONFIRM_SENT){
        if (msg->rand > instance->max_rand) {
            Message resp;
            instance->max_member_idx = msg->sender_idx;
            instance->max_rand = msg->rand;
            resp.rand = msg->rand;
            resp.blockNum = msg->blockNum;
            resp.message_type = ELEC_CONFIRMED;
            resp.sender_idx = term->my_idx;
            int i;
            for (i = 0; i < instance->confirmed_addr_count; i++) {
                memcpy(resp.addr, instance->confirmed_addr[i], 20);
                char *output = serialize(&resp);
                if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *) si_other, si_len) == -1) {
                    perror("Failed to send resp");
                }
                free(output);
            }
            instance->state = STATE_CONFIRMED;
            pcond_broadcast(&instance->cond);
        }
    }
    else if (instance->state == STATE_TRANFERRED) {
        Message resp;
        resp.rand = msg->rand;
        resp.blockNum = msg->blockNum;
        resp.message_type = ELEC_CONFIRMED;
        resp.sender_idx = term->my_idx;
        int i;
        for (i = 0; i<instance->confirmed_addr_count; i++){
            memcpy(resp.addr, instance->confirmed_addr[i], 20);
            char *output = serialize(&resp);
            if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *)si_other, si_len) == -1){
                perror("Failed to send resp");
            }
            free(output);
        }
        instance->state = STATE_CONFIRMED;
        pcond_broadcast(&instance->cond);
    }
    else {
        //Answering the confirm message
        Message resp;
        memcpy(resp.addr, term->my_account, 20);
        resp.rand = msg->rand;
        resp.blockNum = msg->blockNum;
        resp.message_type = ELEC_CONFIRMED;
        resp.sender_idx = term->my_idx;
        char *output = serialize(&resp);
        debug_print("[%d] Sending confirmed to %d, blknum = %lu\n", term->my_idx, msg->sender_idx, msg->blockNum);

        if (sendto(socket, output, MSG_LEN, 0, (struct sockaddr *) si_other, si_len) == -1) {
            perror("Failed to send resp");
        }
        free(output);
        instance->state = STATE_CONFIRMED;
        pcond_broadcast(&instance->cond);
    }
    punlock(&instance->state_lock);
    return ret_broadcast;
}

int event_handle_confirmed(Message *msg, void* s, Term_t *term) {
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    debug_print("[%d] Received ConfirmED Msg from %d, blk = %lu\n",term->my_idx, msg->sender_idx, msg->blockNum);
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    int socket = term->sock;
    plock(&instance->state_lock);
    if (instance->state == STATE_CONFIRM_SENT) {
        int ret = insert_addr(instance->confirmed_addr, msg->addr, &instance->confirmed_addr_count);
        if (ret != 0) {
            punlock(&instance->state_lock);
            return ret_broadcast;
        }
        if (instance->confirmed_addr_count > term->member_count / 2) {
            Message resp;
            memcpy(resp.addr, term->my_account, 20);
            resp.message_type = ELEC_ANNOUNCE;
            resp.blockNum =  msg->blockNum;
            resp.rand = msg->rand;
            resp.sender_idx = term->my_idx;
            ret = broadcast(&resp, term);
            if (ret != 0){
                perror("failed to broadcast Confirm message");
            }
            instance->state = STATE_ELECTED;
            pcond_broadcast(&instance->cond);

        }
    }
    if (instance->state == STATE_TRANFERRED){
        debug_print("[%d] Already transfer my vote to %d, giving him a prepared msg, blk =%lu\n", term->my_idx, instance->max_member_idx, msg->blockNum);
        //They also confirm to me, if I received a confirm message, I need to send confirmed msg on their behalves.
        int ret = insert_addr(instance->confirmed_addr, msg->addr, &instance->confirmed_addr_count);


        Message transfer;
        memcpy(transfer.addr, msg->addr, 20);
        transfer.message_type = ELEC_PREPARED;
        transfer.blockNum = msg->blockNum;
        transfer.rand = msg->rand;
        transfer.sender_idx = term->my_idx;

        char *buf = serialize(&transfer);

        ret = sendto(socket, buf, MSG_LEN, 0, (struct sockaddr*)&term->members[instance->max_member_idx], sizeof(struct sockaddr_in));
        free(buf);
        if (ret == -1){
            perror("Failed to send message\n");
        }
    }


    punlock(&instance->state_lock);
    return ret_broadcast;
}

int event_handle_notify(Message *msg, void* s, Term_t *term){
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    debug_print("[%d] Received Notify Msg, blk = %lu\n", term->my_idx, msg->blockNum);
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    plock(&instance->state_lock);
    instance->max_rand = msg->rand;
    instance->max_member_idx = msg->sender_idx;

    instance->state = STATE_CONFIRMED;

    pcond_broadcast(&instance->cond);
    punlock(&instance->state_lock);
    return ret_broadcast;
}

int event_handle_announce(Message *msg, void* s, Term_t *term){
    int ret_broadcast = 0;
    const struct sockaddr_in *si_other = (struct sockaddr_in*)s;
    fprintf(stderr, "[Leader] %lu %s\n", msg->blockNum, msg->addr);
    uint64_t offset = msg->blockNum - term->start_block;
    if (offset >= term->len){
        info_print("Message NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  msg->blockNum);
        return ret_broadcast;
    }
    instance_t *instance = &term->instances[offset];
    plock(&instance->state_lock);
    instance->max_rand = msg->rand;
    instance->max_member_idx = msg->sender_idx;
    if (instance->state != STATE_ELECTED) {
        instance->state = STATE_CONFIRMED;
    }
    pcond_broadcast(&instance->cond);
    punlock(&instance->state_lock);
    return ret_broadcast;
}

/*
return value:
2. Waiting
1. Elected
0. Failed
-1. Try again.

*/

int api_elect(Term_t *term, uint64_t blk, uint64_t *value, int timeoutMs){
    uint64_t offset = blk - term->start_block;
    if (offset >= term->len){
        info_print("Electing NOT for my term, start=%lu, len = %lu, blk_num = %lu\n", term->start_block, term->len,  blk);
        return -1;
    }

    instance_t *instance = &term->instances[offset];
    if (instance->state == STATE_CONFIRMED){
        info_print("[Election] for block %lu failed\n", blk);
        return 0;
    }

    //TODO: This is problematic.
    // if (instance->state == STATE_PREPARE_SENT || instance->state == STATE_CONFIRM_SENT){
    //     //re-entering this instance.

    //     info_print("[Election] for block %lu failed\n", blk);
    //     return -1;
    // }
    uint64_t r;
    if (instance->try_count== 0){
        //first try
        r = (uint64_t)rand_get();
        instance->my_rand = r;
    }else{
        r = instance->my_rand;
    }

    if ((r > instance->max_rand && instance->try_count== 0) || (instance->try_count>0 && instance->state == STATE_PREPARE_SENT) || (instance->try_count>0 && instance->state == STATE_CONFIRM_SENT))
    {

        instance->max_rand = r;
        memcpy(instance->prepared_addr[0], term->my_account, 20);

        Message msg;
        msg.blockNum = blk;

        if (instance->try_count== 0){
            instance->prepared_addr_count = 1;
            msg.message_type = ELEC_PREPARE;
            instance->state =  STATE_PREPARE_SENT;
            debug_print("[%d] Electing for blk %lu\n", term->my_idx, blk);
        }
        else if  (instance->state == STATE_PREPARE_SENT){
            msg.message_type = ELEC_PREPARE;
            instance->state =  STATE_PREPARE_SENT;
            debug_print("[%d] Retry Prepare, Current state = %d, Retry [%d] electing for block %lu; Prepared count %d, Confirmed Count %d\n", term->my_idx, instance->state, instance->try_count, blk, instance->prepared_addr_count, instance->confirmed_addr_count);
        }
        else{
            msg.message_type = ELEC_CONFIRM;
            instance->state =  STATE_CONFIRM_SENT;
            debug_print("[%d] Retry Confirm, Current state = %d, Retry [%d] electing for block %lu; Prepared count %d, Confirmed Count %d\n", term->my_idx, instance->state, instance->try_count, blk, instance->prepared_addr_count, instance->confirmed_addr_count);
        }

        msg.rand = r;
        msg.sender_idx = term->my_idx;
        memcpy(msg.addr, term->my_account, 20);

        int old_state = instance->state;

        //Sending out prepare message
        broadcast(&msg, term);
        instance->try_count++;

        int state  = instance->state;

        if (state == STATE_ELECTED) {
            info_print("[Election] as leader for block %lu\n", blk);
            return 1;
        } else if (old_state == state) {
            info_print("[Election] waiting for block %lu\n", blk);
            // wait for the result
            return 2 + old_state;
        }
        else if (state == STATE_PREPARE_SENT || state == STATE_CONFIRM_SENT){
            info_print("[Election] sent for block %lu\n", blk);
            return -1;
        }
        else{
            info_print("[Election] for block %lu failed\n", blk);
            return 0;
        }
        //else failed.
    }
    else {
        info_print("[Election] for block %lu failed\n", blk);
        return 0;
    }
}
int api_kill_group(Term_t* term) {
    free(term->members);
    uint64_t i;
    for (i = 0; i<term->len; i++){
        debug_print("[%d] Freeing Instance %lu\n",term->my_idx, i);
        free_instance(term, &term->instances[i]);
    }
    free(term->instances);
    return 1;
}

/*
helper functions.
*/


static int insert_addr(char **addr_array, const char *addr,  int *count){
    int i;
    for (i = 0; i<*count; i++){
        if (memcmp(addr_array[i], addr, 20) == 0){
            return 1; //already in the list.
        }
    }
    memcpy(addr_array[*count], addr, 20);
    *count = *count +1;
    return 0;
}

static int broadcast(const Message *msg, Term_t *term){
    int socket = term->sock;
    int i;
    ssize_t ret;
    char *buf = serialize(msg);
    for (i = 0; i<term->member_count; i++){
        ret = sendto(socket, buf, MSG_LEN, 0, (struct sockaddr*)&term->members[i], sizeof(struct sockaddr_in));
        if (ret == -1){
            perror("Failed to broadcast message\n");
            free(buf);
            return -1;
        }
    }
    free(buf);
    return 0;
}

static void free_instance(Term_t *term, instance_t *instance){

    int j;
    for (j = 0; j<term->member_count; j++){
        free(instance->prepared_addr[j]);
        free(instance->confirmed_addr[j]);
    }
    free(instance->prepared_addr);
    free(instance->confirmed_addr);
}

static int send_to_member(int index, const Message* msg, Term_t *term){
    char *buf = (char *) malloc(MSG_LEN * sizeof(char));
    int ret = sendto(term->sock, buf, MSG_LEN, 0, (struct sockaddr*)&term->members[index], sizeof(struct sockaddr_in));
    if (ret != 0){
        perror("Failed to send to member");
    }
    free(buf);
    return ret;
}
