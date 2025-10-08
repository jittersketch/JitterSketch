

#ifndef SKETCH_DJSKETCHS1OPT_HH
#define SKETCH_DJSKETCHS1OPT_HH

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

#define SMALL_TYPE uint16_t

namespace sketch
{
    struct JitterSketchS1OptStageOneBucket {
        uint16_t fp;
        uint32_t freq;
    };

    struct JitterSketchS1OptStageTwoBucket {
        SMALL_TYPE smallIFPD;
        uint32_t longFP;
        uint64_t lastArrivalTime;
    };

    struct JitterSketchS1OptStageThreeEntry {
        uint64_t lastArrivalTime;
        uint64_t IFPD;
        FlowKey<13> fullID;

        JitterSketchS1OptStageThreeEntry() : lastArrivalTime(0), IFPD(0) {}
    };

    struct JitterSketchS1OptStageThreeBucket{
        std::vector<JitterSketchS1OptStageThreeEntry> entries;
        JitterSketchS1OptStageThreeBucket(size_t entry_num = 4) : entries(entry_num) {}
    };


    template <typename hash_t>
    class JitterSketchS1Opt : public AbstractDetector
    {
    private:
        std::vector<hash::BOBHash32> stage_one_hashes_;
        hash::BOBHash32 bob_hash_2_;
        hash::BOBHash32 bob_hash_3_;

        std::vector<JitterSketchS1OptStageOneBucket> stage_one_;
        std::vector<JitterSketchS1OptStageTwoBucket> stage_two_;
        std::vector<JitterSketchS1OptStageThreeBucket> stage_three_;

        int w1_, w2_, w3_, d3_;
        int s1_hash_num_;

        double jitter_factor_;
        uint64_t min_absolute_jitter_thres_;
        uint64_t max_ifpd_diff_;
        int jitter_detection_mode_;
        int frequency_threshold_;

        std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>> abnormal_events_;
        uint64_t start_time_;

    public:
        JitterSketchS1Opt(int w1, int w2, int w3, int d3, int s1_hash_num, double jitter_factor,
                          uint64_t min_absolute_jitter_thres, uint64_t max_ifpd_diff, int jitter_detection_mode, int frequency_threshold);
        ~JitterSketchS1Opt() = default;

        void setInitTime(uint64_t timestamp) override {
            start_time_ = timestamp;
        }
        std::string name() override { return "JitterSketch-Opt"; }
        size_t size() const override;
        uint64_t update(const FlowKey<13>& flowkey, uint64_t timestamp) override;
        auto clear() -> void override;

        const std::vector<std::tuple<FlowKey<13>, uint64_t, uint64_t, uint64_t>>& getAbnormalEvents() const {
            return abnormal_events_;
        }
    };
}
#endif