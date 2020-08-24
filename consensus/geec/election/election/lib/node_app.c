//
// Created by Maxxie Jiang on 19/5/2018.
//

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <metadata.h>
#include "node.h"
#include "include/metadata.h"
#include "include/sync_header.h"

#ifndef SGX_FAKE
#include "enclave_manager.h"
#include "election_u.h"

#define enclave_api_new_node api_new_node
#define enclave_api_elect api_elect
#define enclave_api_kill_group api_kill_group
#define enclave_event_handle_prepare event_handle_prepare
#define enclave_event_handle_prepared event_handle_prepared
#define enclave_event_handle_confirm event_handle_confirm
#define enclave_event_handle_confirmed event_handle_confirmed
#define enclave_event_handle_announce event_handle_announce
#define enclave_event_handle_notify event_handle_notify

#else
#include "enclave_wrapper.h"
#endif

static void init_instance(Term_t *term, instance_t *instance){
    pcond_init(&instance->cond.cond);
    plock_init(&instance->state_lock.lock);

    instance->max_rand = 0;
    instance->state = STATE_EMPTY;
    instance->confirmed_addr_count = 0;
    instance->prepared_addr_count = 0;
    instance->try_count = 0;


    instance->prepared_addr=(char**)malloc(term->member_count * sizeof(char*));
    instance->confirmed_addr=(char**)malloc(term->member_count * sizeof(char*));
    int j = 0;
    for (j = 0; j<term->member_count; j++){
        instance->prepared_addr[j] = (char *)malloc(20 * sizeof(char));
        instance->confirmed_addr[j] = (char *)malloc(20 * sizeof(char));
    }
}

static void *RecvFunc(void *opaque);

char**makeCharArray(int size) {
    return calloc(sizeof(char*), size);
}

void freeCharArray(char **a, int size) {
    int i;
    for (i = 0; i < size; i++)
        free(a[i]);
    free(a);
}

void setArrayString(char **a, char *s, int n) {
    a[n] = s;
}

