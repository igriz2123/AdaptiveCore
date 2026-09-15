cmake_minimum_required(VERSION 3.20)

project(AdaptiveCore)

set(CMAKE_CXX_STANDARD 17)

add_executable(AdaptiveCore
    src/main.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/index/PGMIndex.cpp
    src/storage/StorageEngine.cpp
)

enable_testing()

add_executable(AdaptiveCoreTests
    tests/IndexTests.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/storage/StorageEngine.cpp
)

add_test(NAME AdaptiveCoreTests COMMAND AdaptiveCoreTests)

add_executable(DatasetGeneratorTests
    tests/DatasetGeneratorTests.cpp
    src/benchmark/DatasetGenerator.cpp
)

add_test(NAME DatasetGeneratorTests COMMAND DatasetGeneratorTests)

add_executable(BenchmarkRunnerTests
    tests/BenchmarkRunnerTests.cpp
    src/adaptive/AdaptiveIndex.cpp
    src/adaptive/AdaptivePolicy.cpp
    src/adaptive/WorkloadAnalyzer.cpp
    src/benchmark/BenchmarkRunner.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME BenchmarkRunnerTests COMMAND BenchmarkRunnerTests)

add_executable(BenchmarkReporterTests
    tests/BenchmarkReporterTests.cpp
    src/benchmark/BenchmarkReporter.cpp
    src/benchmark/BenchmarkRunner.cpp
)

add_test(NAME BenchmarkReporterTests COMMAND BenchmarkReporterTests)

add_executable(PGMIndexTests
    tests/PGMIndexTests.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME PGMIndexTests COMMAND PGMIndexTests)

add_executable(PgmEpsilonExperimentTests
    tests/PgmEpsilonExperimentTests.cpp
    src/benchmark/BenchmarkRunner.cpp
    src/benchmark/DatasetGenerator.cpp
    src/benchmark/PgmEpsilonExperiment.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME PgmEpsilonExperimentTests COMMAND PgmEpsilonExperimentTests)

add_executable(AdaptiveIndexTests
    tests/AdaptiveIndexTests.cpp
    src/adaptive/AdaptiveIndex.cpp
    src/adaptive/AdaptivePolicy.cpp
    src/adaptive/WorkloadAnalyzer.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME AdaptiveIndexTests COMMAND AdaptiveIndexTests)

add_executable(ChangingWorkloadExperimentTests
    tests/ChangingWorkloadExperimentTests.cpp
    src/adaptive/AdaptiveIndex.cpp
    src/adaptive/AdaptivePolicy.cpp
    src/adaptive/WorkloadAnalyzer.cpp
    src/benchmark/ChangingWorkloadExperiment.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME ChangingWorkloadExperimentTests
         COMMAND ChangingWorkloadExperimentTests)

add_executable(AdaptiveCoreBenchmark
    src/benchmark/BenchmarkMain.cpp
    src/adaptive/AdaptiveIndex.cpp
    src/adaptive/AdaptivePolicy.cpp
    src/adaptive/WorkloadAnalyzer.cpp
    src/benchmark/BenchmarkReporter.cpp
    src/benchmark/BenchmarkRunner.cpp
    src/benchmark/ChangingWorkloadExperiment.cpp
    src/benchmark/DatasetGenerator.cpp
    src/benchmark/PgmEpsilonExperiment.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
    src/index/PGMIndex.cpp
)