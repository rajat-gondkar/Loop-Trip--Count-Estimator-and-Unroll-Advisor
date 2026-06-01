#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${LLVM_BIN:-}" ]]; then
  if [[ -x /opt/homebrew/opt/llvm@17/bin/llvm-config ]]; then
    LLVM_BIN="/opt/homebrew/opt/llvm@17/bin"
  elif [[ -x /usr/local/opt/llvm@17/bin/llvm-config ]]; then
    LLVM_BIN="/usr/local/opt/llvm@17/bin"
  elif command -v llvm-config >/dev/null 2>&1; then
    LLVM_BIN="$(dirname "$(command -v llvm-config)")"
  else
    echo "error: set LLVM_BIN to your LLVM 17 bin directory" >&2
    exit 1
  fi
fi

[ -d .venv ] || python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt
LLVM_BIN="$LLVM_BIN" python app.py
