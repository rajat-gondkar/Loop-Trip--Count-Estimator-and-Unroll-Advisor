from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from flask import Flask, jsonify, render_template, request

ROOT = Path(__file__).resolve().parent
BUILD_DIR = Path(os.environ.get("BUILD_DIR", ROOT / "build"))
PLUGIN = Path(os.environ.get("LOOP_ADVISOR_PLUGIN", BUILD_DIR / "LoopUnrollAdvisor.so"))
LLVM_BIN = os.environ.get("LLVM_BIN", "")


def tool(name: str) -> str:
    if LLVM_BIN:
        return str(Path(LLVM_BIN) / name)
    return name


app = Flask(__name__)


def run_command(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, text=True, capture_output=True, check=False)


def parse_pass_output(output: str) -> list[dict[str, Any]]:
    loops: list[dict[str, Any]] = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("LOOP_LOCATION"):
            continue

        parts = [part.strip() for part in line.split("|", 4)]
        if len(parts) != 5:
            continue

        location, trip_count, classification, recommendation, rationale = parts
        loops.append(
            {
                "location": location,
                "tripCount": None if trip_count == "-" else int(trip_count),
                "classification": classification,
                "recommendation": recommendation,
                "rationale": rationale,
            }
        )
    return loops


@app.get("/")
def index():
    return render_template("index.html")


@app.post("/analyze")
def analyze():
    payload = request.get_json(silent=True) or {}
    code = payload.get("code", "")
    if not isinstance(code, str) or not code.strip():
        return jsonify({"error": "Request must include non-empty C source in 'code'."}), 400

    if not PLUGIN.exists():
        return jsonify({"error": f"LoopUnrollAdvisor plugin not found at {PLUGIN}."}), 500

    with tempfile.TemporaryDirectory(prefix="loop-advisor-") as tmp:
        tmp_path = Path(tmp)
        source = tmp_path / "input.c"
        ir = tmp_path / "input.ll"
        source.write_text(code, encoding="utf-8")

        compile_result = run_command(
            [
                tool("clang"),
                "-O0",
                "-g",
                "-Xclang",
                "-disable-O0-optnone",
                "-S",
                "-emit-llvm",
                str(source),
                "-o",
                str(ir),
            ]
        )
        if compile_result.returncode != 0:
            return jsonify({"error": compile_result.stderr.strip() or "Compilation failed."}), 400

        pass_result = run_command(
            [
                tool("opt"),
                "-disable-output",
                "-load-pass-plugin",
                str(PLUGIN),
                "-passes=function(mem2reg,instcombine,simplifycfg,loop-simplify,lcssa,indvars),loop-unroll-advisor",
                str(ir),
            ]
        )
        if pass_result.returncode != 0:
            return jsonify({"error": pass_result.stderr.strip() or "LLVM pass failed."}), 500

        return jsonify({"loops": parse_pass_output(pass_result.stdout)})


if __name__ == "__main__":
    app.run(debug=True, port=int(os.environ.get("PORT", "5000")))
