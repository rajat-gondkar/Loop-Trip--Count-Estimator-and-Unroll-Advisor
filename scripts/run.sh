#!/usr/bin/env bash
set -euo pipefail

LLVM_BIN="${LLVM_BIN:?Please set LLVM_BIN to your LLVM 17 bin directory}"
[ -d .venv ] || python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt
LLVM_BIN="$LLVM_BIN" python app.py
