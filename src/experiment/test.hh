#ifndef TEST_HH
#define TEST_HH

#include "utils/core.hh"
#include "detector/AbstractDetector.hh"
#include "detector/GroundTruthDetector.hh"
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <tuple>
#include <chrono>
#include <algorithm>
#include <fstream>

template <typename sketch_t>
void jitterTest(sketch_t &sketch, const std::vector<core::Record> &vec,
                double jitter_factor, uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold,
                long mem_size) {
    const int matching_mode = 0;
    const uint64_t ifpd_threshold = 500;
    const uint64_t time_threshold = 500000;

    GroundTruthDetector truth_detector(jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold);

    sketch.clear();
    sketch.setInitTime(vec[0].timestamp_);

    for (const auto &record : vec) {
        truth_detector.update(record);
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    for (const auto &record : vec) {
        sketch.update(record.flowkey_, record.timestamp_);
    }
    auto end_time = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
    double throughput = vec.size() / (elapsed_time.count() / 1000.0);

    const auto& sketch_events_raw = sketch.getAbnormalEvents();
    const auto& truth_events_raw = truth_detector.getAbnormalEvents();

    using event_details = std::tuple<uint64_t, uint64_t, uint64_t>;
    std::map<FlowKey<13>, std::vector<event_details>> sketch_events;
    for(const auto& event : sketch_events_raw) {
        sketch_events[std::get<0>(event)].emplace_back(std::get<1>(event), std::get<2>(event), std::get<3>(event));
    }

    std::map<FlowKey<13>, std::vector<event_details>> truth_events;
    for(const auto& event : truth_events_raw) {
        truth_events[std::get<0>(event)].emplace_back(std::get<1>(event), std::get<2>(event), std::get<3>(event));
    }

    auto sort_by_ts = [](const event_details& a, const event_details& b) {
        return std::get<2>(a) < std::get<2>(b);
    };
    for(auto& pair : sketch_events) std::sort(pair.second.begin(), pair.second.end(), sort_by_ts);
    for(auto& pair : truth_events) std::sort(pair.second.begin(), pair.second.end(), sort_by_ts);

    int tp_cnt = 0;
    for (auto const& [key, sketch_event_list] : sketch_events) {
        if (truth_events.count(key)) {
            auto& truth_event_list = truth_events.at(key);
            std::vector<bool> matched_truth_events(truth_event_list.size(), false);

            for (const auto& sketch_event : sketch_event_list) {
                uint64_t sketch_old_ifpd = std::get<0>(sketch_event);
                uint64_t sketch_new_ifpd = std::get<1>(sketch_event);
                uint64_t sketch_ts = std::get<2>(sketch_event);

                for (size_t i = 0; i < truth_event_list.size(); ++i) {
                    if (!matched_truth_events[i]) {
                        const auto& truth_event = truth_event_list[i];
                        uint64_t truth_old_ifpd = std::get<0>(truth_event);
                        uint64_t truth_new_ifpd = std::get<1>(truth_event);
                        uint64_t truth_ts = std::get<2>(truth_event);

                        bool time_match = std::abs((int64_t)sketch_ts - (int64_t)truth_ts) <= time_threshold;

                        if (time_match) {
                            bool match = false;
                            if (matching_mode == 0) {
                                match = true;
                            } else {
                                bool old_ifpd_match = std::abs((int64_t)sketch_old_ifpd - (int64_t)truth_old_ifpd) <= ifpd_threshold;
                                bool new_ifpd_match = std::abs((int64_t)sketch_new_ifpd - (int64_t)truth_new_ifpd) <= ifpd_threshold;
                                if (old_ifpd_match && new_ifpd_match) {
                                    match = true;
                                }
                            }

                            if (match) {
                                tp_cnt++;
                                matched_truth_events[i] = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    int total_sketch_events = sketch_events_raw.size();
    int total_truth_events = truth_events_raw.size();

    int fp_cnt = total_sketch_events - tp_cnt;
    int fn_cnt = total_truth_events - tp_cnt;

    double precision = (tp_cnt + fp_cnt) > 0 ? 1.0 * tp_cnt / (tp_cnt + fp_cnt) : 0;
    double recall = (tp_cnt + fn_cnt) > 0 ? 1.0 * tp_cnt / (tp_cnt + fn_cnt) : 0;
    double f1 = (precision + recall) > 0 ? 2.0 * precision * recall / (precision + recall) : 0;

    std::cout << " Precision: " << precision << std::endl;
    std::cout << " Recall: " << recall << std::endl;
    std::cout << " F1 Score: " << f1 << std::endl;
    std::cout << "Execution Time: " << elapsed_time.count() << " ms" << std::endl;
    std::cout << "Throughput: " << throughput / 1e6 << " Mpps" << std::endl;
    std::cout << " test end" << std::endl << std::endl;
}

#endif // TEST_HH