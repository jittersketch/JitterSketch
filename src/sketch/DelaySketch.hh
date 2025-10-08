#ifndef SKETCH_DELAYSKETCH_HH
#define SKETCH_DELAYSKETCH_HH

#include "detector/AbstractDetector.hh"
#include "utils/flowkey.hh"
#include "utils/hash.hh"
#include "sketch/CMSketch.hh"
#include "utils/BOBHash.hh"
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <tuple>

namespace sketch {

    struct DelaySketchBucket {
        uint16_t fp;
        uint64_t t;
    };

    template <typename hash_t>
    class DelaySketch : public AbstractDetector {
    private:
        int d_;
        int w_;
        std::vector<std::vector<DelaySketchBucket>> sketch_;
        std::vector<hash_t> hash_fns_;
        hash::BOBHash32 fp_hash_;

        CMSketch<hash_t> cm_sketch_;
        double jitter_factor_;
        uint64_t min_absolute_jitter_thres_;
        uint64_t max_ifpd_diff_;
        int jitter_detection_mode_;
        std::vector<std::pair<FlowKey<13>, uint64_t>> last_ifpd_map_;
        size_t last_ifpd_map_size_;
        hash::BOBHash32 ifpd_hash_;
        int frequency_threshold_;
        std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>> abnormal_events_;
        uint64_t start_time_;

    public:
        DelaySketch(int d, int w, double jitter_factor, uint64_t min_absolute_jitter_thres,
                    uint64_t max_ifpd_diff, size_t ifpd_map_size, int cm_width, int cm_depth, int jitter_detection_mode, int frequency_threshold);
        ~DelaySketch() = default;

        void setInitTime(uint64_t timestamp) override {
            start_time_ = timestamp;
        }
        std::string name() override { return "DelaySketch"; }
        size_t size() const override;
        uint64_t update(const FlowKey<13>& flowkey, uint64_t timestamp) override;
        auto clear() -> void override;

        const std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>>& getAbnormalEvents() const {
            return abnormal_events_;
        }
    };

    template <typename hash_t>
    DelaySketch<hash_t>::DelaySketch(int d, int w, double jitter_factor, uint64_t min_absolute_jitter_thres,
                                     uint64_t max_ifpd_diff, size_t ifpd_map_size, int cm_width, int cm_depth, int jitter_detection_mode, int frequency_threshold)
            : d_(d), w_(w), hash_fns_(d),
              jitter_factor_(jitter_factor), min_absolute_jitter_thres_(min_absolute_jitter_thres),
              max_ifpd_diff_(max_ifpd_diff), last_ifpd_map_size_(ifpd_map_size),
              cm_sketch_(cm_width, cm_depth), jitter_detection_mode_(jitter_detection_mode), frequency_threshold_(frequency_threshold) {
        sketch_.resize(d, std::vector<DelaySketchBucket>(w, {0, 0}));
        last_ifpd_map_.resize(last_ifpd_map_size_);
    }

    template <typename hash_t>
    size_t DelaySketch<hash_t>::size() const {
        return (d_ * w_ * sizeof(DelaySketchBucket)) +
               (last_ifpd_map_size_ * sizeof(std::pair<FlowKey<13>, uint64_t>)) +
               cm_sketch_.size();
    }

    template <typename hash_t>
    uint64_t DelaySketch<hash_t>::update(const FlowKey<13>& flowkey, uint64_t timestamp) {
        uint16_t fp_x = fp_hash_(flowkey) & 0xFFFF;
        uint64_t esti_delay = 0;
        bool updated = false;

        for (int i = 0; i < d_ && !updated; ++i) {
            uint32_t j = hash_fns_[i](flowkey) % w_;
            DelaySketchBucket& bucket = sketch_[i][j];

            if (bucket.fp == fp_x) {
                esti_delay = (timestamp > bucket.t) ? (timestamp - bucket.t) : 0;
                bucket.t = timestamp;
                updated = true;
            } else if (bucket.fp == 0 && bucket.t == 0) {
                esti_delay = 0;
                bucket.fp = fp_x;
                bucket.t = timestamp;
                updated = true;
            }
        }

        if (!updated) {
            int replace_row = -1;
            uint32_t replace_col = 0;
            uint64_t max_t = 0;

            for (int i = 0; i < d_; ++i) {
                uint32_t j = hash_fns_[i](flowkey) % w_;
                if (sketch_[i][j].t > max_t) {
                    max_t = sketch_[i][j].t;
                    replace_row = i;
                    replace_col = j;
                }
            }
            if (replace_row != -1) {
                DelaySketchBucket& bucket_to_replace = sketch_[replace_row][replace_col];
                esti_delay = (timestamp > bucket_to_replace.t) ? (timestamp - bucket_to_replace.t) : 0;
                bucket_to_replace.fp = fp_x;
                bucket_to_replace.t = timestamp;
            }
        }

        cm_sketch_.update(flowkey);
        if (cm_sketch_.query(flowkey) >= frequency_threshold_) {
            uint32_t index = ifpd_hash_(flowkey) % last_ifpd_map_size_;
            auto& entry = last_ifpd_map_[index];
            if (entry.first == flowkey) {
                uint64_t old_ifpd = entry.second;
                uint64_t diff = std::abs((int64_t)esti_delay - (int64_t)old_ifpd);

                bool deceleration_jitter = (old_ifpd > 0 && esti_delay > jitter_factor_ * old_ifpd);
                bool acceleration_jitter = (esti_delay > 0 && old_ifpd > jitter_factor_ * esti_delay);

                bool report = false;
                if (jitter_detection_mode_ == 0 && deceleration_jitter) {
                    report = true;
                } else if (jitter_detection_mode_ == 1 && acceleration_jitter) {
                    report = true;
                } else if (jitter_detection_mode_ == 2 && (deceleration_jitter || acceleration_jitter)) {
                    report = true;
                }

                if (report && diff > min_absolute_jitter_thres_ && diff < max_ifpd_diff_) {
                    abnormal_events_.emplace_back(flowkey, old_ifpd, esti_delay, timestamp);
                }
            }
            entry = {flowkey, esti_delay};
        }

        return esti_delay;
    }

    template <typename hash_t>
    auto DelaySketch<hash_t>::clear() -> void {
        for (auto& row : sketch_) {
            std::fill(row.begin(), row.end(), DelaySketchBucket{0, 0});
        }
        std::fill(last_ifpd_map_.begin(), last_ifpd_map_.end(), std::pair<FlowKey<13>, uint64_t>());
        abnormal_events_.clear();
        cm_sketch_.clear();
    }

} // namespace sketch

#endif // SKETCH_DELAYSKETCH_HH