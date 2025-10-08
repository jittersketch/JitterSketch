#ifndef OPTIMIZER_JITTEROPTIMIZER_HH
#define OPTIMIZER_JITTEROPTIMIZER_HH

#include <vector>
#include <cstdint>
#include <memory>

class INIReader;

class JitterOptimizer {
public:
    virtual ~JitterOptimizer() = default;

    virtual void configure(std::shared_ptr<INIReader> config) = 0;

    virtual std::vector<uint64_t> optimize(const std::vector<uint64_t>& arrival_times) = 0;

    virtual std::string name() const = 0;
};

#endif // OPTIMIZER_JITTEROPTIMIZER_HH