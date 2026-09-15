#include "ChangingWorkloadExperiment.h"

#include "../index/BPlusTree.h"
#include "../index/PGMIndex.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <numeric>
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
        return OperationType::PointLookup;
    }
    if (phase == 3) return OperationType::RangeQuery;
    if (phase == 4) {
        std::uniform_int_distribution<int> distribution(0, 99);
        const auto choice = distribution(generator);
        if (choice < 40) return OperationType::PointLookup;
        if (choice < 65) return OperationType::Insert;
        if (choice < 85) return OperationType::Delete;
        return OperationType::RangeQuery;
    }
    std::uniform_int_distribution<int> distribution(0, 3);
    return static_cast<OperationType>(distribution(generator));
}

int choose_key(std::mt19937& generator, std::size_t phase,
               std::size_t offset, int key_space) {
    if (phase == 2) {
        std::uniform_int_distribution<int> distribution(0, 99);
        if (distribution(generator) < 80) return random_key(generator, std::max(1, key_space / 20));
        return random_key(generator, key_space);
    }
    if (phase == 3) return static_cast<int>(offset % static_cast<std::size_t>(key_space));
    if (phase == 4 && offset % 3 == 0) return static_cast<int>(offset % static_cast<std::size_t>(key_space));
    if (phase == 4 && offset % 3 == 1) return random_key(generator, std::max(1, key_space / 20));
    return random_key(generator, key_space);
}

std::size_t execute_request(Index& index, const ChangingWorkloadRequest& request) {
    switch (request.operation) {
    case OperationType::PointLookup: return index.find(request.key).has_value() ? 1 : 0;
    case OperationType::Insert: index.insert(request.key, request.value); return 1;
    case OperationType::Delete: return index.erase(request.key) ? 1 : 0;
    case OperationType::RangeQuery: return index.range(request.key, request.upper_key).size();
    }
    return 0;
}

void record_request(WorkloadAnalyzer& analyzer, const ChangingWorkloadRequest& request) {
    if (request.operation == OperationType::RangeQuery) analyzer.record_range(request.key, request.upper_key);
    else analyzer.record(request.operation, request.key);
}

std::vector<WorkloadSnapshot> analyze_phases(const std::vector<ChangingWorkloadRequest>& requests,
                                             const std::vector<ChangingWorkloadPhase>& phases) {
    std::vector<WorkloadSnapshot> snapshots;
    for (const auto& phase : phases) {
        WorkloadAnalyzer analyzer(std::max<std::size_t>(1, phase.end_operation - phase.begin_operation));
        for (std::size_t operation = phase.begin_operation; operation < phase.end_operation; ++operation)
            record_request(analyzer, requests[operation]);
        snapshots.push_back(analyzer.snapshot());
    }
    return snapshots;
}

struct ReplayResult {
    std::vector<std::int64_t> operation_nanoseconds;
    std::vector<std::int64_t> phase_nanoseconds;
    std::vector<IndexChoice> active_choices;
};

ReplayResult replay(Index& index, const std::vector<ChangingWorkloadRequest>& requests,
                    const std::vector<ChangingWorkloadPhase>& phases,
                    bool capture_choices = false) {
    ReplayResult result;
    result.operation_nanoseconds.reserve(requests.size());
    volatile std::size_t observation = 0;
    for (const auto& phase : phases) {
        std::int64_t phase_total = 0;
        for (std::size_t operation = phase.begin_operation; operation < phase.end_operation; ++operation) {
            const auto started = std::chrono::steady_clock::now();
            observation += execute_request(index, requests[operation]);
            const auto finished = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
            result.operation_nanoseconds.push_back(elapsed);
            phase_total += elapsed;
        }
        result.phase_nanoseconds.push_back(phase_total);
        if (capture_choices) result.active_choices.push_back(static_cast<const AdaptiveIndex&>(index).current_choice());
    }
    (void)observation;
    return result;
}

