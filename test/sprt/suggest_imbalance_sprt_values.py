#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path


OFFSETS = (-48, -16, 16, 48)
TOP_PER_PARAMETER = 1
OFFSET_MIN = -48
OFFSET_MAX = 48
OFFSET_STEP = 1
MIN_PROBABILITY = 50.0


@dataclass
class Proposal:
    parameter: str
    startwert: int
    suggested_value: int
    offset: int
    predicted_winrate: float
    adjusted_probability: float
    risk_penalty: float
    model: str
    left_drop: float
    right_drop: float
    expected_gain: float


def get_default_output_file(summary_csv: Path) -> Path:
    name = summary_csv.stem
    if name.endswith("-summary"):
        output_name = f"{name[:-8]}-proposals.csv"
    else:
        output_name = f"{name}-proposals.csv"
    return summary_csv.with_name(output_name)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Suggest follow-up SPRT values from imbalance-sprt-summary.csv",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "summary_csv",
        help="Path to imbalance-sprt-summary.csv",
    )
    return parser.parse_args()


def parse_float(value: str | None) -> float | None:
    if value is None:
        return None
    stripped = value.strip()
    if not stripped:
        return None
    try:
        return float(stripped)
    except ValueError:
        return None


def solve_3x3(system: list[list[float]], rhs: list[float]) -> tuple[float, float, float]:
    matrix = [row[:] for row in system]
    values = rhs[:]

    for pivot_idx in range(3):
        best_row = max(range(pivot_idx, 3), key=lambda row_idx: abs(matrix[row_idx][pivot_idx]))
        if abs(matrix[best_row][pivot_idx]) < 1e-12:
            raise ValueError("Singular system while fitting quadratic")
        if best_row != pivot_idx:
            matrix[pivot_idx], matrix[best_row] = matrix[best_row], matrix[pivot_idx]
            values[pivot_idx], values[best_row] = values[best_row], values[pivot_idx]

        pivot = matrix[pivot_idx][pivot_idx]
        for col_idx in range(pivot_idx, 3):
            matrix[pivot_idx][col_idx] /= pivot
        values[pivot_idx] /= pivot

        for row_idx in range(3):
            if row_idx == pivot_idx:
                continue
            factor = matrix[row_idx][pivot_idx]
            for col_idx in range(pivot_idx, 3):
                matrix[row_idx][col_idx] -= factor * matrix[pivot_idx][col_idx]
            values[row_idx] -= factor * values[pivot_idx]

    return values[0], values[1], values[2]


def fit_quadratic_least_squares(points: list[tuple[float, float]]) -> tuple[float, float, float]:
    sx0 = float(len(points))
    sx1 = sum(x for x, _ in points)
    sx2 = sum(x * x for x, _ in points)
    sx3 = sum(x * x * x for x, _ in points)
    sx4 = sum(x * x * x * x for x, _ in points)

    sy0 = sum(y for _, y in points)
    sy1 = sum(x * y for x, y in points)
    sy2 = sum(x * x * y for x, y in points)

    normal_matrix = [
        [sx4, sx3, sx2],
        [sx3, sx2, sx1],
        [sx2, sx1, sx0],
    ]
    normal_rhs = [sy2, sy1, sy0]

    return solve_3x3(normal_matrix, normal_rhs)


def eval_quadratic(coeffs: tuple[float, float, float], x: float) -> float:
    a, b, c = coeffs
    return a * x * x + b * x + c


def clamp(value: float, lower: float, upper: float) -> float:
    return max(lower, min(upper, value))


def build_points(row: dict[str, str]) -> list[tuple[float, float]]:
    mapping = {
        -48: parse_float(row.get("winrate_-48")),
        -16: parse_float(row.get("winrate_-16")),
        16: parse_float(row.get("winrate_+16")),
        48: parse_float(row.get("winrate_+48")),
    }
    points: list[tuple[float, float]] = []
    for offset in OFFSETS:
        value = mapping[offset]
        if value is not None:
            points.append((float(offset), value))
    return points


def get_drop(row: dict[str, str], left_key: str, right_key: str) -> float:
    left = parse_float(row.get(left_key))
    right = parse_float(row.get(right_key))
    if left is None or right is None:
        return 0.0
    return left - right


