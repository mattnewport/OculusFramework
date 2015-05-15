#pragma once

#include "farmhash.h"

#include <functional>
#include <numeric>
#include <vector>

size_t hashWithSeed(const char* data, size_t dataSize, size_t seed);

inline size_t hashCombineWithSeed(size_t seed) { return seed; }

template <typename T, typename... Ts>
inline size_t hashCombineWithSeed(size_t seed, const T& t, Ts&&... ts) {
    seed = hashWithSeed(reinterpret_cast<const char*>(&t), sizeof(t), seed);
    return hashCombineWithSeed(seed, ts...);
}

template <typename... Ts>
inline size_t hashCombine(Ts&&... ts) {
    return hashCombineWithSeed(0, ts...);
}

template <typename It>
inline size_t hashCombineRangeWithSeed(size_t seed, It first, It last) {
    return std::accumulate(first, last, seed, [](auto seed, auto val) {
        const auto valHash = std::hash<decltype(val)>{}(val);
        return hashWithSeed(reinterpret_cast<const char*>(&valHash), sizeof(valHash), seed);
    });
}

template <typename It>
inline size_t hashCombineRange(It first, It last) {
    return hashCombineRangeWithSeed(0, first, last);
}

namespace std {
    template <typename T>
    struct hash<vector<T>> {
        size_t operator()(const vector<T>& x) const { return hashCombineRange(begin(x), end(x)); }
    };
}


