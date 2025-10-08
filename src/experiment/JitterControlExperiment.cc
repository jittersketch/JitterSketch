#include "JitterControlExperiment.hh"
#include "utils/INIReader.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include "utils/flowkey.hh"
#include <vector>
#include "optimizer/JitterSketchOptimizer.hh"

struct BufferSlot {
    FlowKey<13> flowkey;
    uint64_t last_arrival_time = 0;
    std::vector<uint64_t> timestamps;
    bool is_active = false;
};

JitterControlExperiment::JitterControlExperiment(const std::vector<core::Record>& records,
                                                 std::shared_ptr<JitterOptimizer> optimizer,
                                                 std::shared_ptr<INIReader> config)
        : records_(records), optimizer_(optimizer), config_(config) {
    frequency_threshold_ = config->GetInteger("general", "frequency_threshold", 30);
    max_buffers_ = config->GetInteger("JitterControlExperiment", "max_buffers", 100);
    buffer_timeout_us_ = config->GetInteger("JitterControlExperiment", "buffer_timeout_us", 2000000);
    B_size_ = config->GetInteger("JitterControlExperiment", "B_size", 10);
}

uint64_t JitterControlExperiment::calculateDelayVariation(const std::vector<uint64_t>& timestamps) {
    if (timestamps.size() < 2) return 0;
    double X_a = static_cast<double>(timestamps.back() - timestamps.front()) / (timestamps.size() - 1);
    double max_variation = 0.0;
    for (size_t i = 0; i < timestamps.size(); ++i) {
        for (size_t k = 0; k < timestamps.size(); ++k) {
            double current_variation = std::abs(
                    static_cast<double>(timestamps[i]) - static_cast<double>(timestamps[k]) - (static_cast<double>(i) - static_cast<double>(k)) * X_a
            );
            if (current_variation > max_variation) {
                max_variation = current_variation;
            }
        }
    }
    return static_cast<uint64_t>(max_variation);
}

int JitterControlExperiment::countVariationFlows(const std::map<FlowKey<13>, std::vector<uint64_t>>& all_timestamps) {
    int variation_flow_count = 0;
    for (auto const& [flowkey, timestamps_const] : all_timestamps) {
        if (timestamps_const.size() >= static_cast<size_t>(frequency_threshold_)) {
            std::vector<uint64_t> timestamps = timestamps_const;
            std::sort(timestamps.begin(), timestamps.end());
            if (calculateDelayVariation(timestamps) > 0) {
                variation_flow_count++;
            }
        }
    }
    return variation_flow_count;
}


