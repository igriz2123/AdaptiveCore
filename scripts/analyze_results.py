from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
INPUT_PATH = PROJECT_ROOT / "experiment_results.csv"
OUTPUT_DIR = PROJECT_ROOT / "results" / "graphs"

REQUIRED_COLUMNS = {
    "DatasetSize",
    "Index",
    "Dataset",
    "Operation",
    "OperationCount",
    "TotalNanoseconds",
    "AverageNanoseconds",
}
WORKLOADS = ["Sequential", "Random", "Clustered"]
INDEXES = ["HashIndex", "BPlusTree", "PGMIndex", "AdaptiveIndex"]
OPERATIONS = {
    "PointLookup": "point_lookup_performance.png",
    "RangeQuery": "range_query_performance.png",
    "Insert": "insert_performance.png",
    "Delete": "delete_performance.png",
}
COLORS = {
    "HashIndex": "#1f77b4",
    "BPlusTree": "#ff7f0e",
    "PGMIndex": "#2ca02c",
    "AdaptiveIndex": "#d62728",
}


def load_results() -> pd.DataFrame:
    if not INPUT_PATH.exists():
        raise FileNotFoundError(f"Benchmark input not found: {INPUT_PATH}")

    results = pd.read_csv(INPUT_PATH)
    missing_columns = REQUIRED_COLUMNS - set(results.columns)
    if missing_columns:
        missing = ", ".join(sorted(missing_columns))
        raise ValueError(f"Benchmark CSV is missing columns: {missing}")

    numeric_columns = [
        "DatasetSize",
        "OperationCount",
        "TotalNanoseconds",
        "AverageNanoseconds",
    ]
    for column in numeric_columns:
        results[column] = pd.to_numeric(results[column], errors="raise")

    if results.empty:
        raise ValueError("Benchmark CSV contains no records")
    if (results["AverageNanoseconds"] < 0).any():
        raise ValueError("AverageNanoseconds contains a negative value")

    return results


def plot_operation(results: pd.DataFrame, operation: str, filename: str) -> None:
    operation_results = results[results["Operation"] == operation]
    if operation_results.empty:
        raise ValueError(f"No records found for operation: {operation}")

    figure, axes = plt.subplots(1, len(WORKLOADS), figsize=(16, 5), sharey=True)
    figure.suptitle(f"{operation} performance by workload")

    for axis, workload in zip(axes, WORKLOADS):
        workload_results = operation_results[
            operation_results["Dataset"] == workload
        ]
        for index_name in INDEXES:
            index_results = workload_results[
                workload_results["Index"] == index_name
            ].sort_values("DatasetSize")
            if index_results.empty:
                continue
            axis.plot(
                index_results["DatasetSize"],
                index_results["AverageNanoseconds"],
                marker="o",
                label=index_name,
                color=COLORS[index_name],
            )

        axis.set_title(workload)
        axis.set_xlabel("Dataset size")
        axis.set_xscale("log")
        axis.set_yscale("log")
        axis.grid(True, which="both", linestyle=":", alpha=0.5)

    axes[0].set_ylabel("Average nanoseconds per operation (log scale)")
    handles, labels = axes[0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="lower center", ncol=4)
    figure.tight_layout(rect=(0, 0.1, 1, 0.93))
    figure.savefig(OUTPUT_DIR / filename, dpi=150)
    plt.close(figure)


def plot_bulk_load(results: pd.DataFrame) -> None:
    bulk_results = results[results["Operation"] == "BulkLoad"]
    if bulk_results.empty:
        raise ValueError("No BulkLoad records found")
    if set(bulk_results["Index"]) != {"PGMIndex"}:
        raise ValueError("BulkLoad records must belong only to PGMIndex")

    figure, axes = plt.subplots(1, len(WORKLOADS), figsize=(16, 5), sharey=True)
    figure.suptitle("PGMIndex BulkLoad performance by workload")

    for axis, workload in zip(axes, WORKLOADS):
        workload_results = bulk_results[
            bulk_results["Dataset"] == workload
        ].sort_values("DatasetSize")
        axis.plot(
            workload_results["DatasetSize"],
            workload_results["AverageNanoseconds"],
            marker="o",
            color=COLORS["PGMIndex"],
            label="PGMIndex BulkLoad",
        )
        axis.set_title(workload)
        axis.set_xlabel("Dataset size")
        axis.set_xscale("log")
        axis.set_yscale("log")
        axis.grid(True, which="both", linestyle=":", alpha=0.5)

    axes[0].set_ylabel("Average nanoseconds per operation (log scale)")
    figure.legend(loc="lower center")
    figure.tight_layout(rect=(0, 0.1, 1, 0.93))
    figure.savefig(OUTPUT_DIR / "pgm_bulk_load_performance.png", dpi=150)
    plt.close(figure)


def print_summary(results: pd.DataFrame) -> None:
    print(f"Loaded {len(results)} benchmark records from {INPUT_PATH.name}")
    print("Fastest measured index by operation and workload (mean across sizes):")

    standard_results = results[results["Operation"].isin(OPERATIONS)]
    grouped = (
        standard_results.groupby(["Operation", "Dataset", "Index"], as_index=False)[
            "AverageNanoseconds"
        ]
        .mean()
    )
    for operation in OPERATIONS:
        for workload in WORKLOADS:
            candidates = grouped[
                (grouped["Operation"] == operation)
                & (grouped["Dataset"] == workload)
            ].sort_values("AverageNanoseconds")
            if candidates.empty:
                continue
            fastest = candidates.iloc[0]
            print(
                f"  {operation:11} | {workload:10} | "
                f"{fastest['Index']:13} | "
                f"mean average ns/op: {fastest['AverageNanoseconds']:.2f}"
            )

    bulk_summary = (
        results[results["Operation"] == "BulkLoad"]
        .groupby("Dataset")["AverageNanoseconds"]
        .mean()
    )
    print("PGMIndex BulkLoad mean average ns/op across sizes:")
    for workload in WORKLOADS:
        if workload in bulk_summary:
            print(f"  {workload:10} | {bulk_summary[workload]:.2f}")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    results = load_results()

    for operation, filename in OPERATIONS.items():
        plot_operation(results, operation, filename)
    plot_bulk_load(results)
    print_summary(results)
    print(f"Graphs written to {OUTPUT_DIR.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
