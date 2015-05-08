#include "scene.h"

#include <fstream>
#include <vector>

using namespace std;

DataBuffer::DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer, size_t size)
    : Size(size) {
    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.BindFlags = use;
    desc.ByteWidth = (unsigned)size;
    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = buffer;
    sr.SysMemPitch = sr.SysMemSlicePitch = 0;
    ThrowOnFailure(device->CreateBuffer(&desc, buffer ? &sr : nullptr, &D3DBuffer));
    SetDebugObjectName(D3DBuffer, "DataBuffer::D3DBuffer");
}

void DataBuffer::Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size) {
    D3D11_MAPPED_SUBRESOURCE map;
    deviceContext->Map(D3DBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy((void*)map.pData, buffer, size);
    deviceContext->Unmap(D3DBuffer, 0);
}

void Model::AllocateBuffers(ID3D11Device* device) {
    VertexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, &Vertices[0],
                                                Vertices.size() * sizeof(Vertex));
    IndexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, &Indices[0],
                                               Indices.size() * sizeof(uint32_t));
}

void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c) {
    Vector3f Vert[][2] = {
        Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
        Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
        Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
        Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
        Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
        Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
        Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
        Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
        Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
        Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2),
    };

    uint32_t CubeIndices[] = {0,  1,  3,  3,  1,  2,  5,  4,  6,  6,  4,  7,
                              8,  9,  11, 11, 9,  10, 13, 12, 14, 14, 12, 15,
                              16, 17, 19, 19, 17, 18, 21, 20, 22, 22, 20, 23};

    for (auto idx : CubeIndices) AddIndex(idx + static_cast<uint32_t>(Vertices.size()));

    for (int v = 0; v < 24; v++) {
        Vertex vvv;
        vvv.Pos = Vert[v][0];
        vvv.U = Vert[v][1].x;
        vvv.V = Vert[v][1].y;
        float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
        float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
        float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
        int bri = rand() % 160;
        float RRR = c.R * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float GGG = c.G * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float BBB = c.B * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        vvv.C.R = RRR > 255 ? 255 : (unsigned char)RRR;
        vvv.C.G = GGG > 255 ? 255 : (unsigned char)GGG;
        vvv.C.B = BBB > 255 ? 255 : (unsigned char)BBB;
        AddVertex(vvv);
    }
}

void HeightField::AddVertices(ID3D11Device* device) {
    ifstream file(R"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\cdem_dem_150506_001104.dat)", ios::in | ios::binary);
    if (!file) {
        OutputDebugStringA("wtf!");
    }
    file.seekg(0, ios::end);
    auto endPos = file.tellg();
    file.seekg(0);
    auto fileSize = endPos - file.tellg();

    const int width = 1749;
    const int height = 1236;
    vector<uint16_t> heights(width * height);
    file.read(reinterpret_cast<char*>(heights.data()), heights.size() * sizeof(uint16_t));
    auto numRead = file.gcount();
    assert(numRead == fileSize);

    const int blockSize = 64;

    Indices.reserve(Indices.size() + 6 * blockSize * blockSize);
    for (int y = 0; y < blockSize; ++y) {
        for (int x = 0; x < blockSize; ++x) {
            uint32_t baseIdx = y * (blockSize + 1) + x;
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx);
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx + 1);
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx + (blockSize + 1));
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx + 1);
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx + (blockSize + 1) + 1);
            Indices.push_back(static_cast<uint32_t>(Vertices.size()) + baseIdx + (blockSize + 1));
        }
    }
    IndexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, &Indices[0],
        Indices.size() * sizeof(uint32_t));

    Vector3f center(0.0f);
    float gridWidth = 1.0f;
    float gridStep = gridWidth / float(width);
    float gridHeight = float(height) * gridStep;
    float gridElevation = 1.0f;
    float gridElevationScale = 0.00005f;
    for (int y = 0; y < height; y += blockSize) {
        for (int x = 0; x < width; x+= blockSize) {
            Vertices.push_back(vector<Vertex>{});
            Vertices.reserve((blockSize + 1) * (blockSize + 1));
            for (int blockY = 0; blockY <= blockSize; ++blockY) {
                for (int blockX = 0; blockX <= blockSize; ++blockX) {
                        Vertex v;
                        auto localX = x + blockX;
                        auto localY = y + blockY;
                        auto gridHeight = localY < height && localX < width ? heights[localY * width + localX] : 0;
                        v.Pos = Vector3f(localX * gridStep, gridElevation + gridHeight * gridElevationScale, localY * gridStep);
                        Vertices.back().push_back(v);
                }
            }
        }
    }
    for (const auto& vertices : Vertices) {
        VertexBuffers.push_back(std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, vertices.data(),
            vertices.size() * sizeof(Vertex)));
    }
}

