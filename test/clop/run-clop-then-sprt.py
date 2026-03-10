#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

DEFAULT_QET_EXE = Path(r"C:\development\bin\qet.exe")


@dataclass(frozen=True)
class OptionSpec:
    name: str
    default: int
    engine_min: int
    engine_max: int


OPTION_LINE_RE = re.compile(
    r"^\s*option\s+name\s+(?P<name>.+?)\s+type\s+spin\s+default\s+(?P<default>-?\d+)\s+min\s+(?P<min>-?\d+)\s+max\s+(?P<max>-?\d+)\s*$",
    re.IGNORECASE,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Startet zuerst einen CLOP-Lauf und danach einen SPRT-Lauf mit den optimierten "
            "Parametern aus einer Auftragsdatei (UCI option lines)."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "auftrag",
        help="Dateiname der Auftragsdatei mit UCI option lines",
    )
    parser.add_argument(
        "--qet",
        default=str(DEFAULT_QET_EXE),
        help="Pfad zu qet.exe",
    )
    parser.add_argument(
        "--clop-settings",
        default="clop-win.ini",
        help="CLOP settings file (wird nicht verändert)",
    )
    parser.add_argument(
        "--sprt-settings",
        default="sprt-win.ini",
        help="SPRT settings file (wird nicht verändert)",
    )
    parser.add_argument(
        "--log-dir",
        default="log",
        help="Verzeichnis für Lauf-Logs relativ zu test/clop",
    )
    return parser.parse_args()


def parse_order_file(order_file: Path) -> list[OptionSpec]:
    options: list[OptionSpec] = []

    for line_no, raw_line in enumerate(order_file.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue

        match = OPTION_LINE_RE.match(line)
        if not match:
            raise RuntimeError(
                f"Ungültige Zeile in Auftragsdatei {order_file} (Zeile {line_no}): {raw_line}"
            )

        name = match.group("name").strip()
        default = int(match.group("default"))
        engine_min = int(match.group("min"))
        engine_max = int(match.group("max"))

        if engine_min > engine_max:
            raise RuntimeError(
                f"Ungültige Grenzen für Option '{name}' in Zeile {line_no}: min > max"
            )

        if default < engine_min or default > engine_max:
            raise RuntimeError(
                f"Default außerhalb Engine-Grenzen für Option '{name}' in Zeile {line_no}"
            )

        options.append(
            OptionSpec(
                name=name,
                default=default,
                engine_min=engine_min,
                engine_max=engine_max,
            )
        )

    if not options:
        raise RuntimeError(f"Keine gültigen option-lines in Auftragsdatei gefunden: {order_file}")

    seen: set[str] = set()
    duplicates: list[str] = []
    for option in options:
        key = option.name.lower()
        if key in seen:
            duplicates.append(option.name)
        seen.add(key)

    if duplicates:
        duplicate_text = ", ".join(duplicates)
        raise RuntimeError(f"Doppelte Option-Namen in Auftragsdatei: {duplicate_text}")

    return options


def compute_clop_bounds(option: OptionSpec) -> tuple[int, int]:
    delta = max(abs(option.default) * 0.5, 10.0)

    raw_min = option.default - delta
    raw_max = option.default + delta

    clop_min = math.floor(raw_min)
    clop_max = math.ceil(raw_max)

    clop_min = max(clop_min, option.engine_min)
    clop_max = min(clop_max, option.engine_max)

    if clop_min == clop_max:
        if clop_min > option.engine_min:
            clop_min -= 1
        elif clop_max < option.engine_max:
            clop_max += 1

    if clop_min > clop_max:
        raise RuntimeError(
            f"Keine gültigen CLOP-Grenzen für Option '{option.name}' ableitbar"
        )

    if clop_min == clop_max:
        raise RuntimeError(
            f"Option '{option.name}' hat keinen optimierbaren Bereich nach Clamping ({clop_min})"
        )

    return clop_min, clop_max


def run_and_tee(command: list[str], log_file: Path, cwd: Path) -> tuple[list[str], int]:
    print("Running command:")
    print(" ".join(command))

    output_lines: list[str] = []
    with log_file.open("w", encoding="utf-8", newline="") as log_handle:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            cwd=str(cwd),
        )

        assert process.stdout is not None
        for line in process.stdout:
            output_lines.append(line)
            log_handle.write(line)
            sys.stdout.write(line)

        return_code = process.wait()

    return output_lines, return_code


def load_engine_cmd_from_ini(settings_file: Path) -> str:
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

    raise RuntimeError(f"[engine] cmd fehlt in Settings-Datei: {settings_file}")


