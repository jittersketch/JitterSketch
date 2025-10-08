
#ifndef SKETCH_DJSKETCH_HH
#define SKETCH_DJSKETCH_HH

#include "detector/AbstractDetector.hh"
#include "utils/flowkey.hh"
#include "utils/hash.hh"
#include "utils/BOBHash.hh"
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <tuple>
#include <iostream>

#define SMALL_TYPE uint32_t

namespace sketch
{
    struct JitterSketchStageOneBucket {
        uint16_t fp;
        uint32_t freq;
    };

    struct JitterSketchStageTwoBucket {
        SMALL_TYPE smallIFPD;
        uint32_t longFP;
        uint64_t lastArrivalTime;
    };

    struct JitterSketchStageThreeEntry {
        uint64_t lastArrivalTime;
        uint64_t IFPD;
        FlowKey<13> fullID;

        JitterSketchStageThreeEntry() : lastArrivalTime(0), IFPD(0) {}
    };

    struct JitterSketchStageThreeBucket{
        std::vector<JitterSketchStageThreeEntry> entries;
        JitterSketchStageThreeBucket(size_t entry_num = 4) : entries(entry_num) {}
    };


    template <typename hash_t>
    class JitterSketch : public AbstractDetector
    {
    private:
        hash::BOBHash32 bob_hash_;

        std::vector<JitterSketchStageOneBucket> stage_one_;
        std::vector<JitterSketchStageTwoBucket> stage_two_;
        std::vector<JitterSketchStageThreeBucket> stage_three_;

        int w1_, w2_, w3_, d3_;

        double jitter_factor_;
        uint64_t min_absolute_jitter_thres_;
        uint64_t max_ifpd_diff_;
        int jitter_detection_mode_;
        int frequency_threshold_;
        std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>> abnormal_events_;
        uint64_t start_time_;

    public:
        JitterSketch(int w1, int w2, int w3, int d3, double jitter_factor,
                     uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold);
        ~JitterSketch() = default;

        void setInitTime(uint64_t timestamp) override {
            start_time_ = timestamp;
        }
        std::string name() override { return "JitterSketch"; }
        size_t size() const override;
        uint64_t update(const FlowKey<13>& flowkey, uint64_t timestamp) override;
        auto clear() -> void override;

        const std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>>& getAbnormalEvents() const {
            return abnormal_events_;
        }
    };

    template <typename hash_t>
    JitterSketch<hash_t>::JitterSketch(int w1, int w2, int w3, int d3, double jitter_factor,
                                       uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold)
            : w1_(w1), w2_(w2), w3_(w3), d3_(d3),
              jitter_factor_(jitter_factor), min_absolute_jitter_thres_(min_absolute_jitter_thres),
              max_ifpd_diff_(max_ifpd_diff), jitter_detection_mode_(jitter_detection_mode), frequency_threshold_(frequency_threshold - 2) {
        stage_one_.resize(w1, {0, 0});
        stage_two_.resize(w2, {0, 0, 0xFF});
        stage_three_.resize(w3, JitterSketchStageThreeBucket(d3));
    }

    template<typename hash_t>
    size_t JitterSketch<hash_t>::size() const {
        size_t s3_entry_size = w3_ * d3_ * sizeof(JitterSketchStageThreeEntry);
        return (w1_ * sizeof(JitterSketchStageOneBucket)) +
               (w2_ * sizeof(JitterSketchStageTwoBucket)) +
               s3_entry_size;
    }