def collect_proposals(summary_csv: Path) -> list[Proposal]:
    proposals: list[Proposal] = []

    with summary_csv.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            parameter = (row.get("parameter") or "").strip()
            startwert_raw = (row.get("startwert") or "").strip()
            if not parameter or not startwert_raw:
                continue

            startwert = int(float(startwert_raw))
            points = build_points(row)
            if len(points) < 3:
                continue

            model = "quadratic_ls"
            gain_points = [(x, y - 50.0) for x, y in points]
            quadratic_gain_coeffs = fit_quadratic_least_squares(gain_points)
            left_drop = get_drop(row, "winrate_-16", "winrate_-48")
            right_drop = get_drop(row, "winrate_+16", "winrate_+48")
            cliff_bias = max(0.0, left_drop - right_drop)

            parameter_candidates: list[Proposal] = []
            baseline_gain_at_zero = eval_quadratic(quadratic_gain_coeffs, 0.0)
            for offset in range(OFFSET_MIN, OFFSET_MAX + 1, OFFSET_STEP):
                if offset == 0:
                    continue

                predicted_gain = eval_quadratic(quadratic_gain_coeffs, float(offset))
                predicted_gain -= baseline_gain_at_zero
                predicted = clamp(50.0 + predicted_gain, 0.0, 100.0)

                negative_factor = max(0.0, -float(offset)) / 48.0
                risk_penalty = cliff_bias * negative_factor
                adjusted = clamp(predicted - risk_penalty, 0.0, 100.0)
                expected_gain = adjusted - 50.0

                if expected_gain <= 0.0 or adjusted <= MIN_PROBABILITY:
                    continue

                parameter_candidates.append(
                    Proposal(
                        parameter=parameter,
                        startwert=startwert,
                        suggested_value=startwert + offset,
                        offset=offset,
                        predicted_winrate=predicted,
                        adjusted_probability=adjusted,
                        risk_penalty=risk_penalty,
                        model=model,
                        left_drop=left_drop,
                        right_drop=right_drop,
                        expected_gain=expected_gain,
                    )
                )

            parameter_candidates.sort(
                key=lambda item: (
                    item.expected_gain,
                    item.predicted_winrate,
                    abs(item.offset),
                ),
                reverse=True,
            )

            used_values: set[int] = set()
            kept_count = 0
            for candidate in parameter_candidates:
                if candidate.suggested_value in used_values:
                    continue
                proposals.append(candidate)
                used_values.add(candidate.suggested_value)
                kept_count += 1
                if kept_count >= TOP_PER_PARAMETER:
                    break

    proposals.sort(
        key=lambda item: (
            item.expected_gain,
            item.predicted_winrate,
        ),
        reverse=True,
    )

    return proposals


def write_proposals(output_file: Path, proposals: list[Proposal]) -> None:
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with output_file.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "rank",
                "parameter",
                "startwert",
                "suggested_value",
                "offset",
                "predicted_winrate",
                "adjusted_probability",
                "risk_penalty",
                "expected_gain",
                "model",
                "left_drop",
                "right_drop",
            ]
        )

        for rank, proposal in enumerate(proposals, start=1):
            writer.writerow(
                [
                    rank,
                    proposal.parameter,
                    proposal.startwert,
                    proposal.suggested_value,
                    proposal.offset,
                    f"{proposal.predicted_winrate:.3f}",
                    f"{proposal.adjusted_probability:.3f}",
                    f"{proposal.risk_penalty:.3f}",
                    f"{proposal.expected_gain:.3f}",
                    proposal.model,
                    f"{proposal.left_drop:.3f}",
                    f"{proposal.right_drop:.3f}",
                ]
            )


def main() -> int:
    args = parse_args()
    summary_csv = Path(args.summary_csv)
    if not summary_csv.is_file():
        raise FileNotFoundError(f"Summary CSV not found: {summary_csv}")

    proposals = collect_proposals(summary_csv)
    output_file = get_default_output_file(summary_csv)
    write_proposals(output_file, proposals)

    print(f"Wrote {len(proposals)} proposals to {output_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
