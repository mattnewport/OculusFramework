#include "hashhelpers.h"

size_t hashWithSeed(const char * data, size_t dataSize, size_t seed) {
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
    if (sizeof(size_t) == sizeof(uint32_t)) {
        return util::Hash32WithSeed(data, dataSize, seed);
    }
    else if (sizeof(size_t) == sizeof(uint64_t)) {
        return static_cast<size_t>(util::Hash64WithSeed(data, dataSize, seed));
    }
    else { // should never get here
        assert(false);
        return 0;
    }
#pragma warning(pop)
}
