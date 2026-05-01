# Loop Unroll Advisor Project Report

## Report Update Rule

Each phase entry records the phase goal, files changed or added, commands run, observed results, problems encountered and fixes, output samples where available, and final status.

## Phase 0: Toolchain Verification And Report Setup

**Goal:** Establish the project baseline, verify available tooling, and create the report file.

**Files changed or added:**
- `PROJECT_REPORT.md`
- `.gitignore`

**Commands run:**
- `clang --version`
- `opt --version`
- `llvm-config --version`
- `cmake --version`
- `brew install llvm@17`
- `/opt/homebrew/opt/llvm@17/bin/clang --version`
- `/opt/homebrew/opt/llvm@17/bin/opt --version`
- `/opt/homebrew/opt/llvm@17/bin/llvm-config --version`

**Results observed:**
- Apple clang was available, but full LLVM tools were initially missing from `PATH`.
- Installed Homebrew LLVM 17.0.6.
- Verified LLVM tools:
  - `clang`: Homebrew clang version 17.0.6
  - `opt`: Homebrew LLVM version 17.0.6
  - `llvm-config`: 17.0.6
- CMake is available at `/opt/homebrew/bin/cmake`.

**Problems encountered and fixes:**
- `opt` and `llvm-config` were missing before install. Fixed by installing `llvm@17` and using `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin`.

**Status:** Completed.

## Phase 1: LLVM Pass Scaffold

**Goal:** Create a compiling out-of-tree LLVM plugin skeleton with new pass manager registration.

**Files changed or added:**
- `CMakeLists.txt`
- `src/LoopUnrollAdvisor.cpp`
- `tests/c/fixed.c`

**Commands run:**
- `cmake -S . -B build -DLLVM_DIR=/opt/homebrew/opt/llvm@17/lib/cmake/llvm`
- `cmake --build build`

**Results observed:**
- The pass is registered under the pipeline name `loop-unroll-advisor`.
- The plugin builds successfully as `build/LoopUnrollAdvisor.so`.

**Problems encountered and fixes:**
- CMake initially failed because `project(... LANGUAGES CXX)` did not enable C, but LLVM's CMake config performs C checks. Fixed by enabling `LANGUAGES C CXX`.
- macOS initially produced `LoopUnrollAdvisor.dylib`. Fixed by setting the plugin suffix to `.so` to match the assignment command.

**Status:** Completed.

## Phase 2: Loop Discovery And Metadata Extraction

**Goal:** Enumerate top-level and nested loops and extract useful metadata.

**Files changed or added:**
- `src/LoopUnrollAdvisor.cpp`
- `tests/c/nested.c`

**Commands run:**
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin ./scripts/run_tests.sh`

**Results observed:**
- The pass recursively traverses loops from `LoopInfo`.
- It records loop source location from debug info and falls back to `function:header` if needed.
- It emits debug information to `stderr` with function, location, depth, induction variable, and SCEV expression.
- Nested test reports both outer and inner loops.

**Representative debug output:**

```text
LoopUnrollAdvisor debug | function=nested | location=tests/c/nested.c:2 | depth=0 | inductionVariable=indvars.iv3 | scev={0,+,1}<nuw><nsw><%2>
LoopUnrollAdvisor debug | function=nested | location=tests/c/nested.c:3 depth=1 | depth=1 | inductionVariable=indvars.iv | scev={0,+,1}<nuw><nsw><%3>
```

**Problems encountered and fixes:**
- `Loop::getInductionVariable(SE)` did not identify every loop IV after canonicalization. Fixed by falling back to header PHI nodes whose SCEV is an add-recurrence for the loop.

**Status:** Completed.

## Phase 3: Trip Count Estimation

**Goal:** Classify loops using ScalarEvolution trip-count APIs.

**Files changed or added:**
- `src/LoopUnrollAdvisor.cpp`
- `tests/c/dynamic.c`
- `tests/c/large.c`
- `tests/c/mixed.c`

**Commands run:**
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin ./scripts/run_tests.sh`

**Results observed:**
- Exact static trip counts use ScalarEvolution's constant backedge-taken count.
- Bounded static trip counts use `SE.getSmallConstantMaxTripCount` only when the bound is small enough to be useful.
- Unknown or runtime-dependent counts are classified as `DYNAMIC`.
- Loops with subloops receive an additional `NESTED` classification tag.

**Problems encountered and fixes:**
- Plain `-O0` IR was too uncanonical for ScalarEvolution and reported all loops as dynamic.
- Plain `-O1` made ScalarEvolution work but optimized away simple store loops.
- Fixed by compiling with `-O0 -Xclang -disable-O0-optnone`, then running `function(mem2reg,instcombine,simplifycfg,loop-simplify,lcssa,indvars)` before the advisor pass.
- Very large type-range bounds such as `2147483648` for `i < n` were treated as dynamic because they are not useful static trip counts for this assignment.

**Status:** Completed.

## Phase 4: Recommendation Engine And Output Contract

**Goal:** Produce a recommendation and rationale for every discovered loop.

**Files changed or added:**
- `src/LoopUnrollAdvisor.cpp`