void JitterControlExperiment::run() {
    std::cout << "--- Jitter Optimization Experiment---" << std::endl;
    std::cout << "Using Optimizer: " << optimizer_->name() << std::endl;

    auto sketch_optimizer = std::dynamic_pointer_cast<JitterSketchOptimizer>(optimizer_);
    if (sketch_optimizer) {
        std::cout << "Optimizer is JitterSketch-aware. Jitter detection is active." << std::endl;
    }

    std::vector<core::Record> sorted_records = records_;
    std::sort(sorted_records.begin(), sorted_records.end(), [](const auto& a, const auto& b) {
        return a.timestamp_ < b.timestamp_;
    });

    std::vector<BufferSlot> buffer_pool(max_buffers_);
    std::map<FlowKey<13>, int> flow_to_buffer_map;

    std::map<FlowKey<13>, std::vector<uint64_t>> all_optimized_timestamps;

    for (const auto& record : sorted_records) {
        uint64_t current_timestamp = record.timestamp_;

        if (sketch_optimizer) {
            sketch_optimizer->processPacket(record.flowkey_, current_timestamp);
        }

        for (int i = 0; i < max_buffers_; ++i) {
            if (buffer_pool[i].is_active && (current_timestamp - buffer_pool[i].last_arrival_time > buffer_timeout_us_)) {
                if (!buffer_pool[i].timestamps.empty()) {
                    std::vector<uint64_t> optimized_chunk = optimizer_->optimize(buffer_pool[i].timestamps);
                    all_optimized_timestamps[buffer_pool[i].flowkey].insert(
                            all_optimized_timestamps[buffer_pool[i].flowkey].end(),
                            optimized_chunk.begin(),
                            optimized_chunk.end()
                    );
                }
                flow_to_buffer_map.erase(buffer_pool[i].flowkey);
                buffer_pool[i] = BufferSlot();
            }
        }

        auto it = flow_to_buffer_map.find(record.flowkey_);
        if (it != flow_to_buffer_map.end()) {
            int buffer_idx = it->second;
            buffer_pool[buffer_idx].timestamps.push_back(current_timestamp);
            buffer_pool[buffer_idx].last_arrival_time = current_timestamp;
        } else {
            bool allocate = false;
            if (sketch_optimizer) {
                if (sketch_optimizer->hasJitter(record.flowkey_)) {
                    allocate = true;
                }
            } else {
                allocate = true;
            }

            if (allocate) {
                int free_idx = -1;
                for (int i = 0; i < max_buffers_; ++i) {
                    if (!buffer_pool[i].is_active) {
                        free_idx = i;
                        break;
                    }
                }
                if (free_idx != -1) {
                    flow_to_buffer_map[record.flowkey_] = free_idx;
                    buffer_pool[free_idx].is_active = true;
                    buffer_pool[free_idx].flowkey = record.flowkey_;
                    buffer_pool[free_idx].timestamps.push_back(current_timestamp);
                    buffer_pool[free_idx].last_arrival_time = current_timestamp;
                }
            }
        }
    }

    for (int i = 0; i < max_buffers_; ++i) {
        if (buffer_pool[i].is_active && !buffer_pool[i].timestamps.empty()) {
            std::vector<uint64_t> optimized_chunk = optimizer_->optimize(buffer_pool[i].timestamps);
            all_optimized_timestamps[buffer_pool[i].flowkey].insert(
                    all_optimized_timestamps[buffer_pool[i].flowkey].end(),
                    optimized_chunk.begin(),
                    optimized_chunk.end()
            );
        }
    }

    std::map<FlowKey<13>, std::vector<uint64_t>> all_original_timestamps;
    for (const auto& record : records_) {
        all_original_timestamps[record.flowkey_].push_back(record.timestamp_);
    }

    uint64_t total_original_variation = 0;
    uint64_t total_optimized_variation = 0;
    int frequent_flow_count = 0;

    std::map<FlowKey<13>, std::vector<uint64_t>> all_optimized_timestamps_for_analysis = all_original_timestamps;
    for(auto const& [flowkey, optimized_stamps] : all_optimized_timestamps){
        if(!optimized_stamps.empty()){
            all_optimized_timestamps_for_analysis[flowkey] = optimized_stamps;
        }
    }


    for (auto const& [flowkey, original_timestamps_const] : all_original_timestamps) {
        if (original_timestamps_const.size() >= static_cast<size_t>(frequency_threshold_)) {
            frequent_flow_count++;

            std::vector<uint64_t> original_timestamps = original_timestamps_const;
            std::sort(original_timestamps.begin(), original_timestamps.end());

            total_original_variation += calculateDelayVariation(original_timestamps);

            auto it = all_optimized_timestamps.find(flowkey);
            if (it != all_optimized_timestamps.end() && !it->second.empty()) {
                std::sort(it->second.begin(), it->second.end());
                total_optimized_variation += calculateDelayVariation(it->second);
            } else {
                total_optimized_variation += calculateDelayVariation(original_timestamps);
            }
        }
    }

    int original_variation_flows = countVariationFlows(all_original_timestamps);
    int optimized_variation_flows = countVariationFlows(all_optimized_timestamps_for_analysis);

    std::cout << "Original Delay Variations Sum : " << total_original_variation << " us (" << total_original_variation / 1000.0 << " ms)" << std::endl;
    std::cout << "Optimized Delay Variations Sum : " << total_optimized_variation << " us (" << total_optimized_variation / 1000.0 << " ms)" << std::endl;

    double jitter_reduction = 0.0;
    if (total_original_variation > 0) {
        jitter_reduction = 100.0 * (static_cast<double>(total_original_variation) - static_cast<double>(total_optimized_variation)) / static_cast<double>(total_original_variation);
        std::cout << "Jitter Reduction: " << std::fixed << std::setprecision(2) << jitter_reduction << "%" << std::endl;
    }

    std::cout << "Original Variations flows: " << original_variation_flows << std::endl;
    std::cout << "Optimized Variations flows: " << optimized_variation_flows << std::endl;
    double jitter_flow_reduction = 0.0;
    if (original_variation_flows > 0) {
        jitter_flow_reduction = 100.0 * (static_cast<double>(original_variation_flows) - static_cast<double>(optimized_variation_flows)) / static_cast<double>(original_variation_flows);
        std::cout << "Variations flow Reduction: " << std::fixed << std::setprecision(2) << jitter_flow_reduction << "%" << std::endl;
    }


    std::cout << "--- End Jitter Optimization Experiment ---" << std::endl << std::endl;
}