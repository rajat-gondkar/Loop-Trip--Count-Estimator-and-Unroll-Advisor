# Design

## Problem Statement

Loop unrolling reduces branch overhead and exposes ILP, but is only beneficial when the trip count is small and statically known. This pass provides the *analysis side* — estimating trip count and emitting a justified recommendation — without performing the transformation itself.

## Approach

- **Pass type**: out-of-tree LLVM pass (New Pass Manager), runs as a `FunctionPass`
- **Pipeline**: precede the pass with `mem2reg, loop-simplify, lcssa, indvars` so ScalarEvolution has the best chance of resolving bounds
- **Trip-count estimation**: call `SE.getBackedgeTakenCount(L)`; if it returns a `SCEVConstant`, the count is exact and static
- **Classification**:
  - `EXACT_STATIC` — SCEVConstant result
  - `SYMBOLIC` — symbolic SCEV expression (e.g. depends on function parameter)
  - `RUNTIME` — SCEV can't compute it but a runtime check would help
  - `UNKNOWN` — SCEV returns CouldNotCompute with no useful info (e.g. pointer-walk loops)
- **Recommendation heuristic**:

| Trip count | Recommendation |
|---|---|
| ≤ 4 | `unroll fully` |
| 5 – 32 | `unroll x4` |
| > 32 | `do not unroll` |
| SYMBOLIC / RUNTIME | `runtime_check_then_unroll` |
| UNKNOWN | `do not unroll` |

## Alternatives Considered

- **In-tree pass**: faster at runtime but requires rebuilding all of LLVM; out-of-tree plugin is sufficient and much faster to develop.
- **LoopInfo only (no SCEV)**: LoopInfo can identify loops but cannot evaluate bound expressions. SCEV is necessary.
- **Profile-guided (PGO)**: gives exact dynamic counts but requires an instrumented run. We chose pure static analysis for zero runtime overhead.
- **LLVM's built-in `LoopUnrollPass`**: performs the actual transformation. We intentionally produce advice only (no IR mutation).

## Limitations

- Pointer-walk loops (e.g. linked-list traversal) are always `UNKNOWN`.
- Loops with multiple exit conditions may be misclassified as `SYMBOLIC`.
- Thresholds (4 / 32) are fixed heuristics, not auto-tuned.
