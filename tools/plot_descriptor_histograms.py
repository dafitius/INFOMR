#!/usr/bin/env python3
"""Plot per-shape descriptor histograms as layered line charts.

Usage:
    python tools/plot_descriptor_histograms.py shape_descriptor_histograms.csv
    python tools/plot_descriptor_histograms.py input.csv --output-dir plots --legend

The output contains one PNG per class and descriptor, arranged in
<output-dir>/<class>/<descriptor>.png. Each shape is drawn as a separate line.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt

REQUIRED_COLUMNS = {
    "class",
    "file",
    "descriptor",
    "bin",
    "range_min",
    "range_max",
    "frequency",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv_file", type=Path, help="Per-shape descriptor histogram CSV"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Directory for plots (default: <CSV name>_plots next to the CSV)",
    )
    parser.add_argument(
        "--legend",
        action="store_true",
        help="Include object names in a legend (can be crowded for large classes)",
    )
    return parser.parse_args()


def read_histograms(
    csv_path: Path,
) -> dict[tuple[str, str], dict[str, list[tuple[int, float, float, float]]]]:
    histograms: dict[
        tuple[str, str], dict[str, list[tuple[int, float, float, float]]]
    ] = defaultdict(lambda: defaultdict(list))

    with csv_path.open("r", newline="", encoding="utf-8-sig") as source:
        rows = (line for line in source if not line.startswith("#"))
        reader = csv.DictReader(rows)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header")
        missing = REQUIRED_COLUMNS - set(reader.fieldnames)
        if missing:
            raise ValueError(
                f"CSV is missing required columns: {', '.join(sorted(missing))}"
            )

        for line_number, row in enumerate(reader, start=3):
            try:
                class_name = row["class"]
                file_name = row["file"]
                descriptor = row["descriptor"]
                if not class_name or not file_name or not descriptor:
                    raise ValueError("class, file, and descriptor must be non-empty")

                bin_index = int(row["bin"])
                range_min = float(row["range_min"])
                range_max = float(row["range_max"])
                frequency = float(row["frequency"])
                if bin_index < 0:
                    raise ValueError("bin must be non-negative")
                if not all(map(math.isfinite, (range_min, range_max, frequency))):
                    raise ValueError("range and frequency values must be finite")
                if range_max <= range_min:
                    raise ValueError("range_max must be greater than range_min")
                if frequency < 0:
                    raise ValueError("frequency must be non-negative")
            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"Invalid data on CSV line {line_number}: {error}"
                ) from error

            histograms[(class_name, descriptor)][file_name].append(
                (bin_index, range_min, range_max, frequency)
            )

    if not histograms:
        raise ValueError("CSV contains no histogram rows")

    for (class_name, descriptor), shapes in histograms.items():
        expected_bins: list[int] | None = None
        for file_name, bins in shapes.items():
            bins.sort(key=lambda item: item[0])
            indices = [item[0] for item in bins]
            if len(indices) != len(set(indices)):
                raise ValueError(
                    f"Duplicate bins for {class_name}/{descriptor}/{file_name}"
                )
            if indices != list(range(len(indices))):
                raise ValueError(
                    f"Bins for {class_name}/{descriptor}/{file_name} "
                    "must be contiguous and start at zero"
                )
            if expected_bins is None:
                expected_bins = indices
            elif indices != expected_bins:
                raise ValueError(
                    f"Inconsistent bin count for class {class_name}, descriptor {descriptor}"
                )

    return histograms


def safe_filename(value: str) -> str:
    return re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", value).strip(" .") or "unnamed"


def plot_histograms(
    histograms: dict[tuple[str, str], dict[str, list[tuple[int, float, float, float]]]],
    output_dir: Path,
    show_legend: bool,
) -> int:
    plots_written = 0
    for (class_name, descriptor), shapes in sorted(histograms.items()):
        fig, axis = plt.subplots(figsize=(10, 6), constrained_layout=True)
        colors = plt.get_cmap("turbo")
        shape_count = len(shapes)

        for shape_number, (file_name, bins) in enumerate(sorted(shapes.items())):
            x_values = [(low + high) / 2 for _, low, high, _ in bins]
            frequencies = [frequency for _, _, _, frequency in bins]
            axis.plot(
                x_values,
                frequencies,
                color=colors(shape_number / max(shape_count - 1, 1)),
                alpha=0.45,
                linewidth=0.9,
                label=Path(file_name).name if show_legend else None,
            )

        axis.set(
            title=f"{class_name} - {descriptor.upper()} ({shape_count} shapes)",
            xlabel=f"{descriptor.upper()} value",
            ylabel="Normalized frequency",
        )
        axis.grid(True, alpha=0.25)
        if show_legend:
            axis.legend(
                title="Shape",
                fontsize="x-small",
                ncol=2,
                bbox_to_anchor=(1.02, 1),
                loc="upper left",
            )

        class_dir = output_dir / safe_filename(class_name)
        class_dir.mkdir(parents=True, exist_ok=True)
        output_path = (
            class_dir
            / f"{safe_filename(descriptor.lower())}-{safe_filename(class_name)}.png"
        )
        fig.savefig(output_path, dpi=160)
        plt.close(fig)
        plots_written += 1

    return plots_written


def main() -> int:
    args = parse_args()
    csv_path = args.csv_file
    if not csv_path.is_file():
        print(f"CSV file not found: {csv_path}", file=sys.stderr)
        return 2

    output_dir = args.output_dir or csv_path.with_name(f"{csv_path.stem}_plots")
    try:
        histograms = read_histograms(csv_path)
        plots_written = plot_histograms(histograms, output_dir, args.legend)
    except (OSError, ValueError) as error:
        print(f"Could not create plots: {error}", file=sys.stderr)
        return 1

    print(f"Created {plots_written} plots in {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