ChangingWorkloadRecord base_record(const ChangingWorkloadPhase& phase, const std::string& type,
                                   const std::string& system, const std::string& active_index,
                                   const WorkloadSnapshot& workload) {
    return {type, phase.number, phase.name, phase.begin_operation, phase.end_operation,
            system, active_index, 0, 0, 0, 0, 0, "", "", 0,
            workload.insert_ratio, workload.point_lookup_ratio, workload.range_query_ratio,
            workload.delete_ratio, 0, workload.observed_key_count, workload.distinct_key_count,
            workload.minimum_key, workload.maximum_key, workload.key_span, workload.key_mean,
            workload.key_variance, workload.key_monotonicity, workload.key_concentration,
            "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt};
}

double percentage_difference(std::int64_t value, std::int64_t baseline) {
    return baseline == 0 ? 0.0 : (static_cast<double>(value - baseline) * 100.0 / baseline);
}

std::optional<std::int64_t> average_window(const std::vector<std::int64_t>& timings,
                                           std::size_t begin, std::size_t end) {
    if (begin >= end || end > timings.size()) return std::nullopt;
    const auto total = std::accumulate(timings.begin() + begin, timings.begin() + end, std::int64_t{0});
    return total / static_cast<std::int64_t>(end - begin);
}

WorkloadSnapshot analyze_window(const std::vector<ChangingWorkloadRequest>& requests,
                                std::size_t begin, std::size_t end) {
    WorkloadAnalyzer analyzer(std::max<std::size_t>(1, end - begin));
    for (std::size_t operation = begin; operation < end; ++operation) record_request(analyzer, requests[operation]);
    return analyzer.snapshot();
}

bool phase_boundary_between(const std::vector<ChangingWorkloadPhase>& phases,
                            std::size_t first_operation, std::size_t second_operation) {
    return std::any_of(phases.begin() + 1, phases.end(), [&](const auto& phase) {
        return phase.begin_operation >= first_operation && phase.begin_operation < second_operation;
    });
}

struct SwitchEvaluation {
    const AdaptiveSwitchEvent* event;
    bool false_switch;
    std::optional<std::int64_t> pre_average;
    std::optional<std::int64_t> post_average;
};

std::vector<SwitchEvaluation> evaluate_switches(const std::vector<AdaptiveSwitchEvent>& events,
                                                const std::vector<ChangingWorkloadRequest>& requests,
                                                const std::vector<ChangingWorkloadPhase>& phases,
                                                const std::vector<std::int64_t>& timings,
                                                std::size_t evaluation_window) {
    std::vector<SwitchEvaluation> evaluations;
    const auto window = std::max<std::size_t>(1, evaluation_window);
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        const auto switch_index = event.operation_number - 1;
        const auto pre_begin = switch_index >= window ? switch_index - window : 0;
        const auto post_begin = event.operation_number;
        const auto post_end = std::min(requests.size(), post_begin + window);
        const auto pre = average_window(timings, pre_begin, switch_index);
        const auto post = average_window(timings, post_begin, post_end);
        bool future_disagrees = false;
        if (post_begin < post_end)
            future_disagrees = AdaptivePolicy{}.choose(analyze_window(requests, post_begin, post_end)) != event.new_choice;
        bool immediate_reversal = false;
        if (index + 1 < events.size()) {
            const auto& next = events[index + 1];
            immediate_reversal = next.new_choice == event.old_choice &&
                next.operation_number - event.operation_number <= window &&
                !phase_boundary_between(phases, event.operation_number, next.operation_number);
        }
        evaluations.push_back({&event, future_disagrees || immediate_reversal, pre, post});
    }
    return evaluations;
}

void append_update_records(std::vector<ChangingWorkloadRecord>& records,
                           const std::string& system, const std::string& active_index,
                           const std::vector<ChangingWorkloadPhase>& phases,
                           const std::vector<ChangingWorkloadRequest>& requests,
                           const std::vector<WorkloadSnapshot>& snapshots,
                           const ReplayResult& replay_result) {
    for (std::size_t phase_index = 0; phase_index < phases.size(); ++phase_index) {
        const auto& phase = phases[phase_index];
        for (const auto operation_type : {OperationType::Insert, OperationType::Delete}) {
            std::size_t count = 0;
            std::int64_t total = 0;
            for (std::size_t operation = phase.begin_operation; operation < phase.end_operation; ++operation) {
                if (requests[operation].operation == operation_type) {
                    ++count;
                    total += replay_result.operation_nanoseconds[operation];
                }
            }
            auto record = base_record(phase, "UpdateThroughput", system, active_index, snapshots[phase_index]);
            record.measured_operation = operation_type == OperationType::Insert ? "Insert" : "Delete";
            record.operation_count = count;
            record.total_nanoseconds = total;
            record.average_nanoseconds = count == 0 ? 0 : total / static_cast<std::int64_t>(count);
            if (total > 0) record.operations_per_second = static_cast<double>(count) * 1'000'000'000.0 / total;
            records.push_back(record);
        }
    }
}

template <typename T>
void write_optional(std::ofstream& output, const std::optional<T>& value) {
    if (value) output << *value;
}

}  // namespace

std::vector<ChangingWorkloadPhase> ChangingWorkloadExperiment::make_phases(const ChangingWorkloadConfig& config) {
    std::vector<ChangingWorkloadPhase> phases;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < config.phase_lengths.size(); ++index) {
        const auto end = begin + config.phase_lengths[index];
        phases.push_back({index + 1, phase_names[index], begin, end});
        begin = end;
    }
    return phases;
}