Texture::Texture(const char* name_, ID3D11Device* device, ID3D11DeviceContext* deviceContext,
                 Sizei size, int mipLevels, unsigned char* data)
    : name(name_) {
    CD3D11_TEXTURE2D_DESC dsDesc(DXGI_FORMAT_R8G8B8A8_UNORM, size.w, size.h, 1, mipLevels);

    device->CreateTexture2D(&dsDesc, nullptr, &Tex);
    SetDebugObjectName(Tex, string("ImageBuffer::Tex - ") + name);

    device->CreateShaderResourceView(Tex, nullptr, &TexSv);
    SetDebugObjectName(TexSv, string("ImageBuffer::TexSv - ") + name);

    if (data)  // Note data is trashed, as is width and height
    {
        for (int level = 0; level < mipLevels; level++) {
            deviceContext->UpdateSubresource(Tex, level, NULL, data, size.w * 4, size.h * 4);
            for (int j = 0; j < (size.h & ~1); j += 2) {
                const uint8_t* psrc = data + (size.w * j * 4);
                uint8_t* pdest = data + ((size.w >> 1) * (j >> 1) * 4);
                for (int i = 0; i<size.w>> 1; i++, psrc += 8, pdest += 4) {
                    pdest[0] =
                        (((int)psrc[0]) + psrc[4] + psrc[size.w * 4 + 0] + psrc[size.w * 4 + 4]) >>
                        2;
                    pdest[1] =
                        (((int)psrc[1]) + psrc[5] + psrc[size.w * 4 + 1] + psrc[size.w * 4 + 5]) >>
                        2;
                    pdest[2] =
                        (((int)psrc[2]) + psrc[6] + psrc[size.w * 4 + 2] + psrc[size.w * 4 + 6]) >>
                        2;
                    pdest[3] =
                        (((int)psrc[3]) + psrc[7] + psrc[size.w * 4 + 3] + psrc[size.w * 4 + 7]) >>
                        2;
                }
            }
            size.w >>= 1;
            size.h >>= 1;
        }
    }
}

ShaderFill::ShaderFill(ID3D11Device* device, std::unique_ptr<Texture>&& t, bool wrap)
    : OneTexture(std::move(t)) {
    CD3D11_SAMPLER_DESC ss{D3D11_DEFAULT};
    ss.AddressU = ss.AddressV = ss.AddressW =
        wrap ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_BORDER;
    ss.Filter = D3D11_FILTER_ANISOTROPIC;
    ss.MaxAnisotropy = 8;
    device->CreateSamplerState(&ss, &SamplerState);
}

