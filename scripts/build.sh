#!/usr/bin/env bash
set -euo pipefail

LLVM_BIN="${LLVM_BIN:?Please set LLVM_BIN to your LLVM 17 bin directory}"
cmake -S . -B build -DLLVM_DIR="$LLVM_BIN/../lib/cmake/llvm"
cmake --build build --parallel
