#include "d3dresourcemanagers.h"

#include "DDSTextureLoader.h"
#include "pipelinestateobject.h"

#include <string>

using namespace std;

Texture2DManager::ResourceType* Texture2DManager::createResource(
    const Texture2DManager::KeyType& key) {
    auto tex = ID3D11ResourcePtr{};
    ID3D11ShaderResourceView* srv = nullptr;
    ThrowOnFailure(DirectX::CreateDDSTextureFromFileEx(
        device.Get(), wstring(begin(key), end(key)).c_str(), 0, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE, 0, 0, true, &tex, &srv));
    return srv;
}

