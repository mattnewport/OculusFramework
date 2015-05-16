#pragma once

#include "d3dhelper.h"
#include "resourcemanager.h"

template <typename T>
struct D3DObjectDeleter {
    void operator()(T* t) { t->Release(); }
};

template <typename Key, typename Resource>
class D3DObjectManagerBase : public ResourceManagerBase<Key, Resource, D3DObjectDeleter<Resource>> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }

protected:
    ID3D11DevicePtr device;
};

using Texture2DKey = std::string;
class Texture2DManager : public D3DObjectManagerBase<Texture2DKey, ID3D11ShaderResourceView> {
    ResourceType* createResource(const KeyType& key) override;
};

