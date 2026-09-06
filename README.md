\# AdaptiveCore



AdaptiveCore is a C++ project that implements and benchmarks multiple index and data-structure approaches. The project currently includes a hash-based index, a B+ tree, and a PGM-inspired learned index.



The goal of the project is to provide a controlled environment for implementing these indexing approaches and comparing their behavior under different dataset sizes, workloads, and operations.



\## Problem Statement



Different indexing techniques have different performance characteristics.



For example:



\* Hash-based indexes can provide fast point lookups.

\* Tree-based indexes can support ordered operations and range queries.

\* Learned indexes attempt to model the relationship between keys and their positions.



AdaptiveCore implements multiple approaches and provides a benchmark framework for comparing them across controlled workloads.



The project is intended for experimentation and learning rather than as a production-ready database engine.



\## Features



AdaptiveCore currently provides:



\* Hash-based indexing

\* B+ tree indexing

\* PGM-inspired learned indexing

\* Insert operations

\* Point lookups

\* Range queries

\* Delete operations

\* PGM bulk loading

\* Deferred PGM model rebuilding

\* Dataset generation

\* Multiple dataset workloads

\* Benchmark execution

\* CSV benchmark reporting

\* Controlled multi-size experiments

\* Unit tests



\## Architecture



The project is organized into three main components.



\### Index Layer



The index layer contains the implementations of the supported indexing structures:



\* `HashIndex`

\* `BPlusTree`

\* `PGMIndex`



The common index interface is defined in:



```text

src/index/Index.h

```



\### Benchmark Layer



The benchmark layer is responsible for:



\* Generating datasets

\* Running benchmark operations

\* Measuring execution time

\* Running controlled experiments

\* Exporting benchmark results to CSV



\### Storage Layer



The project also contains a storage component that provides the foundation for the storage-related parts of the system.



\## Implemented Indexes



\### HashIndex



`HashIndex` is a hash-based index implementation.



It supports the operations required by the benchmark system:



\* Insert

\* Point lookup

\* Range query

\* Delete



Hash-based indexing is generally well suited for point lookups. However, range queries require additional work because hash tables do not naturally maintain key ordering.



\### BPlusTree



`BPlusTree` is a tree-based index implementation.



It supports:



\* Insert

\* Point lookup

\* Range query

\* Delete



Because keys are maintained in an ordered structure, tree-based indexes are suitable for operations that depend on key ordering, including range queries.



\### PGMIndex



`PGMIndex` is a learned-index-inspired implementation based on a model that relates keys to their approximate positions in sorted data.



It supports:



\* Insert

\* Point lookup

\* Range query

\* Delete

\* Bulk loading



The implementation maintains sorted entries and uses a model to assist lookup operations.



\## PGM Bulk Loading



`PGMIndex` supports bulk loading.



Bulk loading allows the index to be constructed from a complete dataset rather than inserting entries individually.



This operation is benchmarked separately from normal insertion.



The distinction is intentional:



\* `Insert` measures repeated individual insertions.

\* `BulkLoad` measures construction of the PGM index from an already available dataset.



`BulkLoad` is currently specific to `PGMIndex` and is not included for `HashIndex` or `BPlusTree`.



\## Deferred PGM Model Rebuilding



Initially, the PGM model was rebuilt after every insertion or deletion.



This approach was inefficient because repeated modifications could trigger repeated model reconstruction.



AdaptiveCore now uses deferred model rebuilding.



The current behavior is:



1\. `insert()` modifies the sorted entries and marks the model as dirty.

2\. A successful `erase()` marks the model as dirty.

3\. The model is not immediately rebuilt after every modification.

4\. `find()` checks whether the model is dirty.

5\. If necessary, the model is rebuilt before the lookup.

6\. After rebuilding, the model becomes clean.

7\. `range()` and `size()` do not force a model rebuild.

8\. `bulk\_load()` rebuilds the model immediately.



This reduces unnecessary rebuilding when multiple modifications occur before a lookup.



\## Benchmark System



The benchmark system measures the performance of supported operations.



The following operations are benchmarked:



\* `Insert`

\* `PointLookup`

\* `RangeQuery`

\* `Delete`



These operations are benchmarked for:



\* `HashIndex`

\* `BPlusTree`

\* `PGMIndex`



`PGMIndex` additionally supports:



\* `BulkLoad`



Bulk loading is measured separately because it represents a different workload from repeated individual insertion.



The benchmark system includes a generic custom timing mechanism for timing individual callable operations.



\## Dataset Types



AdaptiveCore supports multiple workload patterns.



\### Sequential



Keys are generated in sequential order.



This workload represents ordered data.



\### Random



Keys are generated in a random order.



This workload represents less predictable insertion and access patterns.



\### Clustered



Keys are generated in clustered patterns.



This workload is intended to represent data distributions where values are concentrated in particular regions.



\## Building the Project



\### Requirements



The project uses:



\* C++

\* CMake

\* Ninja



The project has been developed and tested using Windows PowerShell.



