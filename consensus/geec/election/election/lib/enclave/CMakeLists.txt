cmake_minimum_required(VERSION 2.8)

project(LeaderElectionTrust)

set(SOURCES
        enclave_sys.h
        enclave_node.c
        election_t.c)

add_custom_command(
        COMMAND ${SGX_EDGER8R} --trusted election.edl --search-path ./ --search-path "$ENV{SGX_SDK}/include"
        DEPENDS election.edl
        OUTPUT election_t.c election_t.h)

include_directories(SYSTEM "$ENV{SGX_SDK}/include/libcxx")
include_directories(SYSTEM "$ENV{SGX_SDK}/include/tlibc")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -fvisibility=hidden -fpie -fstack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -nostdinc++")
set(ENCLAVE_LINK_FLAGS "-Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined -Wl,-pie,-eenclave_entry -Wl,--export-dynamic -Wl,--defsym,__ImageBase=0")


add_library(enclave_trusted_sim SHARED ${SOURCES})
set_property(TARGET enclave_trusted_sim PROPERTY POSITION_INDEPENDENT_CODE OFF)
set_target_properties(enclave_trusted_sim PROPERTIES LINK_FLAGS ${ENCLAVE_LINK_FLAGS})

find_library(TRTS_LIB sgx_trts)
find_library(TRTS_SIM_LIB sgx_trts_sim)
find_library(TSTDC_LIB sgx_tstdc)
find_library(TSTDCXX_LIB sgx_tcxx)
find_library(TKEY_EXCHANGE_LIB sgx_tkey_exchange)
find_library(TCRYPTO_LIB sgx_tcrypto)
find_library(SERVICE_LIB sgx_tservice)
find_library(SERVICE_SIM_LIB sgx_tservice_sim)

set(SIM_Trts_Library_Path "${TRTS_SIM_LIB}")
set(SIM_Service_Library_Path "${SERVICE_SIM_LIB}")
set(Trts_Library_Path "${TRTS_LIB}")
set(Service_Library_Path "${SERVICE_LIB}")

add_library(enclave_trusted_hw SHARED ${SOURCES})
set_property(TARGET enclave_trusted_hw PROPERTY POSITION_INDEPENDENT_CODE OFF)
set_target_properties(enclave_trusted_hw PROPERTIES LINK_FLAGS ${ENCLAVE_LINK_FLAGS})

target_link_libraries(enclave_trusted_hw -Wl,--whole-archive "${Trts_Library_Path}" -Wl,--no-whole-archive -Wl,--start-group "${TSTDC_LIB}" "${TSTDCXX_LIB}"
        "${TKEY_EXCHANGE_LIB}" "${TCRYPTO_LIB}" "${Service_Library_Path}" -Wl,--end-group)

add_custom_command(
        COMMAND ${SGX_ENCLAVE_SIGNER} sign -key "$ENV{SGX_SDK}/SampleCode/SampleEnclave/Enclave/Enclave_private.pem" -enclave $<TARGET_FILE:enclave_trusted_hw> -out libenclave_trusted_signed_hw.so -config enclave.config.xml
        DEPENDS enclave_trusted_hw enclave.config.xml
        OUTPUT libenclave_trusted_signed_hw.so)

target_link_libraries(enclave_trusted_sim -Wl,--whole-archive "${SIM_Trts_Library_Path}" -Wl,--no-whole-archive -Wl,--start-group "${TSTDC_LIB}" "${TSTDCXX_LIB}"
        "${TKEY_EXCHANGE_LIB}" "${TCRYPTO_LIB}" "${SIM_Service_Library_Path}" -Wl,--end-group)

add_definitions(-DENCLAVE)


add_custom_command(
        COMMAND ${SGX_ENCLAVE_SIGNER} sign -key "$ENV{SGX_SDK}/SampleCode/SampleEnclave/Enclave/Enclave_private.pem" -enclave $<TARGET_FILE:enclave_trusted_sim> -out libenclave_trusted_signed_sim.so -config enclave.config.xml
        DEPENDS enclave_trusted_sim enclave.config.xml
        OUTPUT libenclave_trusted_signed_sim.so)

if(DEFINED ENV{SGX_HW})
    add_custom_target(enclave_trusted_signed_target_hw ALL DEPENDS libenclave_trusted_signed_hw.so)
    install(FILES libenclave_trusted_signed_hw.so DESTINATION /usr/lib/)
endif()

add_custom_target(enclave_trusted_signed_target_sim ALL DEPENDS libenclave_trusted_signed_sim.so)
install(FILES libenclave_trusted_signed_sim.so DESTINATION /usr/lib/)