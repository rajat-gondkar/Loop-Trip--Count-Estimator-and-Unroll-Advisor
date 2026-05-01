#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LLVM_BIN="${LLVM_BIN:-}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
PLUGIN="$BUILD_DIR/LoopUnrollAdvisor.so"

if [[ -z "$LLVM_BIN" ]]; then
  if command -v llvm-config >/dev/null 2>&1; then
    LLVM_BIN="$(dirname "$(command -v llvm-config)")"
  elif [[ -x /opt/homebrew/opt/llvm@17/bin/llvm-config ]]; then
    LLVM_BIN="/opt/homebrew/opt/llvm@17/bin"
  elif [[ -x /opt/homebrew/opt/llvm/bin/llvm-config ]]; then
    LLVM_BIN="/opt/homebrew/opt/llvm/bin"
  else
    echo "error: set LLVM_BIN to a directory containing clang, opt, and llvm-config" >&2
    exit 1
  fi
fi

CLANG="$LLVM_BIN/clang"
OPT="$LLVM_BIN/opt"

if [[ ! -x "$CLANG" || ! -x "$OPT" ]]; then
  echo "error: LLVM_BIN must contain executable clang and opt: $LLVM_BIN" >&2
  exit 1
fi

if [[ ! -f "$PLUGIN" ]]; then
  echo "error: pass plugin not found at $PLUGIN; build with cmake first" >&2
  exit 1
fi

mkdir -p "$ROOT_DIR/tests/ir" "$ROOT_DIR/tests/results"

run_case() {
  local name="$1"
  local source="$ROOT_DIR/tests/c/$name.c"
  local ir="$ROOT_DIR/tests/ir/$name.ll"
  local result="$ROOT_DIR/tests/results/$name.txt"

  "$CLANG" -O0 -g -Xclang -disable-O0-optnone -S -emit-llvm "$source" -o "$ir"
  "$OPT" -disable-output -load-pass-plugin "$PLUGIN" \
    -passes="function(mem2reg,instcombine,simplifycfg,loop-simplify,lcssa,indvars),loop-unroll-advisor" \
    "$ir" >"$result"
  echo "wrote $result"
}

run_case fixed
run_case dynamic
run_case nested
run_case large
run_case mixed

grep -q "EXACT_STATIC | unroll x4" "$ROOT_DIR/tests/results/fixed.txt"
grep -q "DYNAMIC | do not unroll" "$ROOT_DIR/tests/results/dynamic.txt"
grep -q "NESTED | do not unroll" "$ROOT_DIR/tests/results/nested.txt"
grep -q "1024 | EXACT_STATIC | do not unroll" "$ROOT_DIR/tests/results/large.txt"
grep -q "unroll x4" "$ROOT_DIR/tests/results/mixed.txt"
grep -q "DYNAMIC | do not unroll" "$ROOT_DIR/tests/results/mixed.txt"

echo "all loop advisor tests passed"
