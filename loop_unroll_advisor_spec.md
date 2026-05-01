# Assignment 4 — Loop Trip-Count Estimator and Unroll Advisor
## Iterative Implementation Plan for Coding Agent

---

## Project Overview

Build an LLVM pass (LoopPass or FunctionPass) that:
1. Analyzes each loop in a program using `ScalarEvolution` and `LoopInfo` APIs
2. Estimates the **static trip count** of the loop
3. Outputs a recommendation: `"unroll fully"`, `"unroll x4"`, or `"do not unroll"` with a one-line rationale
4. Presents results in a **simple web frontend** (output table UI)

---

## Phase 1 — Project Scaffold & Environment Setup

**Goal:** Get a compiling LLVM pass skeleton running end-to-end.

### Tasks
- [ ] Set up LLVM development environment (LLVM 17+ recommended, or match lab version)
- [ ] Create a new out-of-tree LLVM pass using `CMakeLists.txt` with `find_package(LLVM REQUIRED CONFIG)`
- [ ] Implement a minimal `FunctionPass` (or `LoopPass`) that registers itself and runs without crashing
- [ ] Verify the pass loads via `opt -load-pass-plugin ./LoopUnrollAdvisor.so -passes="loop-unroll-advisor" input.ll`
- [ ] Add `LoopAnalysis` and `ScalarEvolutionAnalysis` to the pass's `getAnalysisUsage` (legacy PM) or `getRequiredAnalyses` (new PM)
- [ ] Write a trivial test `.c` file with a simple counted loop and compile to `.ll` using `clang -S -emit-llvm`

### Deliverable Checkpoint
- Pass loads and prints "Hello from LoopUnrollAdvisor" for each function/loop without crashing.

---

## Phase 2 — Loop Enumeration & Induction Variable Extraction

**Goal:** Traverse all loops and extract induction variable metadata.

### Tasks
- [ ] Use `LoopInfo` to iterate over all top-level loops in a function
- [ ] Use `Loop::getSubLoops()` to recursively enumerate nested loops
- [ ] For each loop, use `ScalarEvolution::getInductionVariable(L)` or inspect the loop's canonical induction variable via `Loop::getInductionVariable(SE)`
- [ ] Extract:
  - Loop header basic block name / debug location (file + line number if available via `DILocation`)
  - Induction variable name and type
  - Start value (via `SE.getSCEV(inductionVar)`)
  - Step value
  - Exit/bound value from the loop's exit condition
- [ ] Print extracted metadata to `stderr` in a structured format for debugging

### Key APIs to Use
```cpp
LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
Loop::getInductionVariable(SE);          // canonical IV
SE.getSmallConstantTripCount(L);         // trip count if statically known
SE.getSmallConstantMaxTripCount(L);      // upper bound trip count
SE.getSCEV(V);                           // SCEV expression for a value
```

### Deliverable Checkpoint
- For each loop in a test program, the pass prints the loop location and whether a canonical induction variable was found.

---

## Phase 3 — Trip Count Estimation Logic

**Goal:** Implement the core analysis: estimate trip counts and classify loops.

### Tasks

#### 3a — Static Trip Count (Known at Compile Time)
- [ ] Call `SE.getSmallConstantTripCount(L)` — returns 0 if unknown, else the exact count
- [ ] If nonzero, record as `EXACT_STATIC` with the value

#### 3b — Bounded Trip Count (Upper Bound Known)
- [ ] Call `SE.getSmallConstantMaxTripCount(L)` for a conservative upper bound
- [ ] If different from exact count, record as `BOUNDED` with the max value

#### 3c — Unknown / Dynamic Trip Count
- [ ] If both above return 0, classify as `DYNAMIC` — trip count depends on runtime values
- [ ] Optionally: inspect the loop's exit condition's SCEV expression and report its symbolic form (e.g., `{0,+,1}<loop>` pattern)

#### 3d — Nested Loop Handling
- [ ] For inner loops: report their trip count independently
- [ ] For outer loops containing inner loops: note nesting depth in the output
- [ ] Flag loops with `Loop::getSubLoops().size() > 0` as `NESTED`

### Trip Count Classification Table

| Condition | Classification |
|---|---|
| `getSmallConstantTripCount > 0` | `EXACT_STATIC` |
| `getSmallConstantMaxTripCount > 0` but exact unknown | `BOUNDED_STATIC` |
| Both return 0 | `DYNAMIC` |
| Has sub-loops | Also tagged `NESTED` |