def parse_last_estimate_table(output_lines: list[str], expected_names: set[str]) -> dict[str, float]:
    latest_values: dict[str, float] = {}

    header_re = re.compile(r"^\s*Parameter\s*\|\s*Estimated\s*\|", re.IGNORECASE)
    row_re = re.compile(r"^\s*(?P<name>[^|]+?)\s*\|\s*(?P<estimated>-?\d+(?:\.\d+)?)\s*\|")

    idx = 0
    while idx < len(output_lines):
        line = output_lines[idx].rstrip("\r\n")
        if not header_re.match(line):
            idx += 1
            continue

        j = idx + 1
        if j < len(output_lines):
            j += 1

        table_values: dict[str, float] = {}
        while j < len(output_lines):
            row_line = output_lines[j].rstrip("\r\n")
            if not row_line.strip():
                break
            if row_line.lstrip().startswith("Indicator"):
                break

            row_match = row_re.match(row_line)
            if row_match:
                name = row_match.group("name").strip()
                value = float(row_match.group("estimated"))
                if name in expected_names:
                    table_values[name] = value
            elif "|" not in row_line:
                break

            j += 1

        if table_values:
            latest_values = table_values

        idx = j

    if not latest_values:
        raise RuntimeError("Keine CLOP-Estimated-Tabelle in der Ausgabe gefunden")

    missing = sorted(name for name in expected_names if name not in latest_values)
    if missing:
        missing_text = ", ".join(missing)
        raise RuntimeError(f"CLOP-Ausgabe ohne Werte für Optionen: {missing_text}")

    return latest_values


def round_half_away_from_zero(value: float) -> int:
    if value >= 0:
        return int(math.floor(value + 0.5))
    return int(math.ceil(value - 0.5))


def clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def find_settings_file(script_dir: Path, requested_name: str, fallback_name: str | None = None) -> Path:
    direct = script_dir / requested_name
    if direct.is_file():
        return direct

    if fallback_name:
        fallback = script_dir / fallback_name
        if fallback.is_file():
            return fallback

    raise FileNotFoundError(f"Settings-Datei nicht gefunden: {direct}")


def main() -> int:
    args = parse_args()

    script_dir = Path(__file__).resolve().parent
    workspace_root = script_dir.parents[1]

    qet_exe = Path(args.qet)
    if not qet_exe.is_file():
        raise FileNotFoundError(f"qet.exe nicht gefunden: {qet_exe}")

    order_file = Path(args.auftrag)
    if not order_file.is_absolute():
        order_file = script_dir / order_file
    order_file = order_file.resolve()

    if not order_file.is_file():
        raise FileNotFoundError(f"Auftragsdatei nicht gefunden: {order_file}")

    clop_settings = find_settings_file(script_dir, args.clop_settings)
    sprt_settings = find_settings_file(script_dir, args.sprt_settings, fallback_name="sprt-win.in")

    options = parse_order_file(order_file)
    option_by_name = {option.name: option for option in options}

    time_stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    log_dir = (script_dir / args.log_dir).resolve()
    log_dir.mkdir(parents=True, exist_ok=True)

    clop_log = log_dir / f"clop-{order_file.stem}-{time_stamp}.log"
    sprt_log = log_dir / f"sprt-{order_file.stem}-{time_stamp}.log"

    clop_command: list[str] = [
        str(qet_exe),
        f"--settingsfile={clop_settings}",
    ]

    for option in options:
        clop_min, clop_max = compute_clop_bounds(option)
        clop_command.extend(
            [
                "--clopvalue",
                f"name={option.name}",
                f"min={clop_min}",
                f"max={clop_max}",
            ]
        )

    print("=== CLOP RUN START ===")
    clop_output, clop_exit = run_and_tee(clop_command, clop_log, cwd=workspace_root)
    print(f"CLOP exit code: {clop_exit}")

    if clop_exit != 0:
        raise RuntimeError(f"CLOP-Lauf fehlgeschlagen (Exit Code {clop_exit}). Log: {clop_log}")

    expected_names = {option.name for option in options}
    estimated_values = parse_last_estimate_table(clop_output, expected_names)

    optimized_values: dict[str, int] = {}
    for name, estimated in estimated_values.items():
        option = option_by_name[name]
        rounded = round_half_away_from_zero(estimated)
        optimized_values[name] = clamp(rounded, option.engine_min, option.engine_max)

    engine_cmd = load_engine_cmd_from_ini(sprt_settings)

    sprt_engine_name = f"Qapla opt clop {order_file.stem}"
    sprt_command: list[str] = [
        str(qet_exe),
        f"--settingsfile={sprt_settings}",
        "--engine",
        f"name={sprt_engine_name}",
        f"cmd={engine_cmd}",
        "proto=uci",
        "gauntlet=true",
    ]

    for option in options:
        value = optimized_values[option.name]
        sprt_command.append(f"option.{option.name}={value}")

    print("\nOptimierte Parameter für SPRT:")
    for option in options:
        value = optimized_values[option.name]
        print(f"  {option.name}={value}")

    print("\n=== SPRT RUN START ===")
    sprt_output, sprt_exit = run_and_tee(sprt_command, sprt_log, cwd=workspace_root)
    print(f"SPRT exit code: {sprt_exit}")

    print("\nLäufe beendet.")
    print(f"CLOP Log: {clop_log}")
    print(f"SPRT Log: {sprt_log}")

    for line in reversed([line.rstrip("\r\n") for line in sprt_output if line.strip()]):
        if "SPRT final result:" in line or line.startswith("decision:"):
            print(f"Summary: {line}")
            break

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
