set(EXECUTABLE cpu_generic)

file(GLOB SRC_LIST
    cpu_generic.cpp
    crypto/equihash.cpp )
file(GLOB HEADERS
    cpu_generic.hpp
    util.h
    crypto/equihash.h
    crypto/equihash.tcc
    )

# `.` resolves the vendored `crypto/equihash.h` and `../util.h`; `..` resolves the headers it
# shares with the rest of nheqminer (`crypto/common.h`, `crypto/sha256.h`, `utilstrencodings.h`).
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(.)
include_directories(..)
include_directories(../nheqminer)

# Exposes `EhOptimisedSolve()`; the vendored solver compiles verification-only without it.
add_definitions(-DENABLE_MINING)

ADD_LIBRARY(${EXECUTABLE} STATIC ${SRC_LIST} ${HEADERS})
TARGET_LINK_LIBRARIES(${EXECUTABLE} ${SODIUM_LIBRARIES} sodium)

install( TARGETS ${EXECUTABLE} RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION lib )
install( FILES ${HEADERS} DESTINATION include/${EXECUTABLE} )

# A known-vector check: solves a fixed header and prints the solution, for cross-checking against
# an implementation that shares no code with this one (see README.md).
ADD_EXECUTABLE(cpu_generic_test test_vectors.cpp)
TARGET_LINK_LIBRARIES(cpu_generic_test ${EXECUTABLE} sodium)
ADD_TEST(NAME cpu_generic_48_5 COMMAND cpu_generic_test 48 5)
ADD_TEST(NAME cpu_generic_200_9 COMMAND cpu_generic_test 200 9)

# Negative tests: unsupported parameters, and GPU solvers with parameters they cannot mine, must
# both be refused before any worker starts rather than failing inside a mining thread.
ADD_TEST(NAME nheqminer_rejects_unsupported_params
         COMMAND $<TARGET_FILE:nheqminer> -b 1 -t 1 -n 47 -k 5)
SET_TESTS_PROPERTIES(nheqminer_rejects_unsupported_params PROPERTIES WILL_FAIL TRUE)
ADD_TEST(NAME nheqminer_rejects_gpu_with_non_default_params
         COMMAND $<TARGET_FILE:nheqminer> -b 1 -t 1 -n 48 -k 5 -cd 0)
SET_TESTS_PROPERTIES(nheqminer_rejects_gpu_with_non_default_params PROPERTIES WILL_FAIL TRUE)
