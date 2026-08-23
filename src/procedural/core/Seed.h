#pragma once

#include <cstdint>

namespace procengine {

using Seed = uint64_t;

class Rng {
public:
    explicit Rng(Seed seed) : state_(seed) {}

    uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return static_cast<uint32_t>(state_);
    }

    float nextFloat() {
        return static_cast<float>(next()) / static_cast<float>(0xFFFFFFFF);
    }

    float range(float min, float max) {
        return min + nextFloat() * (max - min);
    }

    int rangeInt(int min, int max) {
        return min + static_cast<int>(nextFloat() * (max - min));
    }

private:
    Seed state_;
};

}