#ifndef DETECTOR_ABSTRACTDETECTOR_HH
#define DETECTOR_ABSTRACTDETECTOR_HH

#include "utils/flowkey.hh"
#include <string>
#include <cstdint>

class AbstractDetector {
public:
    virtual ~AbstractDetector() = default;

    virtual void setInitTime(uint64_t timestamp) = 0;

    virtual std::string name() = 0;

    virtual size_t size() const = 0;

    virtual uint64_t update(const FlowKey<13> &flowkey, uint64_t timestamp) = 0;

    virtual auto clear() -> void = 0;
};

#endif // DETECTOR_ABSTRACTDETECTOR_HH