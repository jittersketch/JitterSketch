#include "OLDCOptimizer.hh"
#include "utils/INIReader.h"
#include <stdexcept>
#include <limits>

OLDCOptimizer::OLDCOptimizer() : B_(0) {}

void OLDCOptimizer::configure(std::shared_ptr<INIReader> config) {
    if (!config) {
        throw std::runtime_error("Configuration is null for OLDCOptimizer");
    }
    B_ = config->GetInteger("JitterControlExperiment", "B_size", 10);
}

std::string OLDCOptimizer::name() const {
    return "OLDC";
}

std::vector<uint64_t> OLDCOptimizer::optimize(const std::vector<uint64_t>& arrival_times) {
    size_t n = arrival_times.size();

    if (n <= static_cast<size_t>(2 * B_)) {
        return arrival_times;
    }

    std::vector<uint64_t> release_times(n);

    double X_a = static_cast<double>(arrival_times.back() - arrival_times.front()) / (n - 1);

    uint64_t a_B = arrival_times[B_];

    for (size_t k = 0; k < n; ++k) {
        uint64_t s_on_star_k = a_B + static_cast<uint64_t>(k * X_a);

        uint64_t a_k = arrival_times[k];
        uint64_t a_k_plus_2B = (k + 2 * B_ < n) ? arrival_times[k + 2 * B_] : std::numeric_limits<uint64_t>::max();

        uint64_t s_on_k = s_on_star_k;
        if (s_on_k < a_k) {
            s_on_k = a_k;
        }
        if (s_on_k > a_k_plus_2B) {
            s_on_k = a_k_plus_2B;
        }
        release_times[k] = s_on_k;
    }
    return release_times;
}