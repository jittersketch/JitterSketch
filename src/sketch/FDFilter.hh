#ifndef SKETCH_FDFILTER_HH
#define SKETCH_FDFILTER_HH

#include "utils/flowkey.hh"
#include "utils/hash.hh"
#include "sketch/BitBloomFilter.hh"
#include "detector/AbstractDetector.hh"
#include "utils/BOBHash.hh"
#include "sketch/CMSketch.hh"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <tuple>

namespace sketch {

    template <typename hash_t>
    class FDFilter : public AbstractDetector {
    private:
        std::vector<BitBf<hash_t>> bfs_;
        BloomFilter<hash_t> gbf_;
        CMSketch<hash_t> cm_sketch_;
        int k_;
        int kk_;
        int part;
        int sub_win_num = 0;
        uint64_t start_time_;
        uint64_t delay_thres_;
        uint64_t last_update_;

        double jitter_factor_;
        uint64_t min_absolute_jitter_thres_;
        uint64_t max_ifpd_diff_;
        int jitter_detection_mode_;
        std::vector<std::pair<FlowKey<13>, uint64_t>> last_ifpd_map_;
        size_t last_ifpd_map_size_;
        hash::BOBHash32 ifpd_hash_;
        const int C = 30;
        std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>> abnormal_events_;

    public:
        FDFilter(int k, int kk, int nbits, int num_hash,
                 int gnbits, int gnum_hash, uint64_t delay_thres, double jitter_factor,
                 uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff,
                 size_t ifpd_map_size, int cm_width, int cm_depth, int jitter_detection_mode, int m);
        ~FDFilter();

        void setInitTime(uint64_t timestamp) override {
            last_update_ = start_time_ = timestamp;
        }
        std::string name() override { return "FDFilter"; }
        size_t size() const override;
        uint64_t update(const FlowKey<13> &flowkey, uint64_t timestamp) override;
        auto clear() -> void override;

        const std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>>& getAbnormalEvents() const {
            return abnormal_events_;
        }
    };

    template <typename hash_t>
    FDFilter<hash_t>::FDFilter(int k, int kk, int nbits, int num_hash,
                               int gnbits, int gnum_hash, uint64_t delay_thres, double jitter_factor,
                               uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff,
                               size_t ifpd_map_size, int cm_width, int cm_depth, int jitter_detection_mode, int m)
            : k_(k), kk_(kk), delay_thres_(delay_thres), jitter_factor_(jitter_factor),
              min_absolute_jitter_thres_(min_absolute_jitter_thres), max_ifpd_diff_(max_ifpd_diff),
              gbf_(gnbits, gnum_hash),
              bfs_(k + 1, BitBf<hash_t>(kk, nbits, num_hash, delay_thres / (k * ((1 << kk) - 1)))),
              last_ifpd_map_size_(ifpd_map_size),
              cm_sketch_(cm_width, cm_depth),
              jitter_detection_mode_(jitter_detection_mode)
    {
        part = k * ((1 << kk) - 1);
        last_ifpd_map_.resize(last_ifpd_map_size_);
    }

    template <typename hash_t>
    FDFilter<hash_t>::~FDFilter() {}

    template <typename hash_t>
    uint64_t FDFilter<hash_t>::update(const FlowKey<13> &flowkey, uint64_t timestamp) {
        if ((timestamp - last_update_) * part >= delay_thres_) {
            last_update_ = timestamp;
            sub_win_num++;
            if (sub_win_num % ((1 << kk_) - 1) == 0) {
                for (int i = 0; i < k_; ++i) {
                    bfs_[i].swap(bfs_[i + 1]);
                }
                bfs_[k_].clear();
            }
        }

        uint64_t esti_delay = 0;
        if (!gbf_.query(flowkey)) {
            gbf_.insert(flowkey);
            bfs_[k_].update(flowkey, sub_win_num % ((1 << kk_) - 1) + 1);
            esti_delay = 0;
        } else {
            int i = 0;
            uint64_t ret = 0;
            for (; i <= k_; ++i) {
                if ((ret = bfs_[k_ - i].query(flowkey))) {
                    break;
                }
            }

            uint64_t interval = delay_thres_ / part;
            int now = sub_win_num % ((1 << kk_) - 1) + 1;

            bfs_[k_].update(flowkey, now);

            if (i == 0) {
                if (ret == now)
                    esti_delay = timestamp - last_update_;
                else
                    esti_delay = timestamp - last_update_ + (now - 1) * interval;
            } else {
                esti_delay = timestamp - last_update_ +
                             (((1 << kk_) - 1) - (int)ret + (i - 1) * ((1 << kk_) - 1) + now - 1) * interval +
                             interval / 2;
            }
        }

        cm_sketch_.update(flowkey);
        if (cm_sketch_.query(flowkey) >= C) {
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
    size_t FDFilter<hash_t>::size() const {
        return bfs_[0].size() * (k_ + 1) + gbf_.size() +
               last_ifpd_map_size_ * sizeof(std::pair<FlowKey<13>, uint64_t>) +
               cm_sketch_.size();
    }

    template <typename hash_t>
    auto FDFilter<hash_t>::clear() -> void {
        for (auto &bf : bfs_) {
            bf.clear();
        }
        std::fill(last_ifpd_map_.begin(), last_ifpd_map_.end(), std::pair<FlowKey<13>, uint64_t>());
        abnormal_events_.clear();
        cm_sketch_.clear();
    }

} // namespace sketch

#endif // SKETCH_FDFILTER_HH
