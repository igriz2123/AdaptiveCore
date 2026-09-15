# AdaptiveCore

AdaptiveCore is a C++17 research and teaching prototype for adaptive indexing. It implements three individual indexes, an adaptive facade that selects among them from recent workload counts, reproducible workload generation, benchmarking, CSV reporting, and Python visualization.

The project is intended for controlled experiments and education. It is not a production database.

## Architecture

### Common Index Interface

[`src/index/Index.h`](src/index/Index.h) defines the common interface:

- `insert(int key, const std::string& value)`
- `find(int key)`
- `erase(int key)`
- `range(int lower_key, int upper_key)`
- `size()`

HashIndex, BPlusTree, PGMIndex, and AdaptiveIndex implement this interface. `PGMIndex::bulk_load()` is intentionally PGM-specific and is not part of the common interface.

### Individual Indexes

#### HashIndex

[`HashIndex`](src/index/HashIndex.h) stores entries in `std::unordered_map`.

- Point operations use hash lookup.
- Range queries scan all entries and sort matching results.
- Duplicate inserts update the existing value.

#### BPlusTree

[`BPlusTree`](src/index/BPlusTree.h) is a custom in-memory B+Tree with:

- Configurable maximum keys per node.
- Sorted leaf entries.
- Linked leaves for range queries.
- Parent pointers.
- Leaf splitting.
- Internal-node splitting and recursive propagation.
- Point lookup, range query, insertion, and simplified deletion.

Deletion currently removes entries from leaves without borrowing, merging, or full tree rebalancing.

#### PGMIndex

[`PGMIndex`](src/index/PGMIndex.h) is an educational PGM-inspired learned index. It is not the official PGM library.

It stores sorted entries and piecewise-linear segments that approximate key-to-position relationships. Point lookup uses a predicted position to create a bounded search window, then verifies the actual key in the sorted vector.

PGMIndex also provides:

- `bulk_load()` for constructing from a complete dataset.
- Deferred model rebuilding after normal inserts and successful deletes.
- Immediate model rebuilding after bulk loading.

The `error_bound` (epsilon) controls the maximum allowed absolute position
prediction error while building each piecewise-linear segment. Smaller values
generally require more segments; the epsilon experiment measures the actual
tradeoff rather than assuming that trend.

### Adaptive Layer

The adaptive implementation is under [`src/adaptive`](src/adaptive):

- [`IndexChoice.h`](src/adaptive/IndexChoice.h) defines `Hash`, `BPlusTree`, and `PGM` choices.
- [`WorkloadAnalyzer.h`](src/adaptive/WorkloadAnalyzer.h) counts recent operations in fixed windows.
- [`AdaptivePolicy.h`](src/adaptive/AdaptivePolicy.h) deterministically selects an index choice.
- [`AdaptiveIndex.h`](src/adaptive/AdaptiveIndex.h) implements the common `Index` interface.

`AdaptiveIndex` maintains:

- A canonical `std::map<int, std::string>` containing all data.
- One active concrete index.
- The current index choice.
- A fixed-window workload analyzer.
- A deterministic policy.

The initial active index is `HashIndex`.

For every operation, AdaptiveIndex updates or queries the active index and records the operation type:

- Insert
- PointLookup
- RangeQuery
- Delete

When a window completes, the policy evaluates the counts:

1. Select `BPlusTree` when `range_queries >= point_lookups` and `range_queries >= writes`, where `writes = inserts + deletes`.
2. Otherwise select `PGM` when `writes == 0` and there is at least one point lookup.
3. Otherwise select `Hash`.

The default operation window is 64 operations. The default minimum interval between switches is two completed windows. This cooldown prevents excessive rebuilding and switching.

When a switch is allowed, AdaptiveIndex creates a fresh target index and rebuilds it from canonical data. It does not migrate directly between index implementations. PGM rebuilds use `bulk_load()`; HashIndex and BPlusTree rebuilds use normal inserts.

## Workload Generation

[`DatasetGenerator`](src/benchmark/DatasetGenerator.h) creates deterministic integer-key datasets:

- **Sequential**: consecutive keys starting from a configurable value.
- **Random**: unique keys sampled with a fixed seed.
- **Clustered**: unique keys selected from separated key regions, shuffled with a fixed seed.

The benchmark currently uses fixed seeds `2026` for Random and `3030` for Clustered workloads.

## Benchmark System

[`BenchmarkRunner`](src/benchmark/BenchmarkRunner.h) measures individual operations with `std::chrono::steady_clock`:

- Insert
- PointLookup
- RangeQuery
- Delete

It also exposes a generic callable timing method used for PGMIndex BulkLoad. BenchmarkRunner does not know about AdaptiveIndex or PGMIndex-specific APIs.

