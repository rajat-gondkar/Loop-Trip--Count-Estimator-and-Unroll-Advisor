cmake_minimum_required(VERSION 3.20)
project(LoopUnrollAdvisor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

add_llvm_pass_plugin(LoopUnrollAdvisor
  src/LoopUnrollAdvisor.cpp
)

set_target_properties(LoopUnrollAdvisor PROPERTIES
  PREFIX ""
  SUFFIX ".so"
)
