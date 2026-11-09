#!/usr/bin/env python3
"""Plot query, micro-average, or macro-average ROC curves from evaluation CSVs.

Usage:
    python tools/plot_roc.py evaluation_roc.csv
    python tools/plot_roc.py evaluation_roc.csv --query 3 --output query_3_roc.png
    python tools/plot_roc.py evaluation_roc.csv --average micro
    python tools/plot_roc.py evaluation_roc.csv --average macro

For an average plot, the evaluator must also have produced the companion
<CSV stem>_scores<suffix> file containing candidate-level distances and labels.
Micro ROC pools all query-candidate pairs by raw distance. Macro ROC computes
one-vs-rest curves per query class and gives each class equal weight.
"""

from __future__ import annotations

import argparse
import array
import csv
import math
import sys
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")

import matplotlib.pyplot as plt

REQUIRED_COLUMNS = {
    "query_index",
    "query_class",
    "query_file",
    "threshold",
    "false_positive_rate",
    "true_positive_rate",
}
SCORE_COLUMNS = {
    "query_index",
    "query_class",
    "query_file",
    "candidate_class",
    "candidate_file",
    "distance",
    "relevant",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_file", type=Path, help="ROC data CSV from --evaluate")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--query",
        type=int,
        help="Query index to plot (default: 0 when --average is not used)",
    )
    selection.add_argument(
        "--average",
        choices=("micro", "macro"),
        help="Plot a pooled micro ROC or class-macro ROC across queries",
    )
    parser.add_argument(
        "--scores",
        type=Path,
        help="Candidate-level scores CSV (default: <CSV stem>_scores.csv)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="PNG path (default depends on --query or --average)",
    )
    return parser.parse_args()


def read_curve(csv_path: Path, query_index: int) -> tuple[str, str, list[tuple[float, float]]]:
    points: list[tuple[float, float]] = []
    query_class = ""
    query_file = ""

    with csv_path.open("r", newline="", encoding="utf-8-sig") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header")
        missing = REQUIRED_COLUMNS - set(reader.fieldnames)
        if missing:
            raise ValueError(f"CSV is missing columns: {', '.join(sorted(missing))}")

        for line_number, row in enumerate(reader, start=2):
            try:
                row_query_index = int(row["query_index"])
            except (TypeError, ValueError) as error:
                raise ValueError(f"Invalid query index on CSV line {line_number}") from error
            if row_query_index != query_index:
                continue

            try:
                false_positive_rate = float(row["false_positive_rate"])
                true_positive_rate = float(row["true_positive_rate"])
                if not math.isfinite(false_positive_rate) or not math.isfinite(true_positive_rate):
                    raise ValueError("rates must be finite")
                if not 0.0 <= false_positive_rate <= 1.0 or not 0.0 <= true_positive_rate <= 1.0:
                    raise ValueError("rates must be between 0 and 1")
                if row["threshold"]:
                    threshold = float(row["threshold"])
                    if not math.isfinite(threshold):
                        raise ValueError("threshold must be finite")
            except (TypeError, ValueError) as error:
                raise ValueError(f"Invalid ROC point on CSV line {line_number}: {error}") from error

            if points and (
                false_positive_rate < points[-1][0] or true_positive_rate < points[-1][1]
            ):
                raise ValueError(f"ROC rates decrease on CSV line {line_number}")

            query_class = row["query_class"]
            query_file = row["query_file"]
            points.append((false_positive_rate, true_positive_rate))

    if not points:
        raise ValueError(f"No ROC data found for query {query_index}")
    if points[0] != (0.0, 0.0) or points[-1] != (1.0, 1.0):
        raise ValueError(f"Query {query_index} does not have a complete ROC curve")

    return query_class, query_file, points


def read_candidate_scores(
    csv_path: Path,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, list[str]]:
    scores = array.array("d")
    labels = bytearray()
    query_classes = array.array("I")
    class_names: list[str] = []
    class_indices: dict[str, int] = {}

    with csv_path.open("r", newline="", encoding="utf-8-sig") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames is None:
            raise ValueError("Candidate scores CSV has no header")
        missing = SCORE_COLUMNS - set(reader.fieldnames)
        if missing:
            raise ValueError(
                f"Candidate scores CSV is missing columns: {', '.join(sorted(missing))}"
            )

        for line_number, row in enumerate(reader, start=2):
            try:
                int(row["query_index"])
                query_class = row["query_class"]
                candidate_class = row["candidate_class"]
                if not query_class or not candidate_class:
                    raise ValueError("query_class and candidate_class must be non-empty")
                distance = float(row["distance"])
                relevant = int(row["relevant"])
                if not math.isfinite(distance):
                    raise ValueError("distance must be finite")
                if relevant not in (0, 1):
                    raise ValueError("relevant must be 0 or 1")
                if relevant != int(candidate_class == query_class):
                    raise ValueError("relevant does not match candidate_class and query_class")
            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"Invalid candidate score on CSV line {line_number}: {error}"
                ) from error

            class_index = class_indices.get(query_class)
            if class_index is None:
                class_index = len(class_names)
                class_indices[query_class] = class_index
                class_names.append(query_class)

            scores.append(distance)
            labels.append(relevant)
            query_classes.append(class_index)

    if not scores:
        raise ValueError("Candidate scores CSV contains no rows")

    return (
        np.frombuffer(scores, dtype=np.float64),
        np.frombuffer(labels, dtype=np.uint8),
        np.frombuffer(query_classes, dtype=np.uint32),
        class_names,
    )


