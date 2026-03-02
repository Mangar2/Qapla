#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


OPTIONS: list[tuple[str, int]] = [
    ("imbalanceOwnPP", 15),
    ("imbalanceOwnNP", 86),
    ("imbalanceOwnNN", -13),
    ("imbalanceOwnBP", 9),
    ("imbalanceOwnBN", 40),
    ("imbalanceOwnBB", 14),
    ("imbalanceOwnRP", 60),
    ("imbalanceOwnRN", 42),
    ("imbalanceOwnRB", 83),
    ("imbalanceOwnRR", 64),
    ("imbalanceOwnQP", 2),
    ("imbalanceOwnQN", -36),
    ("imbalanceOwnQB", -23),
    ("imbalanceOwnQR", -46),
    ("imbalanceOwnQQ", 80),
    ("imbalanceOppNP", 23),
    ("imbalanceOppBP", 51),
    ("imbalanceOppBN", 49),
    ("imbalanceOppRP", 30),
    ("imbalanceOppRN", -18),
    ("imbalanceOppRB", -47),
    ("imbalanceOppQP", 106),
    ("imbalanceOppQN", 27),
    ("imbalanceOppQB", -4),
    ("imbalanceOppQR", 21),
]

DEFAULT_TESTER_BIN: dict[str, str] = {
    "linux": "/home/mangar/bin/qet",
    "win": r"C:\Development\qapla-engine-tester\build\release\qapla-engine-tester.exe",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run imbalance SPRT sweep.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--platform",
        choices=("win", "linux"),
        required=True,
        help="Select settings template: imbalance-sprt-win.ini or imbalance-sprt-linux.ini",
    )
    parser.add_argument(
        "--tester-bin",
        default=None,
        help="Path to tester binary (overrides platform default)",
    )
    parser.add_argument(
        "--log-dir",
        default="test/log/imbalance-sprt",
        help="Log directory (default: test/log/imbalance-sprt)",
    )
    parser.add_argument(
        "--delta",
        type=int,
        default=16,
        help="Absolute value added/subtracted for each option test",
    )
    return parser.parse_args()


def load_engine_cmd(settings_file: Path) -> str:
    in_engine_section = False
    for raw_line in settings_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            in_engine_section = line[1:-1].strip().lower() == "engine"
            continue
        if in_engine_section and "=" in line:
            key, value = line.split("=", 1)
            if key.strip().lower() == "cmd":
                cmd = value.strip()
                if cmd:
                    return cmd

    raise RuntimeError(f"Missing [engine] cmd in settings file: {settings_file}")


def run_and_tee(command: list[str], run_log: Path) -> list[str]:
    output_lines: list[str] = []
    with run_log.open("w", encoding="utf-8", newline="") as log_handle:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        assert process.stdout is not None
        for line in process.stdout:
            output_lines.append(line)
            log_handle.write(line)
            sys.stdout.write(line)

        return_code = process.wait()
        if return_code != 0:
            raise subprocess.CalledProcessError(return_code, command)

    return output_lines


def get_summary_lines(output_lines: list[str]) -> tuple[str, str | None]:
    stripped = [line.rstrip("\r\n") for line in output_lines]
    sprt_candidates = [line for line in stripped if "SPRT final result:" in line]
    decision_candidates = [line for line in stripped if line.startswith("decision:")]

    sprt_line = sprt_candidates[-1] if sprt_candidates else ""
    decision_line = decision_candidates[-1] if decision_candidates else None

    if not sprt_line:
        non_empty = [line for line in stripped if line.strip()]
        sprt_line = non_empty[-1] if non_empty else "NO_RESULT_LINE_FOUND"

    return sprt_line, decision_line


def main() -> int:
    args = parse_args()
    if args.delta <= 0:
        raise ValueError("--delta must be > 0")
    tester_bin = args.tester_bin or DEFAULT_TESTER_BIN[args.platform]

    script_dir = Path(__file__).resolve().parent
    settings_template = script_dir / f"imbalance-sprt-{args.platform}.ini"
    if not settings_template.is_file():
        raise FileNotFoundError(f"Settings template not found: {settings_template}")

    engine_cmd = load_engine_cmd(settings_template)

    log_dir = Path(args.log_dir)
    collect_file = log_dir / "imbalance-sprt-summary.log"
    log_dir.mkdir(parents=True, exist_ok=True)
    collect_file.write_text("", encoding="utf-8")

    temp_settings_file: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix="imbalance-sprt-", suffix=".ini", delete=False) as temp_handle:
            temp_settings_file = Path(temp_handle.name)
        shutil.copyfile(settings_template, temp_settings_file)

        run_count = 0
        total_runs = len(OPTIONS) * 2

        for parameter_name, base_value in OPTIONS:
            for delta in (-args.delta, args.delta):
                test_value = base_value + delta
                run_count += 1

                delta_label = f"plus{args.delta}" if delta > 0 else f"minus{args.delta}"
                engine_name = f"Qaplaoptspsa_{parameter_name}_{delta_label}"
                run_log = log_dir / f"{run_count:02d}-{parameter_name}-{test_value}.log"

                print(f"[{run_count}/{total_runs}] Running {parameter_name}: {base_value} -> {test_value}")

                command = [
                    tester_bin,
                    f"--settingsfile={temp_settings_file}",
                    "--engine",
                    f"name={engine_name}",
                    f"cmd={engine_cmd}",
                    "gauntlet=true",
                    f"option.{parameter_name}={test_value}",
                ]

                output_lines = run_and_tee(command, run_log)
                sprt_line, decision_line = get_summary_lines(output_lines)

                with collect_file.open("a", encoding="utf-8", newline="") as summary_handle:
                    summary_handle.write(f"{parameter_name}={test_value}, {sprt_line}\n")
                    if decision_line:
                        summary_handle.write(f"{decision_line}\n")

        print(f"Finished {total_runs} SPRT runs. Summary written to {collect_file}")
        return 0
    finally:
        if temp_settings_file and temp_settings_file.exists():
            temp_settings_file.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