\### Configure



From the project directory:



```powershell

cmake -S . -B build -G Ninja -DCMAKE\_BUILD\_TYPE=Release

```



\### Build



```powershell

cmake --build build --config Release

```



\## Running Tests



Run all tests using:



```powershell

ctest --test-dir build --output-on-failure

```



The project currently includes tests for:



\* Core index functionality

\* Dataset generation

\* Benchmark execution

\* Benchmark reporting

\* PGMIndex behavior



\## Running Benchmarks



\### Default Benchmark



```powershell

.\\build\\AdaptiveCoreBenchmark.exe

```



\### Benchmark With a Specific Dataset Size



For example:



```powershell

.\\build\\AdaptiveCoreBenchmark.exe 1000

```



\### Benchmark With CSV Output



```powershell

.\\build\\AdaptiveCoreBenchmark.exe 1000 --output benchmark\_results.csv

```



\## CSV Output



Benchmark results can be exported to a CSV file.



The current format is:



```text

DatasetSize,Index,Dataset,Operation,OperationCount,TotalNanoseconds,AverageNanoseconds

```



The fields represent:



\* `DatasetSize` — Number of records used in the dataset.

\* `Index` — Index implementation being benchmarked.

\* `Dataset` — Dataset workload type.

\* `Operation` — Operation being measured.

\* `OperationCount` — Number of operations performed.

\* `TotalNanoseconds` — Total measured execution time.

\* `AverageNanoseconds` — Average execution time per operation.



Generated benchmark result files are not intended to be committed to the repository.



\## Controlled Experiment



AdaptiveCore supports a controlled benchmark experiment.



Run it using:



```powershell

.\\build\\AdaptiveCoreBenchmark.exe --experiment --output experiment\_results.csv

```



The controlled experiment currently uses the following dataset sizes:



```text

100

1000

5000

10000

```



The following workloads are tested:



```text

Sequential

Random

Clustered

```



\### Operations Per Index



`HashIndex`:



\* Insert

\* PointLookup

\* RangeQuery

\* Delete



`BPlusTree`:



\* Insert

\* PointLookup

\* RangeQuery

\* Delete



`PGMIndex`:



\* Insert

\* PointLookup

\* RangeQuery

\* Delete

\* BulkLoad



This produces:



```text

4 dataset sizes × 3 workloads × 13 benchmark records

```



Expected output:



```text

156 records

```



The additional records for `PGMIndex` are caused by its separate `BulkLoad` benchmark.



\## Project Structure



```text

AdaptiveCore/

│   .gitignore

│   CMakeLists.txt

│   README.md

│

├── data/

├── include/

├── results/

│

├── src/

│   │   main.cpp

│   │

│   ├── benchmark/

│   │   ├── BenchmarkMain.cpp

│   │   ├── BenchmarkReporter.cpp

│   │   ├── BenchmarkReporter.h

│   │   ├── BenchmarkRunner.cpp

│   │   ├── BenchmarkRunner.h

│   │   ├── DatasetGenerator.cpp

│   │   └── DatasetGenerator.h

│   │

│   ├── index/

│   │   ├── BPlusTree.cpp

│   │   ├── BPlusTree.h

│   │   ├── HashIndex.cpp

│   │   ├── HashIndex.h

│   │   ├── Index.h

│   │   ├── PGMIndex.cpp

│   │   └── PGMIndex.h

│   │

│   └── storage/

│       ├── StorageEngine.cpp

│       └── StorageEngine.h

│

└── tests/

&#x20;   ├── BenchmarkReporterTests.cpp

&#x20;   ├── BenchmarkRunnerTests.cpp

&#x20;   ├── DatasetGeneratorTests.cpp

&#x20;   ├── IndexTests.cpp

&#x20;   └── PGMIndexTests.cpp

```



\## Current Limitations



AdaptiveCore is currently an experimental and educational project.



Some limitations include:



\* The project is not a complete database management system.

\* Benchmark results can vary depending on the machine and execution environment.

\* Benchmark results should not automatically be treated as rigorous scientific conclusions without further statistical analysis.

\* The current benchmark system focuses on controlled workloads and does not represent every possible real-world workload.

\* The implemented PGM index is a project-specific implementation and should not be considered a replacement for a production-grade learned indexing library.

\* The project does not currently include persistent on-disk index storage.



\## Future Improvements



Possible future improvements include:



\* More advanced benchmark analysis

\* Statistical analysis of repeated benchmark runs

\* Graphical visualization of benchmark results

\* Additional dataset distributions

\* Additional index implementations

\* Persistent storage

\* More advanced B+ tree functionality

\* Improved learned-index modeling

\* Memory usage measurements

\* Larger-scale benchmark experiments

\* Automated experiment analysis



\## Testing Status



The project has been validated using CTest.



The latest validation completed successfully with:



```text

100% tests passed, 0 tests failed out of 5

```



The controlled benchmark experiment also completed successfully with the expected:



```text

156 benchmark records

```



\## License



No license has currently been specified for this project.



