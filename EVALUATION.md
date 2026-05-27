# Evaluation

## Test Cases

Run the pass on the required files in `tests/c/` and record actual output.

| File | Loop Description | Classification | Recommendation |
|------|-----------------|----------------|----------------|
| `simple_for.c` | `for(i=0;i<16;i++)` | `EXACT_STATIC` | `unroll x4` |
| `large_trip.c` | `for(i=0;i<1024;i++)` | `EXACT_STATIC` | `do not unroll` |
| `unknown_bound.c` | `for(i=0;i<n;i++)`, n is param | `DYNAMIC` | `do not unroll` |
| `nested_loop.c` | 2-level nested, inner bound=4 | outer: `EXACT_STATIC,NESTED`; inner: `EXACT_STATIC` | outer: `do not unroll`; inner: `unroll fully` |
| `while_loop.c` | pointer-walk `while(p!=NULL)` | `DYNAMIC` | `do not unroll` |
| `mixed.c` | constant + dynamic + nested + large + dead loops | `EXACT_STATIC`; `DYNAMIC`; `EXACT_STATIC,NESTED`; `EXACT_STATIC`; `EXACT_STATIC`; `DEAD_LOOP` | `unroll x4`; `do not unroll`; `do not unroll`; `unroll fully`; `do not unroll`; `dead loop` |
| `symbolic.c` | `for(i=start;i<end;i+=step)` | `DYNAMIC` | `do not unroll` |

Command used:

```bash
export LLVM_BIN=/opt/homebrew/opt/llvm@17/bin
./scripts/build.sh
for f in tests/c/simple_for.c tests/c/large_trip.c tests/c/unknown_bound.c tests/c/nested_loop.c tests/c/while_loop.c tests/c/mixed.c tests/c/symbolic.c; do
  echo "=== $f ==="
  "$LLVM_BIN/clang" -O0 -g -Xclang -disable-O0-optnone -S -emit-llvm "$f" -o /tmp/test.ll
  "$LLVM_BIN/opt" -disable-output \
    -load-pass-plugin build/LoopUnrollAdvisor.so \
    -passes="function(mem2reg,loop-simplify,lcssa,indvars),loop-unroll-advisor" \
    /tmp/test.ll 2>&1
done
```

Representative pass output:

```text
tests/c/simple_for.c:2 | 16 | EXACT_STATIC | unroll x4 | Moderate trip count (16), x4 exposes more instruction-level parallelism without excessive code size
tests/c/large_trip.c:2 | 1024 | EXACT_STATIC | do not unroll | Large trip count (1024), aggressive unrolling would likely increase code size more than it helps
tests/c/unknown_bound.c:2 | - | DYNAMIC | do not unroll | Loop bound depends on runtime data, so a static unroll choice is not reliable
tests/c/while_loop.c:4 | - | DYNAMIC | do not unroll | Loop bound depends on runtime data, so a static unroll choice is not reliable
tests/c/symbolic.c:2 | - | DYNAMIC | do not unroll | Loop bound depends on runtime data, so a static unroll choice is not reliable
```

## Baseline Comparison

Compare our advisor's recommendations against what `clang -O2` actually unrolls using `-Rpass=loop-unroll`.

Command used:

```bash
for f in tests/c/simple_for.c tests/c/large_trip.c tests/c/unknown_bound.c tests/c/nested_loop.c tests/c/while_loop.c tests/c/mixed.c tests/c/symbolic.c; do
  echo "=== $f ==="
  "$LLVM_BIN/clang" -O2 -Rpass=loop-unroll "$f" -o /dev/null 2>&1 | grep "unrolled"
done
```

| File | LLVM -O2 Unrolled? | Our Recommendation | Agreement |
|------|-------------------|-------------------|-----------|
| `simple_for.c` | Yes, completely unrolled 16 iterations | `unroll x4` | Partial |
| `large_trip.c` | No remark observed | `do not unroll` | Yes |
| `unknown_bound.c` | No remark observed | `do not unroll` | Yes |
| `nested_loop.c` | Yes, both 4-iteration loops completely unrolled | outer: `do not unroll`; inner: `unroll fully` | Partial |
| `while_loop.c` | No remark observed | `do not unroll` | Yes |
| `mixed.c` | Yes, fixed and nested loops unrolled | mixed recommendations by loop | Partial |
| `symbolic.c` | No remark observed | `do not unroll` | Yes |

## Cases Where Static Trip Count Cannot Be Determined

- **Pointer-walk loops** (`while(p != NULL)`): exit depends on heap structure, unknowable statically.
- **Data-dependent loops** (`while(a[i] != sentinel)`): exit condition tied to runtime values.
- **Loops with multiple exits**: SCEV conservatively returns CouldNotCompute.
- In all such cases the pass outputs `UNKNOWN` / `do not unroll` as a safe default.
