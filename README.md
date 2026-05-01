# Loop Unroll Advisor

An out-of-tree LLVM pass that estimates loop trip counts with `LoopInfo` and `ScalarEvolution`, recommends whether to unroll each loop, and displays results in a small local web UI.

## Requirements

- LLVM 17+ with `clang`, `opt`, and `llvm-config`
- CMake 3.20+
- Python 3.10+
- Flask

On macOS with Homebrew, install LLVM if needed:

```bash
brew install llvm@17
```

If LLVM is not on `PATH`, export:

```bash
export LLVM_BIN=/opt/homebrew/opt/llvm@17/bin
export PATH="$LLVM_BIN:$PATH"
```

## Build The Pass

```bash
cmake -S . -B build -DLLVM_DIR="$LLVM_BIN/../lib/cmake/llvm"
cmake --build build
```

The plugin is expected at:

```text
build/LoopUnrollAdvisor.so
```

## Run Tests

```bash
LLVM_BIN="$LLVM_BIN" ./scripts/run_tests.sh
```

The script compiles files in `tests/c`, writes LLVM IR to `tests/ir`, writes pass output to `tests/results`, and checks the expected recommendations.

## Run The Pass Manually

```bash
"$LLVM_BIN/clang" -O0 -g -Xclang -disable-O0-optnone -S -emit-llvm tests/c/mixed.c -o /tmp/mixed.ll
"$LLVM_BIN/opt" -disable-output -load-pass-plugin build/LoopUnrollAdvisor.so \
  -passes="function(mem2reg,instcombine,simplifycfg,loop-simplify,lcssa,indvars),loop-unroll-advisor" \
  /tmp/mixed.ll
```

Output format:

```text
LOOP_LOCATION | TRIP_COUNT | CLASSIFICATION | RECOMMENDATION | RATIONALE
```

## Run The Web UI

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
LLVM_BIN="$LLVM_BIN" python app.py
```

Open:

```text
http://127.0.0.1:5000
```

## API

`POST /analyze`

Request:

```json
{ "code": "void f(int *a) { for (int i = 0; i < 16; i++) a[i] = i; }" }
```

Success response:

```json
{
  "loops": [
    {
      "location": "input.c:1",
      "tripCount": 16,
      "classification": "EXACT_STATIC",
      "recommendation": "unroll x4",
      "rationale": "Moderate trip count (16), x4 unroll balances code size and ILP"
    }
  ]
}
```

Error response:

```json
{ "error": "Compilation failed." }
```
