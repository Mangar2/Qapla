#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


DEFAULT_TESTER_BIN: dict[str, str] = {
    "linux": "/home/mangar/bin/qet",
    "win": r"C:\Development\qapla-engine-tester\build\release\qapla-engine-tester.exe",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run generic SPRT parameter sweep.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--platform",
        choices=("win", "linux"),
        default="win",
        help="Platform for tester binary default",
    )
    parser.add_argument(
        "--tester-bin",
        default=None,
        help="Path to tester binary (overrides platform default)",
    )
    parser.add_argument(
        "--settings-file",
        required=True,
        help="Path to .ini settings file used as template",
    )
    parser.add_argument(
        "--options-file",
        required=True,
        help="Path to Python file defining OPTIONS = [(name, base_value), ...]",
    )
    parser.add_argument(
        "--log-dir",
        default=None,
        help="Log directory (default: test/sprt/log)",
    )
    parser.add_argument(
        "--summary-log",
        default="sprt-summary.log",
        help="Summary log filename inside --log-dir",
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


def load_options(options_file: Path) -> list[tuple[str, int]]:
    scope = runpy.run_path(str(options_file))
    raw_options = scope.get("OPTIONS")
    if raw_options is None:
        raise RuntimeError(f"OPTIONS not found in options file: {options_file}")

    options: list[tuple[str, int]] = []
    for index, entry in enumerate(raw_options, start=1):
        if not isinstance(entry, (tuple, list)) or len(entry) != 2:
            raise RuntimeError(
                f"Invalid OPTIONS entry #{index} in {options_file}: expected (name, value)"
            )
        parameter_name = str(entry[0]).strip()
        try:
            base_value = int(entry[1])
        except (TypeError, ValueError) as exc:
            raise RuntimeError(
                f"Invalid value for OPTIONS entry #{index} in {options_file}: {entry[1]}"
            ) from exc

        if not parameter_name:
            raise RuntimeError(f"Invalid empty parameter name in OPTIONS entry #{index}")

        options.append((parameter_name, base_value))

    if not options:
        raise RuntimeError(f"OPTIONS is empty in {options_file}")

    return options


def run_and_tee(command: list[str], run_log: Path) -> tuple[list[str], int]:
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

    return output_lines, return_code


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


def load_completed_pairs(summary_file: Path) -> set[tuple[str, int]]:
    if not summary_file.is_file():
        return set()

    completed: set[tuple[str, int]] = set()
    pattern = re.compile(r"^\s*([^=\s,]+)\s*=\s*(-?\d+)\s*,")
    for line in summary_file.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if not match:
            continue
        parameter_name = match.group(1)
        parameter_value = int(match.group(2))
        completed.add((parameter_name, parameter_value))

    return completed


def main() -> int:
    args = parse_args()

    tester_bin = args.tester_bin or DEFAULT_TESTER_BIN[args.platform]

    script_dir = Path(__file__).resolve().parent
    settings_template = Path(args.settings_file).resolve()
    options_file = Path(args.options_file).resolve()

    if not settings_template.is_file():
        raise FileNotFoundError(f"Settings file not found: {settings_template}")
    if not options_file.is_file():
        raise FileNotFoundError(f"Options file not found: {options_file}")

    options = load_options(options_file)
    engine_cmd = load_engine_cmd(settings_template)

    log_dir = Path(args.log_dir).resolve() if args.log_dir else (script_dir / "log")
    summary_file = log_dir / args.summary_log
    log_dir.mkdir(parents=True, exist_ok=True)

    completed_pairs = load_completed_pairs(summary_file)

    temp_settings_file: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix="sprt-", suffix=".ini", delete=False) as temp_handle:
            temp_settings_file = Path(temp_handle.name)
        shutil.copyfile(settings_template, temp_settings_file)

        total_runs = len(options)
        run_index = 0
        executed_runs = 0
        skipped_runs = 0

        for parameter_name, test_value in options:
            run_index += 1
            pair = (parameter_name, test_value)

            if pair in completed_pairs:
                skipped_runs += 1
                print(
                    f"[{run_index}/{total_runs}] Skip {parameter_name}: {test_value} (already in {summary_file.name})"
                )
                continue

            executed_runs += 1
            engine_name = f"Qaplaoptspsa_{parameter_name}_{test_value}"
            run_log = log_dir / f"{run_index:03d}-{parameter_name}-{test_value}.log"

            print(f"[{run_index}/{total_runs}] Running {parameter_name}={test_value}")

            command = [
                tester_bin,
                f"--settingsfile={temp_settings_file}",
                "--engine",
                f"name={engine_name}",
                f"cmd={engine_cmd}",
                "gauntlet=true",
                f"option.{parameter_name}={test_value}",
            ]

            output_lines, return_code = run_and_tee(command, run_log)

            # WICHTIG: Der Return-Wert des Testers hängt vom SPRT-Ergebnis ab.
            # Auch ein Return-Wert != 0 ist hier kein Fehler und beendet den Lauf nicht.
            print(f"Tester exit code: {return_code} (ignored)")

            sprt_line, decision_line = get_summary_lines(output_lines)
            with summary_file.open("a", encoding="utf-8", newline="") as summary_handle:
                summary_handle.write(f"{parameter_name}={test_value}, {sprt_line}\n")
                if decision_line:
                    summary_handle.write(f"{decision_line}\n")

        print(
            f"Finished SPRT sweep. Total={total_runs}, executed={executed_runs}, skipped={skipped_runs}. "
            f"Summary written to {summary_file}"
        )
        return 0
    finally:
        if temp_settings_file and temp_settings_file.exists():
            temp_settings_file.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
