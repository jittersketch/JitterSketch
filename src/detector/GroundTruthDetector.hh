#ifndef DETECTOR_GROUNDTRUTHDETECTOR_HH
#define DETECTOR_GROUNDTRUTHDETECTOR_HH

#include "utils/flowkey.hh"
#include "utils/core.hh"
#include <map>
#include <cstdint>
#include <vector>
#include <tuple>
class GroundTruthDetector {
private:
    std::map<FlowKey<13>, uint64_t> flow_map;
    std::map<FlowKey<13>, uint64_t> last_ifpd_map;
    std::map<FlowKey<13>, int> flow_counts;
    std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>> abnormal_events;
    double jitter_factor_;
    uint64_t min_absolute_jitter_thres_;
    uint64_t max_ifpd_diff_;
    int jitter_detection_mode_; // 0: deceleration, 1: acceleration, 2: both
    int frequency_threshold_;

public:
    GroundTruthDetector(double jitter_factor, uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold)
            : jitter_factor_(jitter_factor), min_absolute_jitter_thres_(min_absolute_jitter_thres), max_ifpd_diff_(max_ifpd_diff), jitter_detection_mode_(jitter_detection_mode), frequency_threshold_(frequency_threshold) {}

    uint64_t update(const core::Record& record) {
        uint64_t real_delay = 0;
        auto iter = flow_map.find(record.flowkey_);
        if (iter == flow_map.end()) {
            flow_map.emplace(record.flowkey_, record.timestamp_);
            real_delay = 0;
        } else {
            real_delay = record.timestamp_ - iter->second;
            iter->second = record.timestamp_;
        }

        flow_counts[record.flowkey_]++;
        if (flow_counts[record.flowkey_] >= frequency_threshold_) {
            auto ifpd_iter = last_ifpd_map.find(record.flowkey_);
            if (ifpd_iter != last_ifpd_map.end()) {
                uint64_t old_ifpd = ifpd_iter->second;
                uint64_t diff = std::abs((int64_t)real_delay - (int64_t)old_ifpd);

                bool deceleration_jitter = (old_ifpd > 0 && real_delay > jitter_factor_ * old_ifpd);
                bool acceleration_jitter = (real_delay > 0 && old_ifpd > jitter_factor_ * real_delay);

                bool report = false;
                if (jitter_detection_mode_ == 0 && deceleration_jitter) {
                    report = true;
                } else if (jitter_detection_mode_ == 1 && acceleration_jitter) {
                    report = true;
                } else if (jitter_detection_mode_ == 2 && (deceleration_jitter || acceleration_jitter)) {
                    report = true;
                }


                if (report && diff > min_absolute_jitter_thres_ && diff < max_ifpd_diff_) {
                    abnormal_events.emplace_back(record.flowkey_, old_ifpd, real_delay, record.timestamp_);
                }
            }
            last_ifpd_map[record.flowkey_] = real_delay;
        }

        return real_delay;
    }

    size_t get_flow_count() const {
        return flow_map.size();
    }

    const std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>>& getAbnormalEvents() const {
        return abnormal_events;
    }

    void clear() {
        flow_map.clear();
        last_ifpd_map.clear();
        abnormal_events.clear();
        flow_counts.clear();
    }
};

#endif // DETECTOR_GROUNDTRUTHDETECTOR_HH