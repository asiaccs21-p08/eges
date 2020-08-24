cmake_minimum_required(VERSION 3.5)
project(node C)

set(CMAKE_C_STANDARD 99)

# SGX setting

if(NOT DEFINED ENV{SGX_SDK})
    set(ENV{SGX_SDK} /opt/intel/sgxsdk/)
endif()

if(NOT DEFINED ENV{SGX_SDK})
    message(FATAL_ERROR "$SGX_SDK environment variable must be set.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH} $ENV{SGX_SDK}/lib")
    set(SGX_ENCLAVE_SIGNER "$ENV{SGX_SDK}/bin/x86/sgx_sign")
    set(SGX_EDGER8R "$ENV{SGX_SDK}/bin/x86/sgx_edger8r")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "$ENV{SGX_SDK}/lib64")
    set(SGX_ENCLAVE_SIGNER "$ENV{SGX_SDK}/bin/x64/sgx_sign")
    set(SGX_EDGER8R "$ENV{SGX_SDK}/bin/x64/sgx_edger8r")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENCLAVE -O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENCLAVE -O2")
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE} -DENCLAVE -O2 -DPERF")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(include)
include_directories("$ENV{SGX_SDK}/include")

add_custom_command(
        COMMAND ${SGX_EDGER8R} --untrusted enclave/election.edl --search-path enclave/ --search-path "$ENV{SGX_SDK}/include"
        DEPENDS enclave/election.edl
        OUTPUT election_u.c election_u.h)

add_subdirectory(enclave)

set(SOURCES
        node_app.c
        election_u.c
        ocall_election.h
        ocall_election.c
        enclave_manager.c
        include/metadata.h
        include/sync_header.h)

find_library(UKEY_EXCHANGE_LIB sgx_ukey_exchange)
find_library(URTS_LIB sgx_urts)
find_library(URTS_SIM_LIB sgx_urts_sim)
find_library(UAE_SERVICE_LIB sgx_uae_service)
find_library(UAE_SERVICE_SIM_LIB sgx_uae_service_sim)

find_library(DL dl)


add_library(node_sgx_sim ${SOURCES} enclave_wrapper.h)

target_link_libraries(node_sgx_sim ${UKEY_EXCHANGE_LIB} pthread)
target_link_libraries(node_sgx_sim ${DL} dl)
target_link_libraries(node_sgx_sim ${URTS_SIM_LIB} ${UAE_SERVICE_SIM_LIB})

if(DEFINED ENV{SGX_HW})
add_library(node_sgx_hw ${SOURCES})
target_compile_definitions(node_sgx_hw PUBLIC -DSGX_HW)
target_link_libraries(node_sgx_hw ${UKEY_EXCHANGE_LIB} pthread)
target_link_libraries(node_sgx_hw ${DL} dl)
target_link_libraries(node_sgx_hw ${URTS_LIB} ${UAE_SERVICE_LIB})
endif()

add_library(node_sgx_fake node_app.c enclave/enclave_node.c)
target_compile_definitions(node_sgx_fake PUBLIC -DSGX_FAKE)
#target_link_libraries(node Threads::Threads)
