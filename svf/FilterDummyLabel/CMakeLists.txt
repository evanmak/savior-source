cmake_minimum_required(VERSION 3.6)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

message(STATUS "LLVM definitions ${LLVM_DEFINITIONS}")
message(STATUS "LLVM Include Dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM LIBRARY Dir: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(filterpass)  # Use your pass name here.