[`BenchmarkReporter`](src/benchmark/BenchmarkReporter.h) writes CSV records with this header:

```text
DatasetSize,Index,Dataset,Operation,OperationCount,TotalNanoseconds,AverageNanoseconds
```

The benchmark executable is [`BenchmarkMain.cpp`](src/benchmark/BenchmarkMain.cpp). It benchmarks:

- HashIndex: Insert, PointLookup, RangeQuery, Delete
- BPlusTree: Insert, PointLookup, RangeQuery, Delete
- PGMIndex: Insert, PointLookup, RangeQuery, Delete, BulkLoad
- AdaptiveIndex: Insert, PointLookup, RangeQuery, Delete

BulkLoad is a separate PGMIndex-only operation and does not replace normal Insert.

## Controlled Experiment

Run the controlled experiment with:

```powershell
.\build\AdaptiveCoreBenchmark.exe --experiment --output experiment_results.csv
```

Dataset sizes:

```text
100
1000
5000
10000
```

Workloads:

```text
Sequential
Random
Clustered
```

Expected records:

```text
3 workloads * (4 indexes * 4 operations + 1 PGM BulkLoad) * 4 dataset sizes = 204 records
```

The extra operation is BulkLoad, which exists only for PGMIndex.

The experiment prints progress for each dataset size and workload and reports total orchestration wall-clock time. Large runs can be slow because normal PGM insertion and deletion still involve expensive sorted-vector/model work.

### Final Adaptation Effectiveness Evaluation

Run the continuous changing-workload evaluation with:

```powershell
.\build\AdaptiveCoreBenchmark.exe --changing-workload --output changing_workload_results.csv
python scripts/analyze_changing_workload.py --input changing_workload_results.csv
```

This is the direct comparison between fixed `BPlusTree`, fixed `PGMIndex`, and
`AdaptiveIndex`. All three replay the same deterministic request stream; no
construction or initial bulk-load time is included in request latency.

The stream has five contiguous 256-operation phases by default: `Uniform`,
`HighlySkewed`, `Sequential`, `Mixed`, and `UniformAgain`. The labels are
experiment metadata only: `WorkloadAnalyzer` receives operations and observed
keys, never phase labels.

The CSV preserves the earlier phase-summary and switch-event columns and adds
adaptation metrics. A phase change is the first operation after a phase's
`PhaseBeginOperation`. Detection is the first completed AdaptiveIndex analysis
window strictly after that boundary; `DetectionDelayOperations` is the distance
between those operation numbers. It is intentionally an operation-count metric,
not a wall-clock claim.

Each `Switch` row includes the old/new index, rebuild duration, and averages
over 32 requests immediately before and immediately after the triggering
request (the triggering request is excluded from both windows). The phase
summary latency includes switching work because it is a real cost paid while
serving the stream; one-time initial construction is excluded.

`UpdateThroughput` rows report insert and delete throughput as
`OperationCount / elapsed_seconds`, derived from the timed requests for each
phase and system. Adaptive-to-baseline percentage fields are latency
differences, so a positive value means AdaptiveIndex was slower.

A switch is classified as potentially false when either (a) the deterministic
policy choice for the following 32-request evaluation window differs from the
selected index, or (b) it immediately reverses to the old index within that
window with no intervening phase boundary. `FalseSwitchRate` is false switches
divided by evaluated switches. This is a small deterministic diagnostic, not a
statistical proof of causality.

The analysis script writes these research graphs under `results/graphs/`:

- `changing_workload_phase_latency.png`
- `adaptive_switch_timeline.png`
- `adaptive_pre_post_switch_latency.png`
- `changing_workload_update_throughput.png`
- `adaptive_relative_to_baselines.png`

The experiment is a single deterministic in-process run. Nanosecond timings
can vary across machines and runs, and the results do not establish that the
adaptive approach is universally better. In particular, rebuild costs, the
simple policy, and the educational PGM implementation limit any broader claim.

## Building

Requirements:

- C++17 compiler
- CMake 3.20 or newer
- Ninja

Configure and build:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Testing

Run all registered tests:

```powershell
ctest --test-dir build --output-on-failure
```

The current test targets cover:

- Core HashIndex and BPlusTree behavior.
- PGMIndex model construction, search, mutation, range queries, bulk loading, and deferred rebuilding.
- Dataset sizes, uniqueness, distributions, and reproducibility.
- Benchmark timing results and custom operations.
- CSV reporting.
- AdaptiveIndex workload analysis, policy decisions, switching, cooldown behavior, and data correctness.

## Running Benchmarks

Default dataset size:

