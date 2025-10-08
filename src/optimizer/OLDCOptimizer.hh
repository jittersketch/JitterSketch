#ifndef OPTIMIZER_ALGORITHMBOPTIMIZER_HH
#define OPTIMIZER_ALGORITHMBOPTIMIZER_HH

#include "JitterOptimizer.hh"

class OLDCOptimizer : public JitterOptimizer {
public:
    OLDCOptimizer();

    void configure(std::shared_ptr<INIReader> config) override;

    std::vector<uint64_t> optimize(const std::vector<uint64_t>& arrival_times) override;

    std::string name() const override;

private:
    int B_;
};

#endif // OPTIMIZER_ALGORITHMBOPTIMIZER_HH