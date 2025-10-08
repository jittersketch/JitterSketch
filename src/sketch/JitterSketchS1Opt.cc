

#include "JitterSketchS1Opt.hh"

namespace sketch
{
    template <typename hash_t>
    JitterSketchS1Opt<hash_t>::JitterSketchS1Opt(int w1, int w2, int w3, int d3, int s1_hash_num, double jitter_factor,
                                                 uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold)
            : w1_(w1), w2_(w2), w3_(w3), d3_(d3), s1_hash_num_(s1_hash_num),
              jitter_factor_(jitter_factor), min_absolute_jitter_thres_(min_absolute_jitter_thres),
              max_ifpd_diff_(max_ifpd_diff), jitter_detection_mode_(jitter_detection_mode), frequency_threshold_(frequency_threshold - 2),
              stage_one_hashes_(s1_hash_num)
    {
        stage_one_.resize(w1, {0, 0});
        stage_two_.resize(w2, {0, 0, 0xFF});
        stage_three_.resize(w3, JitterSketchS1OptStageThreeBucket(d3));
    }

    template<typename hash_t>
    size_t JitterSketchS1Opt<hash_t>::size() const {
        size_t s3_entry_size = w3_ * d3_ * sizeof(JitterSketchS1OptStageThreeEntry);
        return (w1_ * sizeof(JitterSketchS1OptStageOneBucket)) +
               (w2_ * sizeof(JitterSketchS1OptStageTwoBucket)) +
               s3_entry_size;
    }

    template<typename hash_t>
    uint64_t JitterSketchS1Opt<hash_t>::update(const FlowKey<13>& flowkey, uint64_t timestamp) {
        uint64_t esti_delay = 0;

        uint32_t hash2_val = bob_hash_2_(flowkey);
        uint32_t s2_idx = hash2_val % w2_;
        uint32_t longFP_val = hash2_val / w2_;

        uint32_t hash3_val = bob_hash_3_(flowkey);
        uint32_t s3_idx = hash3_val % w3_;

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
        JitterSketchS1OptStageTwoBucket& s2_bucket = stage_two_[s2_idx];
        if (s2_bucket.longFP == longFP_val) {
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

        bool matched = false;

        for (int i = 0; i < s1_hash_num_; ++i) {
            uint32_t hash_val = stage_one_hashes_[i](flowkey);
            uint32_t s1_idx = hash_val % w1_;
            uint16_t fp = (hash_val / w1_) & 0xFFFF;

            if (stage_one_[s1_idx].fp == fp) {
                stage_one_[s1_idx].freq++;
                if (stage_one_[s1_idx].freq > frequency_threshold_) {
                    s2_bucket.longFP = longFP_val;
                    s2_bucket.lastArrivalTime = timestamp;
                    s2_bucket.smallIFPD = std::numeric_limits<SMALL_TYPE>::max();
                    stage_one_[s1_idx] = {0, 0};
                }
                matched = true;
                break;
            }
        }

        if (!matched) {
            int empty_s1_idx = -1;
            int empty_hash_idx = -1;
            for (int i = 0; i < s1_hash_num_; ++i) {
                uint32_t hash_val = stage_one_hashes_[i](flowkey);
                uint32_t s1_idx = hash_val % w1_;
                if (stage_one_[s1_idx].freq == 0) {
                    empty_s1_idx = s1_idx;
                    empty_hash_idx = i;
                    break;
                }
            }

            if (empty_s1_idx != -1) {
                uint32_t hash_val = stage_one_hashes_[empty_hash_idx](flowkey);
                uint16_t fp = (hash_val / w1_) & 0xFFFF;
                stage_one_[empty_s1_idx].fp = fp;
                stage_one_[empty_s1_idx].freq = 1;
            } else {
                int min_c_s1_idx = -1;
                uint32_t min_c = std::numeric_limits<uint32_t>::max();
                for (int i = 0; i < s1_hash_num_; ++i) {
                    uint32_t hash_val = stage_one_hashes_[i](flowkey);
                    uint32_t s1_idx = hash_val % w1_;
                    if (stage_one_[s1_idx].freq < min_c) {
                        min_c = stage_one_[s1_idx].freq;
                        min_c_s1_idx = s1_idx;
                    }
                }

                if (min_c_s1_idx != -1) {
                    stage_one_[min_c_s1_idx].freq--;
                }
            }
        }

        return esti_delay;
    }


    template<typename hash_t>
    auto JitterSketchS1Opt<hash_t>::clear() -> void {
        std::fill(stage_one_.begin(), stage_one_.end(), JitterSketchS1OptStageOneBucket{0, 0});
        std::fill(stage_two_.begin(), stage_two_.end(), JitterSketchS1OptStageTwoBucket{0, 0, 0xFF});
        for (auto& bucket : stage_three_) {
            for (auto& entry : bucket.entries) {
                entry = JitterSketchS1OptStageThreeEntry();
            }
        }
        abnormal_events_.clear();
    }

    template class JitterSketchS1Opt<hash::AwareHash>;
}