#include "ChangingWorkloadExperiment.h"

#include "../index/BPlusTree.h"
#include "../index/PGMIndex.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>

namespace {

const std::array<const char*, 5> phase_names{
    "Uniform", "HighlySkewed", "Sequential", "Mixed", "UniformAgain"};

int random_key(std::mt19937& generator, int key_space) {
    std::uniform_int_distribution<int> distribution(0, key_space - 1);
    return distribution(generator);
}

OperationType choose_operation(std::mt19937& generator, std::size_t phase,
                               std::size_t offset) {
    if (phase == 2) {
        std::uniform_int_distribution<int> distribution(0, 99);
        const auto choice = distribution(generator);
        if (choice < 60) {
            return OperationType::PointLookup;
        }
        if (choice < 75) {
            return OperationType::Insert;
        }
        if (choice < 90) {
            return OperationType::Delete;
        }
        return OperationType::RangeQuery;
    }

    if (phase == 3) {
        return static_cast<OperationType>(offset % 4);
    }

    if (phase == 4) {
        std::uniform_int_distribution<int> distribution(0, 99);
        const auto choice = distribution(generator);
        if (choice < 40) {
            return OperationType::PointLookup;
        }
        if (choice < 65) {
            return OperationType::Insert;
        }
        if (choice < 85) {
            return OperationType::Delete;
        }
        return OperationType::RangeQuery;
    }

    std::uniform_int_distribution<int> distribution(0, 3);
    return static_cast<OperationType>(distribution(generator));
}

int choose_key(std::mt19937& generator, std::size_t phase,
               std::size_t offset, int key_space) {
    if (phase == 2) {
        std::uniform_int_distribution<int> distribution(0, 99);
        if (distribution(generator) < 80) {
            const auto hot_space = std::max(1, key_space / 20);
            return random_key(generator, hot_space);
        }
        return random_key(generator, key_space);
    }

    if (phase == 3) {
        return static_cast<int>(offset % static_cast<std::size_t>(key_space));
    }

    if (phase == 4 && offset % 3 == 0) {
        return static_cast<int>(offset % static_cast<std::size_t>(key_space));
    }

    if (phase == 4 && offset % 3 == 1) {
        const auto hot_space = std::max(1, key_space / 20);
        return random_key(generator, hot_space);
    }

    return random_key(generator, key_space);
}

std::size_t execute_request(Index& index, const ChangingWorkloadRequest& request) {
    switch (request.operation) {
    case OperationType::PointLookup:
        return index.find(request.key).has_value() ? 1 : 0;
    case OperationType::Insert:
        index.insert(request.key, request.value);
        return 1;
    case OperationType::Delete:
        return index.erase(request.key) ? 1 : 0;
    case OperationType::RangeQuery:
        return index.range(request.key, request.upper_key).size();
    }
    return 0;
}

struct PhaseTiming {
    std::size_t operation_count;
    std::int64_t total_nanoseconds;
};

std::vector<WorkloadSnapshot> analyze_phases(
    const std::vector<ChangingWorkloadRequest>& requests,
    const std::vector<ChangingWorkloadPhase>& phases) {
    std::vector<WorkloadSnapshot> snapshots;
    snapshots.reserve(phases.size());
    for (const auto& phase : phases) {
        WorkloadAnalyzer analyzer(std::max<std::size_t>(1, phase.end_operation -
                                                             phase.begin_operation));
        for (std::size_t operation = phase.begin_operation;
             operation < phase.end_operation; ++operation) {
            analyzer.record(requests[operation].operation);
        }
        snapshots.push_back(analyzer.snapshot());
    }
    return snapshots;
}

std::vector<PhaseTiming> replay(Index& index,
                                const std::vector<ChangingWorkloadRequest>& requests,
                                const std::vector<ChangingWorkloadPhase>& phases,
                                std::vector<IndexChoice>* active_choices = nullptr) {
    std::vector<PhaseTiming> timings;
    timings.reserve(phases.size());
    volatile std::size_t observation = 0;
    for (const auto& phase : phases) {
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t operation = phase.begin_operation;
             operation < phase.end_operation; ++operation) {
            observation += execute_request(index, requests[operation]);
        }
        const auto finished = std::chrono::steady_clock::now();
        timings.push_back(
            {phase.end_operation - phase.begin_operation,
             std::chrono::duration_cast<std::chrono::nanoseconds>(
                 finished - started)
                 .count()});
        if (active_choices != nullptr) {
            const auto* adaptive_index = dynamic_cast<const AdaptiveIndex*>(&index);
            active_choices->push_back(adaptive_index->current_choice());
        }
    }
    (void)observation;
    return timings;
}

ChangingWorkloadRecord make_summary(const ChangingWorkloadPhase& phase,
                                    const std::string& system,
                                    const std::string& active_index,
                                    const PhaseTiming& timing,
                                    std::size_t switch_count,
                                    const WorkloadSnapshot& workload) {
    const auto average = timing.operation_count == 0
                              ? 0
                              : timing.total_nanoseconds /
                                    static_cast<std::int64_t>(
                                        timing.operation_count);
    return {"Summary", phase.number, phase.name, phase.begin_operation,
            phase.end_operation, system, active_index,
            timing.operation_count, timing.total_nanoseconds, average,
            switch_count, 0, "", "", 0, workload.insert_ratio,
            workload.point_lookup_ratio, workload.range_query_ratio,
            workload.delete_ratio, 0};
}

}  // namespace