```powershell
.\build\AdaptiveCoreBenchmark.exe
```

Explicit dataset size:

```powershell
.\build\AdaptiveCoreBenchmark.exe 32
```

CSV output:

```powershell
.\build\AdaptiveCoreBenchmark.exe 32 --output benchmark_results.csv
```

The executable continues to print results to the console. CSV output is optional and is created or truncated when requested.

### PGM Epsilon Experiment

Run the controlled PGM error-bound experiment with:

```powershell
.\build\AdaptiveCoreBenchmark.exe --pgm-epsilon --output pgm_epsilon_results.csv
```

It evaluates epsilon values `1, 2, 4, 8, 16, 32, 64` on dataset sizes
`1000, 5000, 10000` using deterministic Sequential and Random datasets. Each
configuration uses the same bulk-load input and point-lookup workload for its
comparison. The CSV reports bulk-load and point-lookup timing, segment count,
model memory estimate, average prediction error, and maximum prediction error.
The prediction errors are calculated from the stored PGM segments and their
actual dataset positions. No memory metric beyond the segment-model estimate
is claimed.

### Memory Footprint Experiment

Run the memory experiment with:

```powershell
.\build\AdaptiveCoreBenchmark.exe --memory --output memory_results.csv
```

The experiment compares HashIndex, BPlusTree, PGMIndex, and AdaptiveIndex on
deterministic Sequential and Random datasets of sizes `1000`, `5000`, `10000`,
and `50000`. Each index receives the same keys for a given dataset. HashIndex,
BPlusTree, and AdaptiveIndex are populated with inserts; PGMIndex is populated
with one bulk load because that is its intended construction path.

Memory is measured with a scoped allocation tracker around index population.
`TrackedAllocationBytes` is the live dynamically allocated bytes remaining
after population, while `PeakTrackedAllocationBytes` includes temporary
allocations during construction. `TotalFootprintBytes` adds the concrete index
object size to the live tracked allocation count. `LogicalPayloadBytes` is a
separate estimate of key bytes plus value characters and is not subtracted from
the allocation totals. The tracker excludes allocations made before the scope,
including generated datasets and CSV records.

The tracker uses a fixed in-process pointer registry and measures allocations
made through the benchmark's C++ allocation path during the scoped population.
It is intentionally not a process working-set probe; allocator bookkeeping and
allocations made through unrelated APIs are outside the reported fields.

This is an allocation-footprint comparison, not an exact physical-RAM or
process-working-set measurement. Standard-library allocator metadata and
implementation details are not reported separately, and the experiment does
not claim an exact structure-versus-payload split for containers whose internal
allocations are private. PGM bulk load can have a substantially higher peak
than its steady-state allocation because of temporary construction vectors.

## Benchmark Analysis

The reproducible Python analysis script reads `experiment_results.csv` and writes five PNG graphs:

```powershell
python scripts/analyze_results.py
```

Required Python packages:

- `pandas`
- `matplotlib`

Generated files:

- `results/graphs/point_lookup_performance.png`
- `results/graphs/range_query_performance.png`
- `results/graphs/insert_performance.png`
- `results/graphs/delete_performance.png`
- `results/graphs/pgm_bulk_load_performance.png`

The graphs compare measured `AverageNanoseconds` values by dataset size and workload. They are descriptive benchmark outputs; they do not establish that one index is universally faster than the others.

## Project Structure

```text
AdaptiveCore/
  CMakeLists.txt
  README.md
  src/
    adaptive/
      AdaptiveIndex.*
      AdaptivePolicy.*
      IndexChoice.h
      WorkloadAnalyzer.*
    benchmark/
      BenchmarkMain.cpp
      BenchmarkReporter.*
      BenchmarkRunner.*
      DatasetGenerator.*
      PgmEpsilonExperiment.*
      MemoryFootprintExperiment.*
    index/
      Index.h
      HashIndex.*
      BPlusTree.*
      PGMIndex.*
    storage/
      StorageEngine.*
    main.cpp
  tests/
    AdaptiveIndexTests.cpp
    BenchmarkReporterTests.cpp
    BenchmarkRunnerTests.cpp
    DatasetGeneratorTests.cpp
    IndexTests.cpp
    PGMIndexTests.cpp
    PgmEpsilonExperimentTests.cpp
    MemoryFootprintExperimentTests.cpp
  scripts/
    analyze_results.py
  results/
    graphs/
```

`StorageEngine` is currently a small HashIndex-backed storage wrapper used by the basic executable smoke test. It is not the backend used by the benchmark or AdaptiveIndex.

Generated build files, executables, and benchmark result CSV files are ignored by Git. PNG graphs under `results/graphs/` are generated analysis artifacts.
