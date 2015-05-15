#include "d3dresourcemanagers.h"

#include "DDSTextureLoader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace std;

VertexShader::VertexShader(ID3D11Device* device, ID3DBlob* s, const char* name)
    : byteCode{ s }, numUniformInfo(0) {
    device->CreateVertexShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DVert);
    SetDebugObjectName(D3DVert, name);

    D3DGetBlobPart(s->GetBufferPointer(), s->GetBufferSize(), D3D_BLOB_INPUT_SIGNATURE_BLOB, 0,
        &inputSignature);

    ID3D11ShaderReflectionPtr ref;
    D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref);
    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd;
    if (FAILED(buf->GetDesc(&bufd))) return;

    for (unsigned i = 0; i < bufd.Variables; i++) {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC vd;
        var->GetDesc(&vd);
        Uniform u;
        strcpy_s(u.Name, (const char*)vd.Name);
        ;
        u.Offset = vd.StartOffset;
        u.Size = vd.Size;
        UniformInfo[numUniformInfo++] = u;
    }
    UniformData.resize(bufd.Size);
}

void VertexShader::SetUniform(const char* name, int n, const float* v) {
    for (int i = 0; i < numUniformInfo; ++i) {
        if (!strcmp(UniformInfo[i].Name, name)) {
            memcpy(UniformData.data() + UniformInfo[i].Offset, v, n * sizeof(float));
            return;
        }
    }
}

PixelShader::PixelShader(ID3D11Device* device, ID3DBlob* s, const char* name) : numUniformInfo(0) {
    device->CreatePixelShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DPix);
    SetDebugObjectName(D3DPix, name);

    ID3D11ShaderReflectionPtr ref;
    D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref);
    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd;
    if (FAILED(buf->GetDesc(&bufd))) return;

    for (unsigned i = 0; i < bufd.Variables; i++) {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC vd;
        var->GetDesc(&vd);
        Uniform u;
        strcpy_s(u.Name, (const char*)vd.Name);
        ;
        u.Offset = vd.StartOffset;
        u.Size = vd.Size;
        UniformInfo[numUniformInfo++] = u;
    }
    UniformData.resize(bufd.Size);
}

void PixelShader::SetUniform(const char* name, int n, const float* v) {
    for (int i = 0; i < numUniformInfo; ++i) {
        if (!strcmp(UniformInfo[i].Name, name)) {
            memcpy(UniformData.data() + UniformInfo[i].Offset, v, n * sizeof(float));
            return;
        }
    }
}

RasterizerStateManager::ResourceType* RasterizerStateManager::createResource(
    const RasterizerStateManager::KeyType& key) {
    ID3D11RasterizerState* rs = nullptr;
    ThrowOnFailure(device->CreateRasterizerState(&key, &rs));
    return rs;
}

Texture2DManager::ResourceType* Texture2DManager::createResource(
    const Texture2DManager::KeyType& key) {
    auto tex = ID3D11ResourcePtr{};
    ID3D11ShaderResourceView* srv = nullptr;
    ThrowOnFailure(DirectX::CreateDDSTextureFromFileEx(
        device, wstring(begin(key), end(key)).c_str(), 0, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE, 0, 0, true, &tex, &srv));
    return srv;
}

class ShaderIncludeHandler : public ID3DInclude {
public:
    STDMETHOD(Open)(D3D_INCLUDE_TYPE /*IncludeType*/, LPCSTR pFileName, LPCVOID /*pParentData*/,
        LPCVOID* ppData, UINT* pBytes) override {
        ifstream headerFile{ pFileName };
        if (!headerFile) return E_INVALIDARG;
        auto buf = stringstream{};
        buf << headerFile.rdbuf();
        auto str = buf.str();
        *ppData = str.c_str();
        *pBytes = str.size();
        bufs_[*ppData] = move(str);
        return S_OK;
    }
    STDMETHOD(Close)(LPCVOID pData) override {
        auto findIt = bufs_.find(pData);
        if (findIt == bufs_.end()) return E_INVALIDARG;
        bufs_.erase(findIt);
        return S_OK;
    }

private:
    std::unordered_map<LPCVOID, string> bufs_;
};

