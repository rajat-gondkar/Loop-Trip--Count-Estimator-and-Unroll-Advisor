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


def run_command(args: list[str], env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    cmd_env = os.environ.copy()
    if env:
        cmd_env.update(env)
    return subprocess.run(args, text=True, capture_output=True, check=False, env=cmd_env)


def display_location(location: str) -> str:
    depth = ""
    if " depth=" in location:
        location, depth_value = location.split(" depth=", 1)
        depth = f" (depth {depth_value})"

    line = location.rsplit(":", 1)[-1]
    if line.isdigit():
        return f"Line {line}{depth}"
    return f"{location}{depth}"


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
        
        # Extract line number for sorting
        line_num = None
        if "Line " in location:
            try:
                line_part = location.split("Line ")[1].split()[0]
                line_num = int(line_part)
            except (IndexError, ValueError):
                pass
        
        loops.append(
            {
                "location": display_location(location),
                "tripCount": None if trip_count == "-" else int(trip_count),
                "classification": classification,
                "recommendation": recommendation,
                "rationale": rationale,
                "_lineNum": line_num,  # Internal sorting key
            }
        )
    
    # Sort by line number (None values go to the end)
    loops.sort(key=lambda x: (x["_lineNum"] is None, x["_lineNum"] or 0))
    
    # Remove the internal sorting key before returning
    for loop in loops:
        del loop["_lineNum"]
    
    return loops


@app.get("/")
def index():
    return render_template("index.html")


@app.post("/analyze")
def analyze():
    payload = request.get_json(silent=True) or {}
    code = payload.get("code", "")
    strategy = payload.get("strategy", "balanced")
    
    if not isinstance(code, str) or not code.strip():
        return jsonify({"error": "Request must include non-empty C source in 'code'."}), 400

    # Define threshold presets
    thresholds = {
        "conservative": {"small": 4, "medium": 16},
        "balanced": {"small": 8, "medium": 32},
        "aggressive": {"small": 16, "medium": 64},
    }
    
    if strategy not in thresholds:
        strategy = "balanced"
    
    small_threshold = thresholds[strategy]["small"]
    medium_threshold = thresholds[strategy]["medium"]

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

        # Set environment variables for thresholds
        env = {
            "LOOP_UNROLL_SMALL_THRESHOLD": str(small_threshold),
            "LOOP_UNROLL_MEDIUM_THRESHOLD": str(medium_threshold)
        }

        pass_result = run_command(
            [
                tool("opt"),
                "-disable-output",
                "-load-pass-plugin",
                str(PLUGIN),
                "-passes=function(mem2reg,loop-simplify,lcssa,indvars),loop-unroll-advisor",
                str(ir),
            ],
            env=env
        )
        if pass_result.returncode != 0:
            return jsonify({"error": pass_result.stderr.strip() or "LLVM pass failed."}), 500

        return jsonify({
            "loops": parse_pass_output(pass_result.stdout),
            "strategy": strategy,
            "thresholds": {"small": small_threshold, "medium": medium_threshold}
        })


if __name__ == "__main__":
    app.run(
        debug=os.environ.get("FLASK_DEBUG", "0") == "1",
        port=int(os.environ.get("PORT", "5001")),
        use_reloader=False,
    )