def roc_curve(scores: np.ndarray, labels: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    positive_count = int(np.count_nonzero(labels))
    negative_count = len(labels) - positive_count
    if positive_count == 0 or negative_count == 0:
        raise ValueError("ROC requires both relevant and irrelevant candidates")

    order = np.argsort(scores, kind="stable")
    sorted_scores = scores[order]
    sorted_labels = labels[order]
    group_ends = np.flatnonzero(np.r_[sorted_scores[1:] != sorted_scores[:-1], True]) + 1
    cumulative_positives = np.cumsum(sorted_labels, dtype=np.int64)[group_ends - 1]
    cumulative_negatives = group_ends - cumulative_positives
    false_positive_rates = np.r_[0.0, cumulative_negatives / negative_count]
    true_positive_rates = np.r_[0.0, cumulative_positives / positive_count]
    return false_positive_rates, true_positive_rates


def area_under_curve(false_positive_rates: np.ndarray, true_positive_rates: np.ndarray) -> float:
    return float(
        np.sum(
            np.diff(false_positive_rates)
            * (true_positive_rates[1:] + true_positive_rates[:-1])
            / 2.0
        )
    )


def average_curve(
    score_path: Path,
    average: str,
) -> tuple[str, np.ndarray, np.ndarray, float]:
    scores, labels, query_classes, class_names = read_candidate_scores(score_path)

    if average == "micro":
        false_positive_rates, true_positive_rates = roc_curve(scores, labels)
        area = area_under_curve(false_positive_rates, true_positive_rates)
        return "Micro-average ROC", false_positive_rates, true_positive_rates, area

    class_order = np.argsort(query_classes, kind="stable")
    sorted_classes = query_classes[class_order]
    group_starts = np.r_[0, np.flatnonzero(sorted_classes[1:] != sorted_classes[:-1]) + 1]
    group_ends = np.r_[group_starts[1:], len(sorted_classes)]
    grid = np.linspace(0.0, 1.0, 10001)
    class_curves = []
    class_areas = []

    for start, end in zip(group_starts, group_ends):
        indices = class_order[start:end]
        class_false_positive_rates, class_true_positive_rates = roc_curve(
            scores[indices], labels[indices]
        )
        starts = np.r_[True, class_false_positive_rates[1:] != class_false_positive_rates[:-1]]
        unique_fpr = class_false_positive_rates[starts]
        unique_tpr = np.maximum.reduceat(class_true_positive_rates, np.flatnonzero(starts))
        if unique_fpr[-1] == 1.0:
            unique_tpr[-1] = np.min(class_true_positive_rates[class_false_positive_rates == 1.0])
        class_curves.append(np.interp(grid, unique_fpr, unique_tpr))
        class_areas.append(area_under_curve(class_false_positive_rates, class_true_positive_rates))

    if not class_curves:
        raise ValueError("No class curves available for macro averaging")
    mean_true_positive_rates = np.mean(class_curves, axis=0)
    grid = np.r_[grid, 1.0]
    mean_true_positive_rates = np.r_[mean_true_positive_rates, 1.0]
    area = float(np.mean(class_areas))
    return (
        f"Macro-average ROC ({len(class_curves)} classes)",
        grid,
        mean_true_positive_rates,
        area,
    )


def save_plot(
    title: str,
    false_positive_rates: np.ndarray,
    true_positive_rates: np.ndarray,
    area: float,
    output_path: Path,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure, axis = plt.subplots(figsize=(7, 7), constrained_layout=True)
    axis.plot(
        false_positive_rates,
        true_positive_rates,
        linewidth=2,
        label=f"AUROC = {area:.4f}",
    )
    axis.fill_between(false_positive_rates, true_positive_rates, alpha=0.15)
    axis.plot([0, 1], [0, 1], linestyle="--", color="gray", label="Random")
    axis.set(
        title=title,
        xlabel="False positive rate",
        ylabel="True positive rate",
        xlim=(0, 1),
        ylim=(0, 1),
    )
    axis.set_aspect("equal", adjustable="box")
    axis.grid(True, alpha=0.25)
    axis.legend(loc="lower right")
    figure.savefig(output_path, dpi=160)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    query_index = 0 if args.query is None else args.query
    if query_index < 0:
        print("Query index must be non-negative", file=sys.stderr)
        return 2
    if not args.csv_file.is_file():
        print(f"CSV file not found: {args.csv_file}", file=sys.stderr)
        return 2

    average = args.average
    if average:
        score_path = args.scores or args.csv_file.with_name(
            f"{args.csv_file.stem}_scores{args.csv_file.suffix}"
        )
        output_path = args.output or args.csv_file.with_name(
            f"{args.csv_file.stem}_{average}.png"
        )
    else:
        score_path = None
        output_path = args.output or args.csv_file.with_name(
            f"{args.csv_file.stem}_query_{query_index}.png"
        )
    try:
        if average:
            if not score_path.is_file():
                raise ValueError(
                    f"Candidate scores CSV not found: {score_path}. "
                    "Run --evaluate again with a ROC CSV output path."
                )
            title, x_values, y_values, area = average_curve(
                score_path, average
            )
        else:
            query_class, query_file, points = read_curve(args.csv_file, query_index)
            x_values = np.array([point[0] for point in points])
            y_values = np.array([point[1] for point in points])
            area = area_under_curve(x_values, y_values)
            title = f"ROC - {query_class}/{query_file}"
        save_plot(title, x_values, y_values, area, output_path)
    except (OSError, ValueError) as error:
        print(f"Could not create ROC plot: {error}", file=sys.stderr)
        return 1

    print(f"Created ROC plot (AUROC = {area:.4f}): {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
