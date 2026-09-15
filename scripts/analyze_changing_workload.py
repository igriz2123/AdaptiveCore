"""Create research plots for the changing-workload adaptation evaluation."""

from pathlib import Path
import argparse

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "results" / "graphs"
SYSTEMS = ["BPlusTree", "PGMIndex", "AdaptiveIndex"]


def load_results(path: Path) -> pd.DataFrame:
    results = pd.read_csv(path)
    required = {
        "RecordType", "Phase", "System", "AverageNanoseconds",
        "SwitchOperationNumber", "MeasuredOperation", "OperationsPerSecond",
    }
    missing = required - set(results.columns)
    if missing:
        raise ValueError(f"Changing-workload CSV is missing: {', '.join(sorted(missing))}")
    return results


def save(figure: plt.Figure, filename: str) -> None:
    figure.tight_layout()
    figure.savefig(OUTPUT_DIR / filename, dpi=150)
    plt.close(figure)


def plot_phase_latency(results: pd.DataFrame) -> None:
    summaries = results[results["RecordType"] == "Summary"]
    figure, axis = plt.subplots(figsize=(9, 5))
    for system in SYSTEMS:
        data = summaries[summaries["System"] == system].sort_values("Phase")
        axis.plot(data["Phase"], data["AverageNanoseconds"], label=system)
    axis.set(title="Average latency by workload phase", xlabel="Phase", ylabel="Average ns per request")
    axis.set_xticks(sorted(summaries["Phase"].unique()))
    axis.grid(True)
    axis.legend()
    save(figure, "changing_workload_phase_latency.png")


def plot_switch_timeline(results: pd.DataFrame) -> None:
    summaries = results[(results["RecordType"] == "Summary") & (results["System"] == "AdaptiveIndex")]
    switches = results[results["RecordType"] == "Switch"].sort_values("SwitchOperationNumber")
    choices = {"Hash": 0, "BPlusTree": 1, "PGM": 2}
    figure, axis = plt.subplots(figsize=(10, 5))
    timeline_operations = pd.concat([pd.Series([0]), switches["SwitchOperationNumber"]], ignore_index=True)
    timeline_choices = pd.concat([pd.Series([choices["Hash"]]), switches["ActiveIndex"].map(choices)], ignore_index=True)
    axis.step(timeline_operations, timeline_choices, where="post")
    for boundary in summaries["PhaseBeginOperation"].drop_duplicates().sort_values().iloc[1:]:
        axis.axvline(boundary)
    axis.set(title="Adaptive index switch timeline", xlabel="Operation number", ylabel="Active index")
    axis.set_yticks(list(choices.values()), list(choices.keys()))
    axis.grid(True, axis="y")
    save(figure, "adaptive_switch_timeline.png")


def plot_pre_post(results: pd.DataFrame) -> None:
    switches = results[results["RecordType"] == "Switch"].dropna(subset=["PreSwitchAverageNanoseconds", "PostSwitchAverageNanoseconds"])
    if switches.empty:
        return
    figure, axis = plt.subplots(figsize=(9, 5))
    positions = range(len(switches))
    axis.plot(positions, switches["PreSwitchAverageNanoseconds"], label="Pre-switch")
    axis.plot(positions, switches["PostSwitchAverageNanoseconds"], label="Post-switch")
    axis.set(title="Latency around actual adaptive switches", xlabel="Switch event", ylabel="Average ns per request")
    axis.set_xticks(list(positions), [str(value) for value in switches["SwitchOperationNumber"]])
    axis.grid(True)
    axis.legend()
    save(figure, "adaptive_pre_post_switch_latency.png")


def plot_update_throughput(results: pd.DataFrame) -> None:
    updates = results[results["RecordType"] == "UpdateThroughput"].dropna(subset=["OperationsPerSecond"])
    figure, axes = plt.subplots(1, 2, figsize=(12, 5), sharey=True)
    for axis, operation in zip(axes, ["Insert", "Delete"]):
        data = updates[updates["MeasuredOperation"] == operation]
        for system in SYSTEMS:
            values = data[data["System"] == system].sort_values("Phase")
            values = values.set_index("Phase").reindex(range(1, 6))
            axis.plot(values.index, values["OperationsPerSecond"], label=system)
        axis.set(title=f"{operation} throughput", xlabel="Phase", ylabel="Operations per second")
        axis.grid(True)
    axes[0].legend()
    save(figure, "changing_workload_update_throughput.png")


def plot_baseline_difference(results: pd.DataFrame) -> None:
    adaptive = results[(results["RecordType"] == "Summary") & (results["System"] == "AdaptiveIndex")]
    figure, axis = plt.subplots(figsize=(9, 5))
    axis.plot(adaptive["Phase"], adaptive["AdaptiveVsBPlusPercent"], label="Adaptive vs BPlusTree")
    axis.plot(adaptive["Phase"], adaptive["AdaptiveVsPGMPercent"], label="Adaptive vs PGMIndex")
    axis.axhline(0)
    axis.set(title="Adaptive latency relative to fixed baselines", xlabel="Phase", ylabel="Latency difference (%)")
    axis.grid(True)
    axis.legend()
    save(figure, "adaptive_relative_to_baselines.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=PROJECT_ROOT / "changing_workload_results.csv")
    args = parser.parse_args()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    results = load_results(args.input)
    plot_phase_latency(results)
    plot_switch_timeline(results)
    plot_pre_post(results)
    plot_update_throughput(results)
    plot_baseline_difference(results)
    print(f"Graphs written to {OUTPUT_DIR.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