Term_t* New_Node(char **ipstrs, int *ports, int offset, char *my_account,
                 int commitee_count, uint64_t start_blk, uint64_t term_len) {
    init_enclave();
    Term_t *ret = NULL;
    Term_t *term = NULL;
    term = (Term_t*)malloc(sizeof(Term_t));
    sgx_status_t status = enclave_api_new_node(enclave_id, &ret, term, ipstrs, ports, offset, my_account,
                 commitee_count, start_blk, term_len);
    if (status != SGX_SUCCESS) {
        return NULL;
    }

    term = ret;

    term->instances = (instance_t *) malloc(term->len * sizeof(instance_t));
    for (int i = 0; i < term->len; i++){
        init_instance(term, &term->instances[i]);
    }

    term->members = (struct sockaddr_in *)malloc(commitee_count * sizeof(struct sockaddr_in));
    uint32_t x;
    for (x =0; x < commitee_count; x++){
        memset(&term->members[x], 0, sizeof(struct sockaddr_in));
        term->members[x].sin_family = AF_INET;
        term->members[x].sin_port = htons(ports[x]);
        term->members[x].sin_addr.s_addr = inet_addr(ipstrs[x]);
    }

    term->sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (term->sock < 0){
        perror("Failed to create socket\n");
        pthread_exit(NULL);
    }
    if (bind(term->sock, (struct sockaddr*)&term->members[offset], sizeof(term->members[offset])) == -1){
        perror("Failed to bind to socket\n");
        pthread_exit(NULL);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(term->sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }

    term->should_stop = 0;
    term->is_stoped = 0;
    pthread_mutex_init(&term->flag_lock, NULL);

    int p_ret = pthread_create(&term->recvt, NULL, RecvFunc, (void *)term);
    if (p_ret != 0) {
        perror("Failed to create thread");
        return NULL;
    }
    return term;
}

int elect(Term_t *term, uint64_t blk, uint64_t *rand, int timeoutMs) {
    int ret = 0;
    sgx_status_t status = enclave_api_elect(enclave_id, &ret, term, blk, rand, timeoutMs);
    if (status != SGX_SUCCESS) {
        // todo: return what?
        fprintf(stderr, "error in ecall %d\n", status);
        return -1;
    }

    // wait until we get a result
    if (ret >= 2) {
        struct timespec timeToWait;
        struct timeval now;
        int rt;
        gettimeofday(&now, NULL);
        timeToWait.tv_sec = now.tv_sec+0;
        timeToWait.tv_nsec = (now.tv_usec+1000UL*timeoutMs)*1000UL;
        if (timeToWait.tv_nsec > 1000000000UL){
            timeToWait.tv_nsec -= 1000000000UL;
            timeToWait.tv_sec += 1;
        }

        instance_t *instance = &term->instances[blk - term->start_block];
        int old_state = ret - 2;
        plock(&instance->state_lock.lock);
        if (old_state == instance->state) {
            rt = pthread_cond_timedwait(&instance->cond.cond, &instance->state_lock.lock, &timeToWait);
        }
        int state  = instance->state;
        punlock(&instance->state_lock.lock);
        if (state == STATE_ELECTED) {
            info_print("[Election] as leader for block %lu\n", blk);
            return 1;
        }
        else if (state == STATE_PREPARE_SENT || state == STATE_CONFIRM_SENT){
            return -1;
        }
        else{
            info_print("[Election] for block %lu failed\n", blk);
            return 0;
        }
    }

    return ret;
}

int killGroup(Term_t *term) {
    int ret;
    sgx_status_t status = enclave_api_kill_group(enclave_id, &ret, term);
    if (status != SGX_SUCCESS) {
        // todo: return what?
        return -1;
    }
    close(term->sock);
    term->should_stop = 1;
    while (!term->is_stoped) ;
    free(term);
    return ret;
}

static void *RecvFunc(void *opaque) {
    Term_t *term = (Term_t *)opaque;
    int s = term->sock;
    /*
     * debug
     */
    struct sockaddr_in foo;
    int len = sizeof(struct sockaddr_in);
    getsockname(s,  (struct sockaddr *) &foo, &len);
    fprintf(stderr, "Thread Receving network packets, listening on %s:%d, start_blk = %lu, len = %lu\n",inet_ntoa(foo.sin_addr),
            ntohs(foo.sin_port), term->start_block, term->len);

    char buffer[1024];
    int recv_len;
    struct sockaddr_in si_other;
    socklen_t si_len = sizeof(si_other);
    while(1){
        pthread_mutex_lock(&term->flag_lock);
        if (term->should_stop){
            pthread_mutex_unlock(&term->flag_lock);
            term->is_stoped = 1;
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&term->flag_lock);

        recv_len = recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr *)&si_other, &si_len);
        if (recv_len != MSG_LEN){
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                continue;
            }
            fprintf(stderr, "Wrong Message Format\n");
            continue;
        }
        Message msg = deserialize(buffer);
        int ret_broadcast = 0;
        switch(msg.message_type){
            case ELEC_PREPARE :
                enclave_event_handle_prepare(enclave_id, &ret_broadcast, &msg, &si_other, term, si_len);
                break;
            case ELEC_PREPARED :
                enclave_event_handle_prepared(enclave_id, &ret_broadcast, &msg, &si_other, term);
                break;
            case ELEC_CONFIRM :
                enclave_event_handle_confirm(enclave_id, &ret_broadcast, &msg, &si_other, term, si_len);
                break;
            case ELEC_CONFIRMED :
                enclave_event_handle_confirmed(enclave_id, &ret_broadcast, &msg, &si_other, term);
                break;
            case ELEC_ANNOUNCE :
                enclave_event_handle_announce(enclave_id, &ret_broadcast, &msg, &si_other, term);
                break;
            case ELEC_NOTIFY:
                enclave_event_handle_notify(enclave_id, &ret_broadcast, &msg, &si_other, term);
                break;
        }
        if (ret_broadcast > 0) {
            instance_t *instance = &term->instances[ret_broadcast - 1];
            pcond_broadcast(&instance->cond.cond);
        }

    }
}