### Deliverable Checkpoint
- Pass correctly reports `EXACT_STATIC: 100` for `for(int i=0; i<100; i++)` and `DYNAMIC` for `for(int i=0; i<n; i++)`.

---

## Phase 4 — Unroll Recommendation Engine

**Goal:** Produce a recommendation with rationale for each loop.

### Recommendation Rules

Implement the following decision logic (adjust thresholds as needed):

```
if classification == DYNAMIC:
    recommendation = "do not unroll"
    rationale = "Trip count not statically determinable"

else if tripCount <= 8:
    recommendation = "unroll fully"
    rationale = "Small static trip count ({tripCount}), full unroll eliminates loop overhead"

else if tripCount <= 32:
    recommendation = "unroll x4"
    rationale = "Moderate trip count ({tripCount}), x4 unroll balances code size and ILP"

else if tripCount > 32 and not NESTED:
    recommendation = "do not unroll"
    rationale = "Large trip count ({tripCount}), code size increase not justified"

if NESTED:
    recommendation = "do not unroll"
    rationale = "Nested loop — unrolling outer loop may cause instruction cache pressure"
```

### Tasks
- [ ] Implement the above decision tree as a `getRecommendation(Loop*, ScalarEvolution&)` function
- [ ] Return a struct: `{ location, tripCount, classification, recommendation, rationale }`
- [ ] Collect results for all loops in a function into a vector
- [ ] Print the final output table to `stdout` in pipe-delimited format:
  ```
  LOOP_LOCATION | TRIP_COUNT | RECOMMENDATION | RATIONALE
  ```

### Deliverable Checkpoint
- Running the pass on a multi-loop test file produces a complete output table with correct recommendations.

---

## Phase 5 — Test Programs

**Goal:** Validate the pass against diverse loop structures.

### Test Case 1 — Fixed Count Loop
```c
// test_fixed.c
void fixed() {
    int arr[16];
    for (int i = 0; i < 16; i++) arr[i] = i * 2;
}
```
Expected: `EXACT_STATIC: 16`, recommendation: `unroll x4`

### Test Case 2 — Variable / Dynamic Loop
```c
// test_dynamic.c
void dynamic(int n, int *arr) {
    for (int i = 0; i < n; i++) arr[i] = 0;
}
```
Expected: `DYNAMIC`, recommendation: `do not unroll`

### Test Case 3 — Nested Loop
```c
// test_nested.c
void nested(int A[8][8]) {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            A[i][j] = i + j;
}
```
Expected: outer `NESTED`, inner `EXACT_STATIC: 8`, outer recommendation: `do not unroll`

### Test Case 4 — Large Fixed Count
```c
// test_large.c
void large(int *arr) {
    for (int i = 0; i < 1024; i++) arr[i] = 0;
}
```
Expected: `EXACT_STATIC: 1024`, recommendation: `do not unroll`

### Test Case 5 — Mixed Function
```c
// test_mixed.c — combine all of the above in one translation unit
```

### Tasks
- [ ] Write all test `.c` files
- [ ] Create a `Makefile` or shell script `run_tests.sh` that:
  - Compiles each to `.ll` with `clang -O0 -S -emit-llvm`
  - Runs the pass with `opt`
  - Captures stdout to `.txt` result files
- [ ] Verify output matches expected results for each test

---

## Phase 6 — Frontend (Web UI)

**Goal:** Build a simple, clean web interface to visualize the pass output.

### Architecture
- **Backend**: A minimal Python Flask (or Node.js Express) server that:
  - Accepts a `.c` file upload or inline C code via a text area
  - Invokes `clang` → `opt` pipeline server-side
  - Returns the pipe-delimited output table as JSON
- **Frontend**: A single-page HTML/CSS/JS app (no heavy framework needed) that:
  - Provides a code editor text area for C input
  - Has a "Analyze Loops" button
  - Displays the results in a styled table

### Frontend Requirements
- [ ] **Input Panel**: Text area pre-filled with a sample loop program, syntax-highlighted if possible (use CodeMirror or highlight.js)
- [ ] **Analyze Button**: Sends code to backend, shows a loading state
- [ ] **Results Table** with columns:
  - Loop Location (file:line)
  - Estimated Trip Count
  - Classification (`EXACT_STATIC` / `BOUNDED_STATIC` / `DYNAMIC` / `NESTED`)
  - Recommendation (color-coded badge: green = unroll fully, yellow = unroll x4, red = do not unroll)
  - Rationale
