# Implementation

## LLVM APIs Used

| API | Purpose |
|---|---|
| `LoopInfo` | Enumerate all loops; query nesting depth |
| `ScalarEvolution` | `getBackedgeTakenCount(L)` — symbolic trip count |
| `SE.getSmallConstantTripCount(L)` | Fast path for small constant bounds |
| `SE.hasLoopInvariantBackedgeTakenCount(L)` | Gate for RUNTIME classification |
| `Loop::getStartLoc()` | Emit source file + line in output |
| `Loop::getLoopDepth()` | Include nesting level in rationale |

## Pass Pipeline

The pass must be preceded by canonicalisation passes:

```text
function(mem2reg, loop-simplify, lcssa, indvars)
```

- `mem2reg` — promotes alloca slots to SSA registers so SCEV can reason about values
- `loop-simplify` — inserts a pre-header, ensures a single back-edge
- `lcssa` — moves uses outside the loop into Loop-Closed SSA form
- `indvars` — widens and canonicalises induction variables

Manual invocation:

```bash
"$LLVM_BIN/clang" -O0 -g -Xclang -disable-O0-optnone -S -emit-llvm input.c -o input.ll
"$LLVM_BIN/opt" -disable-output \
  -load-pass-plugin build/LoopUnrollAdvisor.so \
  -passes="function(mem2reg,loop-simplify,lcssa,indvars),loop-unroll-advisor" \
  input.ll
```

## Output & Web UI

- The pass writes results to `stderr` via `llvm::errs()` in pipe-delimited format.
- `app.py` runs `clang` + `opt` as subprocesses, captures stderr, parses matching lines, and returns JSON to the browser.
- The Flask front-end (vanilla JS) renders one result card per loop.

## Build System

Key CMake lines:

```cmake
find_package(LLVM REQUIRED CONFIG)
add_library(LoopUnrollAdvisor MODULE src/LoopUnrollAdvisor.cpp)
target_link_libraries(LoopUnrollAdvisor PRIVATE LLVMCore LLVMAnalysis)
set_target_properties(LoopUnrollAdvisor PROPERTIES PREFIX "" SUFFIX ".so")
```