template <typename ShaderType>
ShaderType* loadShader(ID3D11Device& device, const std::string& filename, const char* target) {
    ifstream shaderSourceFile{ filename };
    stringstream buf;
    buf << shaderSourceFile.rdbuf();
    ID3DBlobPtr compiledShader;
    ID3DBlobPtr errorMessages;
    ShaderIncludeHandler shaderIncludeHandler;
    if (SUCCEEDED(D3DCompile(buf.str().c_str(), buf.str().size(), filename.c_str(), nullptr,
        &shaderIncludeHandler, "main", target, 0, 0, &compiledShader,
        &errorMessages))) {
        return new ShaderType{ &device, compiledShader, filename.c_str() };
    }
    else {
        OutputDebugStringA(static_cast<const char*>(errorMessages->GetBufferPointer()));
        return nullptr;
    }
}

VertexShaderManager::ResourceType* VertexShaderManager::createResource(const KeyType& key) {
    return loadShader<VertexShader>(device, key, "vs_5_0");
}

PixelShaderManager::ResourceType* PixelShaderManager::createResource(const KeyType& key) {
    return loadShader<PixelShader>(device, key, "ps_5_0");
}

InputLayoutManager::ResourceType* InputLayoutManager::createResource(const KeyType& key) {
    ID3D11InputLayout* inputLayout = nullptr;
    device->CreateInputLayout(key.inputElementDescs.data(), key.inputElementDescs.size(),
        key.shaderInputSignature->GetBufferPointer(),
        key.shaderInputSignature->GetBufferSize(), &inputLayout);
    return inputLayout;
}

BlendStateManager::ResourceType* BlendStateManager::createResource(const KeyType& key) {
    ID3D11BlendState* blendState = nullptr;
    ThrowOnFailure(device->CreateBlendState(&key, &blendState));
    return blendState;
}

DepthStencilStateManager::ResourceType* DepthStencilStateManager::createResource(
    const KeyType& key) {
    ID3D11DepthStencilState* depthStencilState = nullptr;
    ThrowOnFailure(device->CreateDepthStencilState(&key, &depthStencilState));
    return depthStencilState;
}

PipelineStateObjectManager::ResourceType* PipelineStateObjectManager::createResource(
    const KeyType& key) {
    PipelineStateObject* pso = new PipelineStateObject{};
    pso->vertexShader = stateManagers.vertexShaderManager.get(key.vertexShader);
    pso->pixelShader = stateManagers.pixelShaderManager.get(key.pixelShader);
    pso->blendState = stateManagers.blendStateManager.get(key.blendState);
    pso->rasterizerState = stateManagers.rasterizerStateManager.get(key.rasterizerState);
    pso->depthStencilState = stateManagers.depthStencilStateManager.get(key.depthStencilState);
    pso->inputLayout = stateManagers.inputLayoutManager.get(
        InputLayoutKey{ key.inputElementDescs, pso->vertexShader.get()->inputSignature });
    pso->primitiveTopology = key.primitiveTopology;
    return pso;
}

size_t hashHelper(const PipelineStateObjectDesc & x) {
    return hashCombine(hash<VertexShaderManager::KeyType>{}(x.vertexShader),
        hash<PixelShaderManager::KeyType>{}(x.pixelShader),
        hash<BlendStateManager::KeyType>{}(x.blendState),
        hash<RasterizerStateManager::KeyType>{}(x.rasterizerState),
        hash<DepthStencilStateManager::KeyType>{}(x.depthStencilState),
        hash<InputElementDescs>{}(x.inputElementDescs), x.primitiveTopology);
}

bool operator==(const PipelineStateObjectDesc& a, const PipelineStateObjectDesc& b) {
    auto tupleify = [](const PipelineStateObjectDesc& a) {
        return std::make_tuple(a.vertexShader, a.pixelShader, a.blendState, a.rasterizerState,
            a.depthStencilState, a.inputElementDescs, a.primitiveTopology);
    };
    return tupleify(a) == tupleify(b);
}

InputLayoutKey::InputLayoutKey(const InputElementDescs& inputElementDescs_,
                               ID3DBlob* shaderInputSignature_)
    : inputElementDescs{inputElementDescs_},
      shaderInputSignature{shaderInputSignature_},
      hashVal{hashCombine(
          std::hash<InputElementDescs>{}(inputElementDescs),
          hashWithSeed(static_cast<const char*>(shaderInputSignature->GetBufferPointer()),
                       shaderInputSignature->GetBufferSize(), 0))} {}
