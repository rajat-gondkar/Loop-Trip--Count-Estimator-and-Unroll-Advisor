# Loop Unroll Advisor

Loop Unroll Advisor is an LLVM pass that estimates loop trip counts with LoopInfo and ScalarEvolution, classifies each loop, and recommends whether to `unroll fully`, `unroll x4`, or `do not unroll`. It can be used from the terminal or through the included Flask frontend.

## Prerequisites

This project is intended for macOS or Linux. The provided scripts use Bash and Unix-style paths. On Windows, use WSL/Linux unless you also add Windows-specific build and run scripts.

- LLVM 17 or newer
- CMake 3.20 or newer
- Python 3.10 or newer
- Flask, installed from `requirements.txt`

The scripts try to auto-detect LLVM in common locations such as Homebrew's `llvm@17`. If auto-detection fails, set `LLVM_BIN` to the folder that contains your LLVM tools (`clang`, `opt`, and `llvm-config`).

```bash
export LLVM_BIN=/path/to/llvm/bin
```

Example on macOS with Homebrew:

```bash
export LLVM_BIN=/opt/homebrew/opt/llvm@17/bin
```

## Build

Run this once after setting `LLVM_BIN`:

```bash
./scripts/build.sh
```

## Use The Frontend

Start the Flask frontend:

```bash
./scripts/run.sh
```

Then open:

```text
http://127.0.0.1:5000
```

If port `5000` is already in use, run:

```bash
PORT=5001 ./scripts/run.sh
```

Then open:

```text
http://127.0.0.1:5001
```

The frontend lets you paste C code, analyze loops, and use the built-in test button to load and run sample test cases without typing terminal commands.


## Run From Terminal

```bash
LLVM_BIN="$LLVM_BIN" ./scripts/run_tests.sh
```

The terminal may show debug lines from the LLVM pass while tests run. The readable output tables are saved as text files under:

```text
tests/results/
```

For the most complete sample, open:

```bash
cat tests/results/mixed.txt
```

## Output Format

```text
LOOP_LOCATION | TRIP_COUNT | CLASSIFICATION | RECOMMENDATION | RATIONALE
```

## Repo Structure

```text
.
├── src/
├── scripts/
├── tests/
│   └── c/
├── DESIGN.md
├── IMPLEMENTATION.md
└── EVALUATION.md
```