- [ ] **Edge case display**: If trip count is unknown, show `—` in the count column with a tooltip explanation
- [ ] Responsive layout, works on desktop browser
- [ ] No login or auth required — simple local tool

### Backend Requirements
- [ ] `POST /analyze` endpoint accepts `{ "code": "<c source>" }`
- [ ] Writes code to a temp file, runs `clang -S -emit-llvm -o /tmp/input.ll /tmp/input.c`
- [ ] Runs `opt -load-pass-plugin ./LoopUnrollAdvisor.so -passes="loop-unroll-advisor" /tmp/input.ll`
- [ ] Parses stdout into JSON array of loop result objects
- [ ] Returns `{ "loops": [ { location, tripCount, classification, recommendation, rationale }, ... ] }`
- [ ] Error handling: return `{ "error": "..." }` if compilation or pass fails

### Deliverable Checkpoint
- Paste a C function with 2–3 loops into the UI, click Analyze, and see the results table populate correctly.

---

## Phase 7 — Discussion & Report

**Goal:** Document the analysis, limitations, and observations.

### Required Discussion Points

#### 7a — When Trip Count Cannot Be Determined Statically
Discuss and give concrete examples for each case:
- **Runtime-dependent bounds**: `for(int i = 0; i < n; i++)` — `n` is a function parameter
- **Pointer arithmetic bounds**: `while(*ptr != '\0')` — depends on memory contents
- **Indirect control flow**: loops exiting via `break` inside conditionals
- **Floatng-point induction variables**: SCEV cannot reason about FP arithmetic
- **Loops with multiple exits**: `SE.getSmallConstantTripCount` returns 0 for multi-exit loops
- **Irreducible loops**: LoopInfo may not even identify them as loops

#### 7b — ScalarEvolution Limitations
- SCEV works on integer arithmetic only
- Cannot handle pointer-based or modular arithmetic trip counts
- Wrapping behavior (`nsw`/`nuw` flags) affects SCEV's analysis

#### 7c — Correctness vs. Profitability
- Explain why static analysis alone is insufficient for unroll decisions (cache effects, register pressure, vectorization interaction)
- Note that this pass provides the **analysis prerequisite**, not the full optimization

### Deliverable
A `REPORT.md` covering all of the above with code examples and pass output screenshots.

---

## File Structure

```
loop-unroll-advisor/
├── CMakeLists.txt
├── LoopUnrollAdvisor.cpp        # Main pass implementation
├── LoopUnrollAdvisor.h          # (optional) header
├── tests/
│   ├── test_fixed.c
│   ├── test_dynamic.c
│   ├── test_nested.c
│   ├── test_large.c
│   ├── test_mixed.c
│   └── run_tests.sh
├── frontend/
│   ├── server.py                # Flask backend (or server.js)
│   ├── requirements.txt         # (Flask, etc.)
│   ├── static/
│   │   ├── index.html
│   │   ├── style.css
│   │   └── app.js
│   └── README.md                # How to run the frontend
└── REPORT.md
```

---

## Phase Summary Table

| Phase | Goal | Key Output |
|---|---|---|
| 1 | Scaffold | Compiling pass skeleton |
| 2 | Loop enumeration | IV and loop location extraction |
| 3 | Trip count estimation | Classification: EXACT / BOUNDED / DYNAMIC |
| 4 | Recommendation engine | Per-loop recommendation + rationale |
| 5 | Test programs | Validated output on 5 test cases |
| 6 | Frontend | Web UI with code input and results table |
| 7 | Report | Discussion of limitations and design choices |

---

## Important Notes for Coding Agent

- Use the **New Pass Manager** (`PassInfoMixin`) if targeting LLVM 14+; fall back to legacy PM only if the lab environment requires it.
- Always call `SE.hasLoopInvariantBackedgeTakenCount(L)` before `getSmallConstantTripCount` to avoid unnecessary computation.
- Use `L->getStartLoc()` for loop location; fall back to header BB name if debug info is unavailable.
- The frontend backend server **must** sanitize and sandbox the code execution (use `subprocess` with a timeout, never `shell=True` with user input directly).
- All phases should be independently testable — do not couple Phase 6 to a specific output format until Phase 4 is stable.
