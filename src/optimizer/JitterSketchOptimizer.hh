

#ifndef OPTIMIZER_DJSKETCHOPTIMIZER_HH
#define OPTIMIZER_DJSKETCHOPTIMIZER_HH

#include "JitterOptimizer.hh"
#include "sketch/JitterSketch.hh"
#include "utils/hash.hh"
#include <set>
#include <memory>

class JitterSketchOptimizer : public JitterOptimizer {
public:
    JitterSketchOptimizer();

    void configure(std::shared_ptr<INIReader> config) override;

    std::vector<uint64_t> optimize(const std::vector<uint64_t>& arrival_times) override;

    std::string name() const override;

    void processPacket(const FlowKey<13>& flowkey, uint64_t timestamp);

    bool hasJitter(const FlowKey<13>& flowkey);

    void clearJitteredFlows();

private:
    int B_;
    std::unique_ptr<sketch::JitterSketch<hash::AwareHash>> dj_sketch_;
    std::set<FlowKey<13>> jittered_flows_;
};

#endif