// Simple latency box (keep similar vertex format and shader params same, for ease of code)
Scene::Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int reducedVersion) {
    UniformBufferGen = std::make_unique<DataBuffer>(device, D3D11_BIND_CONSTANT_BUFFER, nullptr,
                                                    2000);  // make sure big enough

    // Construct textures
    const auto texWidthHeight = 256;
    const auto texCount = 5;
    static Model::Color tex_pixels[texCount][texWidthHeight * texWidthHeight];
    std::unique_ptr<ShaderFill> generated_texture[texCount];

    for (int k = 0; k < texCount; ++k) {
        for (int j = 0; j < texWidthHeight; ++j)
            for (int i = 0; i < texWidthHeight; ++i) {
                if (k == 0)
                    tex_pixels[0][j * texWidthHeight + i] =
                        (((i >> 7) ^ (j >> 7)) & 1) ? Model::Color(180, 180, 180, 255)
                                                    : Model::Color(80, 80, 80, 255);  // floor
                if (k == 1)
                    tex_pixels[1][j * texWidthHeight + i] =
                        (((j / 4 & 15) == 0) ||
                         (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0)))
                            ? Model::Color(60, 60, 60, 255)
                            : Model::Color(180, 180, 180, 255);  // wall
                if (k == 2 || k == 4)
                    tex_pixels[k][j * texWidthHeight + i] =
                        (i / 4 == 0 || j / 4 == 0) ? Model::Color(80, 80, 80, 255)
                                                   : Model::Color(180, 180, 180, 255);  // ceiling
                if (k == 3)
                    tex_pixels[3][j * texWidthHeight + i] =
                        Model::Color(128, 128, 128, 255);  // blank
            }
        std::unique_ptr<Texture> t = std::make_unique<Texture>(
            "generated_texture", device, deviceContext, Sizei(texWidthHeight, texWidthHeight), 8,
            (unsigned char*)tex_pixels[k]);
        generated_texture[k] = std::make_unique<ShaderFill>(device, std::move(t));
    }

    // Construct geometry
    std::unique_ptr<Model> m = make_unique<Model>(Vector3f(0, 0, 0), std::move(generated_texture[2]));  // Moving box
    m->AddSolidColorBox(0, 0, 0, +1.0f, +1.0f, 1.0f, Model::Color(64, 64, 64));
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vector3f(0, 0, 0), std::move(generated_texture[1])));  // Walls
    m->AddSolidColorBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Left Wall
    m->AddSolidColorBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f,
                        Model::Color(128, 128, 128));  // Back Wall
    m->AddSolidColorBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Right Wall
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vector3f(0, 0, 0), std::move(generated_texture[0])));  // Floors
    m->AddSolidColorBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f,
                        Model::Color(128, 128, 128));  // Main floor
    m->AddSolidColorBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f,
                        Model::Color(128, 128, 128));  // Bottom floor
    m->AllocateBuffers(device);
    Add(move(m));

    if (reducedVersion) return;

    m.reset(new Model(Vector3f(0, 0, 0), std::move(generated_texture[4])));  // Ceiling
    m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, Model::Color(128, 128, 128));
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vector3f(0, 0, 0), std::move(generated_texture[3])));  // Fixtures & furniture
    m->AddSolidColorBox(9.5f, 0.75f, 3.0f, 10.1f, 2.5f, 3.1f,
                        Model::Color(96, 96, 96));  // Right side shelf// Verticals
    m->AddSolidColorBox(9.5f, 0.95f, 3.7f, 10.1f, 2.75f, 3.8f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(9.55f, 1.20f, 2.5f, 10.1f, 1.30f, 3.75f,
                        Model::Color(96, 96, 96));  // Right side shelf// Horizontals
    m->AddSolidColorBox(9.55f, 2.00f, 3.05f, 10.1f, 2.10f, 4.2f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(5.0f, 1.1f, 20.0f, 10.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Right railing
    m->AddSolidColorBox(-10.0f, 1.1f, 20.0f, -5.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Left railing
    for (float f = 5.0f; f <= 9.0f; f += 1.0f) {
        m->AddSolidColorBox(f, 0.0f, 20.0f, f + 0.1f, 1.1f, 20.1f,
                            Model::Color(128, 128, 128));  // Left Bars
        m->AddSolidColorBox(-f, 1.1f, 20.0f, -f - 0.1f, 0.0f, 20.1f,
                            Model::Color(128, 128, 128));  // Right Bars
    }
    m->AddSolidColorBox(-1.8f, 0.8f, 1.0f, 0.0f, 0.7f, 0.0f, Model::Color(128, 128, 0));  // Table
    m->AddSolidColorBox(-1.8f, 0.0f, 0.0f, -1.7f, 0.7f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.8f, 0.7f, 1.0f, -1.7f, 0.0f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.0f, 1.0f, -0.1f, 0.7f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.7f, 0.0f, -0.1f, 0.0f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.4f, 0.5f, -1.1f, -0.8f, 0.55f, -0.5f,
                        Model::Color(44, 44, 128));  // Chair Set
    m->AddSolidColorBox(-1.4f, 0.0f, -1.1f, -1.34f, 1.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 1
    m->AddSolidColorBox(-1.4f, 0.5f, -0.5f, -1.34f, 0.0f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 0.0f, -0.5f, -0.86f, 0.5f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 1.0f, -1.1f, -0.86f, 0.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-1.4f, 0.97f, -1.05f, -0.8f, 0.92f, -1.10f,
                        Model::Color(44, 44, 128));  // Chair Back high bar

    for (float f = 3.0f; f <= 6.6f; f += 0.4f)
        m->AddSolidColorBox(-3, 0.0f, f, -2.9f, 1.3f, f + 0.1f, Model::Color(64, 64, 64));  // Posts

    m->AllocateBuffers(device);

    Add(move(m));

    // Terrain
    heightField = make_unique<HeightField>(Vector3f(0.0f), device);
    heightField->AddVertices(device);
}

void HeightField::Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase, DataBuffer* uniformBuffer) {
    VertexShader* VShader = shaderDatabase.GetVertexShader("terrainvs.hlsl");
    uniformBuffer->Refresh(context, VShader->UniformData.data(), VShader->UniformData.size());
    ID3D11Buffer* vsConstantBuffers[] = { uniformBuffer->D3DBuffer };
    context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    const vector<D3D11_INPUT_ELEMENT_DESC> modelVertexDesc{
        D3D11_INPUT_ELEMENT_DESC{ "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
        offsetof(Vertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    context->IASetInputLayout(shaderDatabase.GetInputLayout(VShader, modelVertexDesc));
    context->IASetIndexBuffer(IndexBuffer->D3DBuffer, DXGI_FORMAT_R32_UINT, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(VShader->D3DVert, NULL, 0);

    PixelShader* pixelShader = shaderDatabase.GetPixelShader("terrainps.hlsl");
    context->PSSetShader(pixelShader->D3DPix, NULL, 0);
    
    for (const auto& vertexBuffer : VertexBuffers) {
        ID3D11Buffer* vertexBuffers[] = { vertexBuffer->D3DBuffer };
        const UINT strides[] = { sizeof(Vertex) };
        const UINT offsets[] = { 0 };
        context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
        context->DrawIndexed(Indices.size(), 0, 0);
    }
}

void Scene::Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase, ShaderFill* fill,
                   DataBuffer* vertices, DataBuffer* indices, UINT stride, int count) {
    UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = {vertices->D3DBuffer};
    context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);

    VertexShader* VShader = shaderDatabase.GetVertexShader("simplevs.hlsl");
    UniformBufferGen->Refresh(context, VShader->UniformData.data(), VShader->UniformData.size());
    ID3D11Buffer* vsConstantBuffers[] = {UniformBufferGen->D3DBuffer};
    context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    const vector<D3D11_INPUT_ELEMENT_DESC> modelVertexDesc{
        D3D11_INPUT_ELEMENT_DESC{"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                                 offsetof(Model::Vertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
                                 offsetof(Model::Vertex, C), D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                                 offsetof(Model::Vertex, U), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    context->IASetInputLayout(shaderDatabase.GetInputLayout(VShader, modelVertexDesc));
    context->IASetIndexBuffer(indices->D3DBuffer, DXGI_FORMAT_R32_UINT, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(VShader->D3DVert, NULL, 0);

    PixelShader* pixelShader = shaderDatabase.GetPixelShader("simpleps.hlsl");
    context->PSSetShader(pixelShader->D3DPix, NULL, 0);
    ID3D11SamplerState* samplerStates[] = {fill->SamplerState};
    context->PSSetSamplers(0, 1, samplerStates);

    if (fill && fill->OneTexture) {
        ID3D11ShaderResourceView* srvs[] = {fill->OneTexture->TexSv};
        context->PSSetShaderResources(0, 1, srvs);
    }
    else {
        ID3D11ShaderResourceView* srvs[] = { nullptr };
        context->PSSetShaderResources(0, 1, srvs);
    }
    context->DrawIndexed(count, 0, 0);
}

void Scene::Render(DirectX11& dx11, Matrix4f view, Matrix4f proj) {
    view.Transpose();
    proj.Transpose();
    for (auto& model : Models) {
        VertexShader* VShader = dx11.shaderDatabase.GetVertexShader("simplevs.hlsl");
        VShader->SetUniform("World", 16, &model->GetMatrix().Transposed().M[0][0]);
        VShader->SetUniform("View", 16, &view.M[0][0]);
        VShader->SetUniform("Proj", 16, &proj.M[0][0]);

        Render(dx11.Context, dx11.shaderDatabase, model->Fill.get(), model->VertexBuffer.get(),
               model->IndexBuffer.get(), sizeof(Model::Vertex), model->Indices.size());
    }

    VertexShader* VShader = dx11.shaderDatabase.GetVertexShader("terrainvs.hlsl");
    VShader->SetUniform("World", 16, &heightField->GetMatrix().Transposed().M[0][0]);
    VShader->SetUniform("View", 16, &view.M[0][0]);
    VShader->SetUniform("Proj", 16, &proj.M[0][0]);

    heightField->Render(dx11.Context, dx11.shaderDatabase, UniformBufferGen.get());
}
