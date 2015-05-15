#include "d3dstatemanagers.h"

#include "pipelinestateobject.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

VertexShader::VertexShader(ID3D11Device* device, ID3DBlob* s, const char* name)
    : byteCode{s}, numUniformInfo(0) {
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

class ShaderIncludeHandler : public ID3DInclude {
public:
    STDMETHOD(Open)(D3D_INCLUDE_TYPE /*IncludeType*/, LPCSTR pFileName, LPCVOID /*pParentData*/,
                    LPCVOID* ppData, UINT* pBytes) override {
        ifstream headerFile{pFileName};
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
    ifstream shaderSourceFile{filename};
    stringstream buf;
    buf << shaderSourceFile.rdbuf();
    ID3DBlobPtr compiledShader;
    ID3DBlobPtr errorMessages;
    ShaderIncludeHandler shaderIncludeHandler;
    if (SUCCEEDED(D3DCompile(buf.str().c_str(), buf.str().size(), filename.c_str(), nullptr,
                             &shaderIncludeHandler, "main", target, 0, 0, &compiledShader,
                             &errorMessages))) {
        return new ShaderType{&device, compiledShader, filename.c_str()};
    } else {
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
    if (key.inputElementDescs.empty()) return nullptr; // null InputLayout is valid
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

InputLayoutKey::InputLayoutKey(const InputElementDescs& inputElementDescs_,
                               ID3DBlob* shaderInputSignature_)
    : inputElementDescs{inputElementDescs_},
      shaderInputSignature{shaderInputSignature_},
      hashVal{hashCombine(
          std::hash<InputElementDescs>{}(inputElementDescs),
          hashWithSeed(static_cast<const char*>(shaderInputSignature->GetBufferPointer()),
                       shaderInputSignature->GetBufferSize(), 0))} {}
