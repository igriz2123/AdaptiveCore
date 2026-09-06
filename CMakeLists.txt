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
    src/benchmark/BenchmarkRunner.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
)

add_test(NAME BenchmarkRunnerTests COMMAND BenchmarkRunnerTests)

add_executable(PGMIndexTests
    tests/PGMIndexTests.cpp
    src/index/PGMIndex.cpp
)

add_test(NAME PGMIndexTests COMMAND PGMIndexTests)

add_executable(AdaptiveCoreBenchmark
    src/benchmark/BenchmarkMain.cpp
    src/benchmark/BenchmarkRunner.cpp
    src/benchmark/DatasetGenerator.cpp
    src/index/BPlusTree.cpp
    src/index/HashIndex.cpp
)