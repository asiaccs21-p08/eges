//
// Created by jianyu on 5/20/18.
//

#ifndef NODE_ENCLAVE_WRAPPER_H
#define NODE_ENCLAVE_WRAPPER_H

#include <stddef.h>
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>
#include "include/metadata.h"
#include "enclave/enclave_node.h"

// A fake implementation of enclave
typedef int sgx_status_t;
typedef int sgx_enclave_id_t;
#define SGX_SUCCESS 1

#define init_enclave()

sgx_enclave_id_t enclave_id;

sgx_status_t enclave_api_new_node(sgx_enclave_id_t eid, void** retval, Term_t* term, char** ipstrs, int* ports, int offset,
                          char* my_account, int commitee_count, uint64_t start_blk, uint64_t term_len) {
    *retval = api_new_node(term, ipstrs, ports, offset, my_account, commitee_count, start_blk, term_len);
    return SGX_SUCCESS;
}
sgx_status_t enclave_api_elect(sgx_enclave_id_t eid, int* retval, Term_t* term, uint64_t blk, uint64_t* rand, int timeout) {
    *retval = api_elect(term, blk, rand, timeout);
    return  SGX_SUCCESS;
}
sgx_status_t enclave_api_kill_group(sgx_enclave_id_t eid, int* retval, Term_t* term) {
    *retval = api_kill_group(term);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_prepare(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term, unsigned int si_len) {
    *retval = event_handle_prepare(msg, si_other, term, si_len);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_prepared(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term) {
    *retval = event_handle_prepared(msg, si_other, term);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_confirm(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term, unsigned int si_len) {
    *retval = event_handle_confirm(msg, si_other, term, si_len);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_confirmed(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term) {
    *retval = event_handle_confirmed(msg, si_other, term);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_notify(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term) {
    *retval = event_handle_notify(msg, si_other, term);
    return SGX_SUCCESS;
}
sgx_status_t enclave_event_handle_announce(sgx_enclave_id_t eid, int* retval, Message* msg, void* si_other, Term_t* term) {
    *retval = event_handle_announce(msg, si_other, term);
    return SGX_SUCCESS;
}

#endif //NODE_ENCLAVE_WRAPPER_H