std::vector<ChangingWorkloadPhase> ChangingWorkloadExperiment::make_phases(
    const ChangingWorkloadConfig& config) {
    std::vector<ChangingWorkloadPhase> phases;
    phases.reserve(config.phase_lengths.size());
    std::size_t begin = 0;
    for (std::size_t index = 0; index < config.phase_lengths.size(); ++index) {
        const auto end = begin + config.phase_lengths[index];
        phases.push_back({index + 1, phase_names[index], begin, end});
        begin = end;
    }
    return phases;
}

std::vector<ChangingWorkloadRequest> ChangingWorkloadExperiment::generate_requests(
    const ChangingWorkloadConfig& config) {
    if (config.key_space <= 0) {
        throw std::invalid_argument("key_space must be greater than zero");
    }

    const auto phases = make_phases(config);
    std::mt19937 generator(config.seed);
    std::vector<ChangingWorkloadRequest> requests;
    requests.reserve(phases.back().end_operation);
    for (const auto& phase : phases) {
        for (std::size_t operation = phase.begin_operation;
             operation < phase.end_operation; ++operation) {
            const auto offset = operation - phase.begin_operation;
            const auto type = choose_operation(generator, phase.number, offset);
            const auto key = choose_key(generator, phase.number, offset,
                                        config.key_space);
            requests.push_back({type, key, std::min(config.key_space - 1, key + 5),
                                "value-" + std::to_string(operation)});
        }
    }
    return requests;
}

std::vector<ChangingWorkloadRecord> ChangingWorkloadExperiment::run(
    const ChangingWorkloadConfig& config) {
    const auto phases = make_phases(config);
    const auto requests = generate_requests(config);
    const auto workload_snapshots = analyze_phases(requests, phases);
    std::vector<ChangingWorkloadRecord> records;

    BPlusTree bplus_tree;
    const auto bplus_timings = replay(bplus_tree, requests, phases);
    for (std::size_t index = 0; index < phases.size(); ++index) {
        records.push_back(make_summary(phases[index], "BPlusTree", "BPlusTree",
                                       bplus_timings[index], 0,
                                       workload_snapshots[index]));
    }

    PGMIndex pgm_index;
    const auto pgm_timings = replay(pgm_index, requests, phases);
    for (std::size_t index = 0; index < phases.size(); ++index) {
        records.push_back(
            make_summary(phases[index], "PGMIndex", "PGMIndex",
                         pgm_timings[index], 0, workload_snapshots[index]));
    }

    AdaptiveIndex adaptive_index;
    std::vector<IndexChoice> adaptive_choices;
    const auto adaptive_timings =
        replay(adaptive_index, requests, phases, &adaptive_choices);
    const auto events = adaptive_index.switch_events();
    for (std::size_t index = 0; index < phases.size(); ++index) {
        std::size_t phase_switches = 0;
        for (const auto& event : events) {
            if (event.operation_number > phases[index].begin_operation &&
                event.operation_number <= phases[index].end_operation) {
                ++phase_switches;
            }
        }
        records.push_back(make_summary(
            phases[index], "AdaptiveIndex", to_string(adaptive_choices[index]),
            adaptive_timings[index], phase_switches,
            workload_snapshots[index]));
    }

    for (const auto& event : events) {
        const auto phase = std::find_if(
            phases.begin(), phases.end(), [&event](const auto& candidate) {
                return event.operation_number > candidate.begin_operation &&
                       event.operation_number <= candidate.end_operation;
            });
        if (phase == phases.end()) {
            continue;
        }
        records.push_back({"Switch", phase->number, phase->name,
                   phase->begin_operation, phase->end_operation,
                   "AdaptiveIndex", to_string(event.new_choice),
                   event.operation_number, 0, 0, 0,
                           event.operation_number, to_string(event.old_choice),
                           to_string(event.new_choice), event.duration.count(),
                           0.0, 0.0, 0.0, 0.0, event.window_number});
    }
    return records;
}

void ChangingWorkloadExperiment::write_csv(
    const std::string& output_path,
    const std::vector<ChangingWorkloadRecord>& records) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Unable to open changing workload output file: " +
                                 output_path);
    }

    output << "RecordType,Phase,PhaseName,PhaseBeginOperation,PhaseEndOperation,"
              "System,ActiveIndex,OperationCount,"
              "TotalNanoseconds,AverageNanoseconds,SwitchCount,"
              "SwitchOperationNumber,SwitchFrom,SwitchTo,"
              "SwitchDurationNanoseconds,InsertRatio,PointLookupRatio,"
              "RangeQueryRatio,DeleteRatio,SwitchWindowNumber\n";
    for (const auto& record : records) {
        output << record.record_type << ',' << record.phase << ','
               << record.phase_name << ',' << record.phase_begin_operation << ','
               << record.phase_end_operation << ',' << record.system << ','
               << record.active_index << ',' << record.operation_count << ','
               << record.total_nanoseconds << ',' << record.average_nanoseconds
               << ',' << record.switch_count << ','
               << record.switch_operation_number << ',' << record.switch_from
               << ',' << record.switch_to << ','
               << record.switch_duration_nanoseconds << ','
               << record.insert_ratio << ',' << record.point_lookup_ratio << ','
               << record.range_query_ratio << ',' << record.delete_ratio << ','
               << record.switch_window_number << '\n';
    }
    if (!output) {
        throw std::runtime_error(
            "Unable to write changing workload output file: " + output_path);
    }
}
