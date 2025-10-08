

#ifndef COMMON_BOBHASH_HH
#define COMMON_BOBHASH_HH
#include "utils/flowkey.hh"
#include <cstdint>
#include <ctime>

namespace hash {
    constexpr int MAX_BIG_PRIME32 = 50;
    constexpr int MAX_PRIME32 = 1229;

// Use 'extern' to declare the arrays. Their definitions are now in BOBHash.cc
    extern const uint32_t big_prime3232[MAX_BIG_PRIME32];
    extern const uint32_t prime32[MAX_PRIME32];

    class BOBHash32 {
    private:
        uint32_t prime32Num_;

    public:
        static void random_seed();

        // Declarations only
        BOBHash32() noexcept;
        ~BOBHash32() noexcept;
        uint32_t operator()(const uint8_t *data, int n) const;

        template <int32_t key_len>
        uint32_t operator()(const FlowKey<key_len> &flowkey) const {
            return this->operator()(flowkey.cKey(), key_len);
        }
    };

} // namespace hash

#endif