**Commands run:**
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin ./scripts/run_tests.sh`

**Results observed:**
- The pass prints a pipe-delimited table:

```text
LOOP_LOCATION | TRIP_COUNT | CLASSIFICATION | RECOMMENDATION | RATIONALE
```

**Representative output:**

```text
tests/c/fixed.c:2 | 16 | EXACT_STATIC | unroll x4 | Moderate trip count (16), x4 unroll balances code size and ILP
tests/c/dynamic.c:2 | - | DYNAMIC | do not unroll | Trip count not statically determinable
tests/c/large.c:2 | 1024 | EXACT_STATIC | do not unroll | Large trip count (1024), code size increase not justified
```

**Recommendation behavior:**
- Dynamic trip count: `do not unroll`.
- Static count `<= 8`: `unroll fully`.
- Static count `<= 32`: `unroll x4`.
- Static count `> 32`: `do not unroll`.
- Nested outer loop: `do not unroll`.

**Status:** Completed.

## Phase 5: Test Suite And Automation

**Goal:** Add repeatable tests for the pass behavior.

**Files changed or added:**
- `scripts/run_tests.sh`
- `tests/c/fixed.c`
- `tests/c/dynamic.c`
- `tests/c/nested.c`
- `tests/c/large.c`
- `tests/c/mixed.c`

**Commands run:**
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin ./scripts/run_tests.sh`

**Results observed:**
- The test script compiles each C file to LLVM IR, runs the plugin, writes result files, and checks expected recommendations with `grep`.
- Final result: `all loop advisor tests passed`.

**Generated result files:**
- `tests/results/fixed.txt`
- `tests/results/dynamic.txt`
- `tests/results/nested.txt`
- `tests/results/large.txt`
- `tests/results/mixed.txt`

**Status:** Completed.

## Phase 6: Backend Service

**Goal:** Build a local JSON API that compiles submitted C code, runs the pass, and returns loop results.

**Files changed or added:**
- `app.py`
- `requirements.txt`

**Commands run:**
- `python3 -m venv .venv`
- `.venv/bin/pip install -r requirements.txt`
- `.venv/bin/python -m py_compile app.py`
- Flask test client request against `POST /analyze`

**Results observed:**
- `POST /analyze` accepts `{ "code": "<c source>" }`.
- Successful responses return `{ "loops": [...] }`.
- Compilation and pass failures return `{ "error": "..." }`.
- The backend uses `LLVM_BIN`, `BUILD_DIR`, and `LOOP_ADVISOR_PLUGIN` environment variables when provided.

**Sample success response:**

```json
{
  "loops": [
    {
      "classification": "DYNAMIC",
      "location": "/tmp/.../input.c:1",
      "rationale": "Trip count not statically determinable",
      "recommendation": "do not unroll",
      "tripCount": null
    },
    {
      "classification": "EXACT_STATIC",
      "location": "/tmp/.../input.c:1",
      "rationale": "Moderate trip count (16), x4 unroll balances code size and ILP",
      "recommendation": "unroll x4",
      "tripCount": 16
    }
  ]
}
```

**Problems encountered and fixes:**
- Flask was not installed in the active Python. Fixed by creating `.venv` and installing `requirements.txt`.

**Status:** Completed.

## Phase 7: Web Frontend

**Goal:** Create a simple browser UI for loop analysis.

**Files changed or added:**
- `templates/index.html`
- `static/styles.css`
- `static/app.js`

**Commands run:**
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin .venv/bin/python app.py`
- `curl -s http://127.0.0.1:5000/`
- `curl -s -X POST http://127.0.0.1:5000/analyze ...`

**Results observed:**
- The UI includes a C source editor, analyze button, loading/error states, and a results table.
- Recommendations are color coded with badges.
- Unknown trip counts display `-` with a tooltip.
- Flask served the page at `http://127.0.0.1:5000/`.
- The running `/analyze` endpoint returned JSON loop recommendations through HTTP.

**Status:** Completed.

## Phase 8: Final Polish And Report Completion

**Goal:** Make the project easy to build, run, test, and submit.

**Files changed or added:**
- `README.md`
- `.gitignore`

**Commands run:**
- `cmake -S . -B build -DLLVM_DIR=/opt/homebrew/opt/llvm@17/lib/cmake/llvm`
- `cmake --build build`
- `LLVM_BIN=/opt/homebrew/opt/llvm@17/bin ./scripts/run_tests.sh`
- `.venv/bin/python -m py_compile app.py`

**Results observed:**
- README documents toolchain setup, build commands, tests, manual pass execution, web UI usage, and API shape.
- The pass builds successfully.
- The test suite passes.
- Backend request handling works through Flask's test client.

**Status:** Completed.

## Final Results

| Scenario | Observed classification | Recommendation |
|---|---|---|
| Fixed `i < 16` loop | `EXACT_STATIC`, count `16` | `unroll x4` |
| Dynamic `i < n` loop | `DYNAMIC`, count unknown | `do not unroll` |
| Nested outer `i < 8` loop | `EXACT_STATIC,NESTED`, count `8` | `do not unroll` |
| Nested inner `j < 8` loop | `EXACT_STATIC`, count `8` | `unroll fully` |
| Large `i < 1024` loop | `EXACT_STATIC`, count `1024` | `do not unroll` |

## Known Limitations

- ScalarEvolution results depend on the IR shape, so the project intentionally runs a small canonicalization pipeline before the advisor.
- Very large maximum trip counts inferred from integer type ranges are treated as dynamic because they are not useful static trip counts for the assignment's recommendation model.
- The UI is intended as a local development tool and does not implement authentication or sandboxing for untrusted code.
