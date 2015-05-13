#pragma once

#include "farmhash.h"

#include <cassert>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

inline size_t hashCombine32(size_t seed) { return seed; }

template <typename T, typename... Ts>
inline size_t hashCombine32(size_t seed, const T& t, Ts&&... ts) {
    seed = util::Hash32WithSeed(reinterpret_cast<const char*>(&t), sizeof(t), seed);
    return hashCombine32(seed, ts...);
}

inline size_t hashCombine64(size_t seed) { return seed; }

template <typename T, typename... Ts>
inline size_t hashCombine64(size_t seed, const T& t, Ts&&... ts) {
    seed = static_cast<size_t>(
        util::Hash64WithSeed(reinterpret_cast<const char*>(&t), sizeof(t), seed));
    return hashCombine64(seed, ts...);
}

template <typename... Ts>
inline size_t hashCombine(Ts&&... ts) {
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
    if (sizeof(size_t) == sizeof(uint32_t)) {
        return hashCombine32(0, ts...);
    } else if (sizeof(size_t) == sizeof(uint64_t)) {
        return hashCombine64(0, ts...);
    } else {  // should never get here...
        assert(false);
        return 0;
    }
#pragma warning(pop)
}

template <typename Key, typename Resource>
class ResourceManagerBase {
public:
    using KeyType = Key;
    using ResourceType = Resource;

    // ResourceHandles are tracking and threadsafe which makes them fairly expensive to copy so you
    // don't want to pass them around by value. The system is designed so that a resource is not
    // freed for 2 frames after the last ResourceHandle goes away so it is safe to pass raw
    // Resources around within a frame. For this reason it is not possible to copy construct or
    // assign a ResourceHandle.
    friend class ResourceHandle;
    class ResourceHandle {
    public:
        ResourceHandle() = default;
        ResourceHandle(const ResourceHandle&) = delete;
        ResourceHandle(ResourceHandle&& x) : owner{ x.owner }, key{ x.key }, resource{ x.resource } {
            owner->track(*key, *this);
        }
        ResourceHandle(ResourceManagerBase& owner_, const KeyType& key_, ResourceType& resource_)
            : owner{&owner_}, key{&key_}, resource{&resource_} {
            // This could be optimized since generally we just inserted the resource so no need to look it up again. It would complicate the code though.
            owner->track(*key, *this);
        }
        ~ResourceHandle() {
            owner->untrack(key, *this);
        }

        ResourceHandle& operator=(const ResourceHandle&) = delete;
        ResourceHandle& operator=(ResourceHandle&& x) {
            if (this != &x) {
                owner->untrack(key, *this);
                owner = x.owner;
                key = x.key;
                resource = x.resource;
                owner->track(*key, *this);
            }
            return *this;
        }

        ResourceType* get() { return resource; }

    private:
        ResourceManagerBase* owner = nullptr;
        const KeyType* key = nullptr;
        ResourceType* resource = nullptr;
    };

    ResourceHandle get(const Key& key) {
        auto findIt = resourceTable.find(key);
        if (findIt != resourceTable.end()) return ResourceHandle{*this, key, *findIt->second.resource.get()};
        auto newIt =
            resourceTable.emplace(std::make_pair(key, ResourceOwner(this, createResource(key))))
                .first;
        return ResourceHandle{*this, newIt->first, *newIt->second.resource.get()};
    }

private:
    struct ResourceDeleter {
        ResourceDeleter() = default;
        ResourceDeleter(const ResourceDeleter&) = default;
        explicit ResourceDeleter(ResourceManagerBase* owner_) : owner(owner_) {}
        void operator()(ResourceType* resource) { owner->destroyResource(resource); }
        ResourceManagerBase* owner = nullptr;
    };
    struct ResourceOwner {
        ResourceOwner() = default;
        explicit ResourceOwner(ResourceManagerBase* owner_, Resource* resource_)
            : owner{owner_}, resource{resource_, ResourceDeleter{owner_}} {}
        ResourceManagerBase* owner = nullptr;
        std::vector<ResourceHandle*> liveReferences;
        std::unique_ptr<ResourceType, ResourceDeleter> resource;
    };
    using ResourceTable = std::unordered_map<KeyType, ResourceOwner>;

    virtual ResourceType* createResource(const Key& key) = 0;
    virtual void destroyResource(ResourceType* resource) = 0;

    void track(const KeyType& key, ResourceHandle& handle) {
        resourceTable[key].liveReferences.push_back(&handle);
    }
    void untrack(const KeyType* key, ResourceHandle& handle) {
        if (key) {
            auto& refs = resourceTable[*key].liveReferences;
            auto findIt = std::find(std::begin(refs), std::end(refs), &handle);
            assert(findIt != end(refs));
            refs.erase(findIt);
        }
    }

    ResourceTable resourceTable;
};
