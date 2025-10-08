#ifndef EXPERIMENT_JITTEREXPERIMENT_HH
#define EXPERIMENT_JITTEREXPERIMENT_HH

#include "utils/core.hh"
#include "optimizer/JitterOptimizer.hh"
#include "optimizer/JitterSketchOptimizer.hh"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <map>

class INIReader;

class JitterControlExperiment {
public:
    JitterControlExperiment(const std::vector<core::Record>& records,
                            std::shared_ptr<JitterOptimizer> optimizer,
                            std::shared_ptr<INIReader> config);
    void run();

private:
    const std::vector<core::Record>& records_;
    std::shared_ptr<JitterOptimizer> optimizer_;
    std::shared_ptr<INIReader> config_;

    int frequency_threshold_;
    int max_buffers_;
    uint64_t buffer_timeout_us_;
    int B_size_;


    uint64_t calculateDelayVariation(const std::vector<uint64_t>& timestamps);
    int countVariationFlows(const std::map<FlowKey<13>, std::vector<uint64_t>>& all_timestamps);
};

#endif // EXPERIMENT_JITTEREXPERIMENT_HH