    template<typename hash_t>
    uint64_t JitterSketch<hash_t>::update(const FlowKey<13>& flowkey, uint64_t timestamp) {
        uint64_t esti_delay = 0;

        uint32_t hash_val = bob_hash_(flowkey);
        uint32_t s1_idx = hash_val % w1_;
        uint16_t fp = (hash_val / w1_) & 0xFFFF;

        uint32_t hash2 = (hash_val >> 16) | (hash_val << 16);
        uint32_t s2_idx = hash2 % w2_;
        uint32_t longFp_val = hash2 / w2_;

        uint32_t hash3 = hash_val ^ (hash2);
        uint32_t s3_idx = hash3 % w3_;

        for (auto& entry : stage_three_[s3_idx].entries) {
            if (entry.fullID == flowkey) {
                uint64_t old_ifpd = entry.IFPD;
                esti_delay = (timestamp > entry.lastArrivalTime) ? (timestamp - entry.lastArrivalTime) : 0;
                uint64_t diff = std::abs((int64_t)esti_delay - (int64_t)old_ifpd);

                bool deceleration_jitter = (old_ifpd > 0 && esti_delay > jitter_factor_ * old_ifpd);
                bool acceleration_jitter = (esti_delay > 0 && old_ifpd > jitter_factor_ * esti_delay);

                bool report = false;
                if (jitter_detection_mode_ == 0 && deceleration_jitter) report = true;
                else if (jitter_detection_mode_ == 1 && acceleration_jitter) report = true;
                else if (jitter_detection_mode_ == 2 && (deceleration_jitter || acceleration_jitter)) report = true;

                if (report && diff > min_absolute_jitter_thres_ && diff < max_ifpd_diff_) {
                    abnormal_events_.emplace_back(flowkey, old_ifpd, esti_delay, timestamp);
                }

                entry.IFPD = esti_delay;
                entry.lastArrivalTime = timestamp;
                return esti_delay;
            }
        }

        bool flag = false;
        JitterSketchStageTwoBucket& s2_bucket = stage_two_[s2_idx];
        if (s2_bucket.longFP == longFp_val) {
            esti_delay = (timestamp > s2_bucket.lastArrivalTime) ? (timestamp - s2_bucket.lastArrivalTime) : 0;
            uint64_t old_ifpd = s2_bucket.smallIFPD;

            uint64_t diff = std::abs((int64_t)esti_delay - (int64_t)old_ifpd);
            bool deceleration_jitter = (old_ifpd > 0 && esti_delay > jitter_factor_ * old_ifpd);
            bool acceleration_jitter = (esti_delay > 0 && old_ifpd > jitter_factor_ * esti_delay);
            bool report = false;
            if (jitter_detection_mode_ == 0 && deceleration_jitter) report = true;
            else if (jitter_detection_mode_ == 1 && acceleration_jitter) report = true;
            else if (jitter_detection_mode_ == 2 && (deceleration_jitter || acceleration_jitter)) report = true;

            if (report && diff > min_absolute_jitter_thres_ && diff < max_ifpd_diff_) {
                abnormal_events_.emplace_back(flowkey, old_ifpd, esti_delay, timestamp);
                flag = true;
            }

            if (esti_delay >= std::numeric_limits<SMALL_TYPE>::max() || flag) {
                int empty_entry_idx = -1;
                int replace_idx = -1;
                double max_idle_index = -1.0;

                for (int i = 0; i < d3_; ++i) {
                    auto& entry = stage_three_[s3_idx].entries[i];
                    if (entry.lastArrivalTime == 0) {
                        empty_entry_idx = i;
                        break;
                    }
                    double idle_index = (entry.IFPD > 0) ? ((double)(timestamp - entry.lastArrivalTime) / entry.IFPD) : std::numeric_limits<double>::max();
                    if (idle_index > max_idle_index) {
                        max_idle_index = idle_index;
                        replace_idx = i;
                    }
                }

                int target_idx = (empty_entry_idx != -1) ? empty_entry_idx : replace_idx;
                auto& target_entry = stage_three_[s3_idx].entries[target_idx];
                target_entry.fullID = flowkey;
                target_entry.lastArrivalTime = timestamp;
                target_entry.IFPD = esti_delay;

                s2_bucket = {0, 0, 0xFF};
            } else {
                s2_bucket.lastArrivalTime = timestamp;
                s2_bucket.smallIFPD = static_cast<SMALL_TYPE>(esti_delay);
            }
            return esti_delay;
        }

        JitterSketchStageOneBucket& s1_bucket = stage_one_[s1_idx];
        if (s1_bucket.fp == fp) {
            s1_bucket.freq++;
            if (s1_bucket.freq > frequency_threshold_) {
                s2_bucket.longFP = longFp_val;
                s2_bucket.lastArrivalTime = timestamp;
                s2_bucket.smallIFPD = std::numeric_limits<SMALL_TYPE>::max();
                s1_bucket = {0, 0};
            }
        } else if (s1_bucket.freq == 0) {
            s1_bucket.fp = fp;
            s1_bucket.freq = 1;
        } else {
            s1_bucket.freq--;
            if (s1_bucket.freq == 0) {
                s1_bucket.fp = fp;
                s1_bucket.freq = 1;
            }
        }

        return esti_delay;
    }


    template<typename hash_t>
    auto JitterSketch<hash_t>::clear() -> void {
        std::fill(stage_one_.begin(), stage_one_.end(), JitterSketchStageOneBucket{0, 0});
        std::fill(stage_two_.begin(), stage_two_.end(), JitterSketchStageTwoBucket{0, 0, 0xFF});
        for (auto& bucket : stage_three_) {
            for (auto& entry : bucket.entries) {
                entry = JitterSketchStageThreeEntry();
            }
        }
        abnormal_events_.clear();
    }
}

#endif