std::vector<ChangingWorkloadRequest> ChangingWorkloadExperiment::generate_requests(const ChangingWorkloadConfig& config) {
    if (config.key_space <= 0) throw std::invalid_argument("key_space must be greater than zero");
    const auto phases = make_phases(config);
    std::mt19937 generator(config.seed);
    std::vector<ChangingWorkloadRequest> requests;
    requests.reserve(phases.back().end_operation);
    for (const auto& phase : phases) {
        for (std::size_t operation = phase.begin_operation; operation < phase.end_operation; ++operation) {
            const auto offset = operation - phase.begin_operation;
            const auto type = choose_operation(generator, phase.number, offset);
            const auto key = choose_key(generator, phase.number, offset, config.key_space);
            requests.push_back({type, key, std::min(config.key_space - 1, key + 5), "value-" + std::to_string(operation)});
        }
    }
    return requests;
}

std::vector<ChangingWorkloadRecord> ChangingWorkloadExperiment::run(const ChangingWorkloadConfig& config) {
    const auto phases = make_phases(config);
    const auto requests = generate_requests(config);
    const auto snapshots = analyze_phases(requests, phases);
    std::vector<ChangingWorkloadRecord> records;

    BPlusTree bplus_tree;
    const auto bplus = replay(bplus_tree, requests, phases);
    PGMIndex pgm_index;
    const auto pgm = replay(pgm_index, requests, phases);
    AdaptiveIndex adaptive_index(config.operation_window, config.minimum_windows_between_switches);
    const auto adaptive = replay(adaptive_index, requests, phases, true);
    const auto events = adaptive_index.switch_events();
    const auto decisions = adaptive_index.decision_events();
    const auto switch_evaluations = evaluate_switches(events, requests, phases,
                                                       adaptive.operation_nanoseconds,
                                                       config.switch_evaluation_window);

    for (std::size_t index = 0; index < phases.size(); ++index) {
        const auto& phase = phases[index];
        for (const auto* system : {"BPlusTree", "PGMIndex"}) {
            const auto& result = std::string(system) == "BPlusTree" ? bplus : pgm;
            auto record = base_record(phase, "Summary", system, system, snapshots[index]);
            record.operation_count = phase.end_operation - phase.begin_operation;
            record.total_nanoseconds = result.phase_nanoseconds[index];
            record.average_nanoseconds = record.total_nanoseconds / static_cast<std::int64_t>(record.operation_count);
            records.push_back(record);
        }
        auto record = base_record(phase, "Summary", "AdaptiveIndex", to_string(adaptive.active_choices[index]), snapshots[index]);
        record.operation_count = phase.end_operation - phase.begin_operation;
        record.total_nanoseconds = adaptive.phase_nanoseconds[index];
        record.average_nanoseconds = record.total_nanoseconds / static_cast<std::int64_t>(record.operation_count);
        record.switch_count = static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const auto& event) {
            return event.operation_number > phase.begin_operation && event.operation_number <= phase.end_operation;
        }));
        record.total_switches = static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const auto& event) {
            return event.operation_number <= phase.end_operation;
        }));
        if (index > 0) {
            record.phase_change_operation = phase.begin_operation;
            const auto detection = std::find_if(decisions.begin(), decisions.end(), [&](const auto& decision) {
                return decision.operation_number > phase.begin_operation;
            });
            if (detection != decisions.end()) {
                record.detection_operation = detection->operation_number;
                record.detection_delay_operations = detection->operation_number - phase.begin_operation;
            }
        }
        const auto evaluated_end = std::count_if(switch_evaluations.begin(), switch_evaluations.end(), [&](const auto& evaluation) {
            return evaluation.event->operation_number <= phase.end_operation;
        });
        const auto false_end = std::count_if(switch_evaluations.begin(), switch_evaluations.end(), [&](const auto& evaluation) {
            return evaluation.event->operation_number <= phase.end_operation && evaluation.false_switch;
        });
        record.false_switch_count = static_cast<std::size_t>(false_end);
        record.false_switch_rate = evaluated_end == 0 ? 0.0 : static_cast<double>(false_end) / evaluated_end;
        record.adaptive_vs_bplus_percent = percentage_difference(record.average_nanoseconds, bplus.phase_nanoseconds[index] / static_cast<std::int64_t>(record.operation_count));
        record.adaptive_vs_pgm_percent = percentage_difference(record.average_nanoseconds, pgm.phase_nanoseconds[index] / static_cast<std::int64_t>(record.operation_count));
        records.push_back(record);
    }

    append_update_records(records, "BPlusTree", "BPlusTree", phases, requests, snapshots, bplus);
    append_update_records(records, "PGMIndex", "PGMIndex", phases, requests, snapshots, pgm);
    append_update_records(records, "AdaptiveIndex", "AdaptiveIndex", phases, requests, snapshots, adaptive);

    for (const auto& evaluation : switch_evaluations) {
        const auto& event = *evaluation.event;
        const auto phase = std::find_if(phases.begin(), phases.end(), [&](const auto& candidate) {
            return event.operation_number > candidate.begin_operation && event.operation_number <= candidate.end_operation;
        });
        if (phase == phases.end()) continue;
        auto record = base_record(*phase, "Switch", "AdaptiveIndex", to_string(event.new_choice), event.workload);
        record.operation_count = event.operation_number;
        record.switch_operation_number = event.operation_number;
        record.switch_from = to_string(event.old_choice);
        record.switch_to = to_string(event.new_choice);
        record.switch_duration_nanoseconds = event.duration.count();
        record.switch_window_number = event.window_number;
        record.pre_switch_average_nanoseconds = evaluation.pre_average;
        record.post_switch_average_nanoseconds = evaluation.post_average;
        if (evaluation.pre_average && *evaluation.pre_average > 0 && evaluation.post_average) {
            record.latency_change_percent = percentage_difference(*evaluation.post_average, *evaluation.pre_average);
            record.switch_overhead_percent = static_cast<double>(event.duration.count()) * 100.0 / *evaluation.pre_average;
        }
        record.false_switch = evaluation.false_switch;
        records.push_back(record);
    }
    return records;
}

