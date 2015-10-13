#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

// The ResourceManager system is intended to address a few major pieces of functionality:
// 1. ) Ensure there is only one unique instance in memory of each unique resource.
// 2. ) Enable hotloading of assets with automatic updates to any references.
// 3. ) Manage resource lifetimes.
// Later...
// 4. ) Would be nice to use the system to support overriding resources (e.g. enable wireframe by
// going through all the rasterizer states in the corresponding manager and replacing them with a
// version with wireframe enabled). Some complexity here though - is it possible to make it generic?
//
// The intent is that the ResourceManager will be thread safe and able to perform most of the heavy
// lifting of managing GPU resource lifetimes under D3D12 (where e.g. a resource cannot be freed
// immediately when it is no longer referenced from game code but must be kept alive until we know
// the GPU will no longer reference it which may be one or more frames later). Currently it is not
// thread safe however and in particular hot loading will need some careful consideration to make
// thread safe.
//
// Design Overview
//
// There is a ResourceManager per resource type. It seems unnecessary to try and cram all resources
// under a common interface of one uber resource manager. Generally client code will know exactly
// which type of resource it wants so there is no need to have a single global resource manager.
//
// Resource uniqueness is ensured by hashing the key used to look up a particular resource type.
// This key will often be a string (currently corresponding to a file name) but may be any hashable
// type. Some utility functions for hashing types that don't already provide an std::hash
// implementation are provided. In the case of some resources such as D3D11 render state objects the
// key is the descriptor struct that defines the render state for example.
//
// Hotloading is supported through the most complex aspect of the current implementation. Each entry
// in a ResourceManager resource table keeps a list of pointers to all currently live
// ResourceHandles for it's resource type. When a resource is hotloaded we can then go through and
// update all the referencing handles. The idea behind this system is that hotloading is a
// relatively rare operation and we don't want to pay any extra indirection cost when referencing
// resources under normal conditions in order to support it (e.g. by looking up a resource every
// time we want to use it). Instead we pay a higher cost when hotloading to update all references
// since performance is not a major issue in this situation. You can think of it as an event rather
// than polling model.
//
// Hotloading will be more complex when the resource manager is made thread safe. Details still need
// to be worked out there for how to handle safely updating resource pointers in arbitrary threads.
// We may be ok with relying on atomic pointer writes to do the update and just lock the internal
// structures. Since the resource manager should be accessed relatively infrequently (only when
// requesting a new resource) the overhead of locking may not be too bad.
//
// Currently resource lifetime management is braindead - we just free everything when the manager is
// destroyed - but it should be simple to extend the system to support reference counting since we
// already track that information.

template <typename Key, typename Resource, typename ResourceDeleter = std::default_delete<Resource>>
class ResourceManagerBase {
public:
    using KeyType = Key;
    using ResourceType = Resource;
    using ResourceManagerBaseType = ResourceManagerBase<KeyType, ResourceType>;

    ResourceManagerBase() = default;
    ResourceManagerBase(const ResourceManagerBase&) = delete;
    ResourceManagerBase& operator=(const ResourceManagerBase&) = delete;

    // ResourceHandles are tracking and (will be) threadsafe which makes them fairly expensive to
    // copy so you don't want to pass them around by value. The system is designed so that a
    // resource is not freed for 2 frames after the last ResourceHandle goes away so it is safe to
    // pass raw Resources around within a frame. For this reason it is not possible to copy
    // construct or assign a ResourceHandle.
    friend class ResourceHandle;
    class ResourceHandle {
    public:
        ResourceHandle() = default;
        ResourceHandle(const ResourceHandle&) = delete;
        ResourceHandle(ResourceHandle&& x) : owner{x.owner}, key{x.key}, resource{x.resource} {
            owner->track(*key, *this);
        }
        ResourceHandle(ResourceManagerBase& owner_, const KeyType& key_, ResourceType& resource_)
            : owner{&owner_}, key{&key_}, resource{&resource_} {
            // This could be optimized since generally we just inserted the resource so no need to
            // look it up again. It would complicate the code though.
            owner->track(*key, *this);
        }
        ~ResourceHandle() { owner->untrack(key, *this); }

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
        friend class ResourceManagerBaseType;
        ResourceManagerBase* owner = nullptr;
        const KeyType* key = nullptr;
        ResourceType* resource = nullptr;
    };

    ResourceHandle get(const Key& key) {
        auto findIt = resourceTable.find(key);
        if (findIt != resourceTable.end())
            return ResourceHandle{*this, findIt->first, *findIt->second.resource.get()};
        auto newIt =
            resourceTable.emplace(std::make_pair(key, ResourceOwner(this, createResource(key))))
                .first;
        return ResourceHandle{*this, newIt->first, *newIt->second.resource.get()};
    }

    void recreate(const Key& key) {
        auto findIt = resourceTable.find(key);
        if (findIt != resourceTable.end()) recreate(*findIt);
    }

    void recreateAll() {
        for (auto& e : resourceTable) {
            recreate(e);
        }
    }

protected:
    virtual ~ResourceManagerBase() {
        assert(std::all_of(begin(resourceTable), end(resourceTable),
                           [](const auto& e) { return e.second.liveReferences.empty(); }) &&
               ("All resource handles tracked by a ResourceManager should be destroyed before "
                "it is destroyed."));
    }

private:
    struct ResourceOwner {
        ResourceOwner() = default;
        explicit ResourceOwner(ResourceManagerBase* owner_, Resource* resource_)
            : owner{owner_}, resource{resource_} {}
        ResourceManagerBase* owner = nullptr;
        std::vector<ResourceHandle*> liveReferences;
        std::unique_ptr<ResourceType, ResourceDeleter> resource;
    };
    using ResourceTable = std::unordered_map<KeyType, ResourceOwner>;

    virtual ResourceType* createResource(const Key& key) = 0;

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

    void recreate(typename ResourceTable::value_type& entry) {
        auto recreated = createResource(entry.first);
        auto& resourceOwner = entry.second;
        if (recreated) {
            resourceOwner.resource.reset(recreated);
            for (auto& ref : resourceOwner.liveReferences) {
                ref->resource = recreated;
            }
        }
    }

    ResourceTable resourceTable;
};
