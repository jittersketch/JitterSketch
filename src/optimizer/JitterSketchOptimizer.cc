#include "JitterSketchOptimizer.hh"
#include "utils/INIReader.h"
#include <stdexcept>
#include <limits>
#include <iostream>

JitterSketchOptimizer::JitterSketchOptimizer() : B_(0) {}

void JitterSketchOptimizer::configure(std::shared_ptr<INIReader> config) {
    if (!config) {
        throw std::runtime_error("Configuration is null for JitterSketchOptimizer");
    }
    B_ = config->GetInteger("DJSketchOptimizer", "B_size", 10);
    double jitter_factor = config->GetReal("DJSketchOptimizer", "jitter_factor", 4.0);
    uint64_t min_absolute_jitter_thres = config->GetInteger("DJSketchOptimizer", "min_absolute_jitter_thres", 5000);
    uint64_t max_ifpd_diff = config->GetInteger("DJSketchOptimizer", "max_ifpd_diff", 1000000);
    long mem_size = config->GetInteger("DJSketchOptimizer", "mem_size", 100000);
    int jitter_detection_mode = config->GetInteger("general", "jitter_detection_mode", 2);
    int frequency_threshold = config->GetInteger("general", "frequency_threshold", 30);


    double s1_ratio = config->GetReal("DJSketchOptimizer", "stage_one_ratio", 0.2);
    double s2_ratio = config->GetReal("DJSketchOptimizer", "stage_two_ratio", 0.4);
    int d3 = config->GetInteger("DJSketchOptimizer", "d3", 4);

    size_t s1_bucket_size = sizeof(sketch::JitterSketchStageOneBucket);
    size_t s2_bucket_size = sizeof(sketch::JitterSketchStageTwoBucket);
    size_t s3_entry_size = sizeof(sketch::JitterSketchStageThreeEntry);

    size_t s1_mem_bytes = static_cast<size_t>(mem_size * s1_ratio);
    int w1 = s1_bucket_size > 0 ? s1_mem_bytes / s1_bucket_size : 0;

    size_t s2_mem_bytes = static_cast<size_t>(mem_size * s2_ratio);
    int w2 = s2_bucket_size > 0 ? s2_mem_bytes / s2_bucket_size : 0;

    long s3_mem_bytes = mem_size - s1_mem_bytes - s2_mem_bytes;
    int w3 = 0;
    if (d3 > 0 && s3_mem_bytes > 0 && s3_entry_size > 0) {
        w3 = s3_mem_bytes / (d3 * s3_entry_size);
    }
    dj_sketch_ = std::make_unique<sketch::JitterSketch<hash::AwareHash>>(w1, w2, w3, d3, jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold);
}

std::string JitterSketchOptimizer::name() const {
    return "JitterSketch Opt";
}

void JitterSketchOptimizer::processPacket(const FlowKey<13>& flowkey, uint64_t timestamp) {
    if (dj_sketch_) {
        dj_sketch_->update(flowkey, timestamp);
        const auto& abnormal_events = dj_sketch_->getAbnormalEvents();
        if (!abnormal_events.empty()) {
            const auto& last_event = abnormal_events.back();
            if (std::get<0>(last_event) == flowkey) {
                jittered_flows_.insert(flowkey);
            }
        }
    }
}

bool JitterSketchOptimizer::hasJitter(const FlowKey<13>& flowkey) {
    return jittered_flows_.count(flowkey) > 0;
}

void JitterSketchOptimizer::clearJitteredFlows() {
    jittered_flows_.clear();
}

std::vector<uint64_t> JitterSketchOptimizer::optimize(const std::vector<uint64_t>& arrival_times) {
    size_t n = arrival_times.size();

    if (n <= static_cast<size_t>(2 * B_)) {
        return arrival_times;
    }

    std::vector<uint64_t> release_times(n);
    double X_a = static_cast<double>(arrival_times.back() - arrival_times.front()) / (n - 1);
    uint64_t a_B = arrival_times[B_];

    for (size_t k = 0; k < n; ++k) {
        uint64_t s_on_star_k = a_B + static_cast<uint64_t>((double)k * X_a);
        uint64_t a_k = arrival_times[k];
        uint64_t a_k_plus_2B = (k + 2 * B_ < n) ? arrival_times[k + 2 * B_] : std::numeric_limits<uint64_t>::max();

        uint64_t s_on_k = s_on_star_k;
        if (s_on_k < a_k) {
            s_on_k = a_k;
        }
        if (s_on_k > a_k_plus_2B) {
            s_on_k = a_k_plus_2B;
        }
        release_times[k] = s_on_k;
    }
    return release_times;
}