void ChangingWorkloadExperiment::write_csv(const std::string& output_path,
                                           const std::vector<ChangingWorkloadRecord>& records) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) throw std::runtime_error("Unable to open changing workload output file: " + output_path);
    output << "RecordType,Phase,PhaseName,PhaseBeginOperation,PhaseEndOperation,System,ActiveIndex,OperationCount,"
              "TotalNanoseconds,AverageNanoseconds,SwitchCount,SwitchOperationNumber,SwitchFrom,SwitchTo,"
              "SwitchDurationNanoseconds,InsertRatio,PointLookupRatio,RangeQueryRatio,DeleteRatio,SwitchWindowNumber,"
              "ObservedKeyCount,DistinctKeyCount,MinimumKey,MaximumKey,KeySpan,KeyMean,KeyVariance,KeyMonotonicity,KeyConcentration,"
              "MeasuredOperation,PhaseChangeOperation,DetectionOperation,DetectionDelayOperations,PreSwitchAverageNanoseconds,"
              "PostSwitchAverageNanoseconds,LatencyChangePercent,SwitchOverheadPercent,TotalSwitches,FalseSwitchCount,"
              "FalseSwitchRate,OperationsPerSecond,AdaptiveVsBPlusPercent,AdaptiveVsPGMPercent,FalseSwitch\n";
    for (const auto& record : records) {
        output << record.record_type << ',' << record.phase << ',' << record.phase_name << ','
               << record.phase_begin_operation << ',' << record.phase_end_operation << ',' << record.system << ','
               << record.active_index << ',' << record.operation_count << ',' << record.total_nanoseconds << ','
               << record.average_nanoseconds << ',' << record.switch_count << ',' << record.switch_operation_number << ','
               << record.switch_from << ',' << record.switch_to << ',' << record.switch_duration_nanoseconds << ','
               << record.insert_ratio << ',' << record.point_lookup_ratio << ',' << record.range_query_ratio << ','
               << record.delete_ratio << ',' << record.switch_window_number << ',' << record.observed_key_count << ','
               << record.distinct_key_count << ',' << record.minimum_key << ',' << record.maximum_key << ','
               << record.key_span << ',' << record.key_mean << ',' << record.key_variance << ','
               << record.key_monotonicity << ',' << record.key_concentration << ',' << record.measured_operation << ',';
        write_optional(output, record.phase_change_operation); output << ',';
        write_optional(output, record.detection_operation); output << ',';
        write_optional(output, record.detection_delay_operations); output << ',';
        write_optional(output, record.pre_switch_average_nanoseconds); output << ',';
        write_optional(output, record.post_switch_average_nanoseconds); output << ',';
        write_optional(output, record.latency_change_percent); output << ',';
        write_optional(output, record.switch_overhead_percent); output << ',';
        write_optional(output, record.total_switches); output << ',';
        write_optional(output, record.false_switch_count); output << ',';
        write_optional(output, record.false_switch_rate); output << ',';
        write_optional(output, record.operations_per_second); output << ',';
        write_optional(output, record.adaptive_vs_bplus_percent); output << ',';
        write_optional(output, record.adaptive_vs_pgm_percent); output << ',';
        if (record.false_switch) output << (*record.false_switch ? "true" : "false");
        output << '\n';
    }
    if (!output) throw std::runtime_error("Unable to write changing workload output file: " + output_path);
}
