#ifndef SKETCH_CMSKETCH_HH
#define SKETCH_CMSKETCH_HH

#include "utils/hash.hh"
#include "utils/flowkey.hh"
#include <vector>
#include <algorithm>
#include <limits>

namespace sketch {

    template <typename hash_t>
    class CMSketch {
    private:
        int width_;
        int depth_;
        std::vector<std::vector<uint32_t>> sketch_;
        std::vector<hash_t> hash_fns_;

    public:
        CMSketch(int width, int depth);
        ~CMSketch() = default;

        template <int32_t key_len>
        void update(const FlowKey<key_len>& flowkey, int count = 1);

        template <int32_t key_len>
        uint32_t query(const FlowKey<key_len>& flowkey) const;

        void clear();
        size_t size() const;
    };

    template <typename hash_t>
    CMSketch<hash_t>::CMSketch(int width, int depth)
            : width_(width), depth_(depth), hash_fns_(depth) {
        sketch_.resize(depth, std::vector<uint32_t>(width, 0));
    }

    template <typename hash_t>
    template <int32_t key_len>
    void CMSketch<hash_t>::update(const FlowKey<key_len>& flowkey, int count) {
        for (int i = 0; i < depth_; ++i) {
            uint32_t index = hash_fns_[i](flowkey) % width_;
            sketch_[i][index] += count;
        }
    }

    template <typename hash_t>
    template <int32_t key_len>
    uint32_t CMSketch<hash_t>::query(const FlowKey<key_len>& flowkey) const {
        uint32_t min_count = std::numeric_limits<uint32_t>::max();
        for (int i = 0; i < depth_; ++i) {
            uint32_t index = hash_fns_[i](flowkey) % width_;
            min_count = std::min(min_count, sketch_[i][index]);
        }
        return min_count;
    }

    template <typename hash_t>
    void CMSketch<hash_t>::clear() {
        for (int i = 0; i < depth_; ++i) {
            std::fill(sketch_[i].begin(), sketch_[i].end(), 0);
        }
    }

    template <typename hash_t>
    size_t CMSketch<hash_t>::size() const {
        return depth_ * width_ * sizeof(uint32_t);
    }

} // namespace sketch

#endif // SKETCH_CMSKETCH_HH