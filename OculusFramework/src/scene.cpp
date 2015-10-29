#include "scene.h"

#include "pipelinestateobject.h"
#include "util.h"

#include <iterator>
#include <vector>

#include "DDSTextureLoader.h"

#include "imgui/imgui.h"

#include "hlslmacros.h"
#include "../commonstructs.hlsli"

using namespace std;

using namespace mathlib;

void Model::AllocateBuffers(ID3D11Device* device) {
    VertexBuffer = CreateBuffer(
        device, BufferDesc{Vertices.size() * sizeof(Vertices[0]), D3D11_BIND_VERTEX_BUFFER},
        {Vertices.data()});
    IndexBuffer = CreateBuffer(
        device, BufferDesc{Indices.size() * sizeof(Indices[0]), D3D11_BIND_INDEX_BUFFER},
        {Indices.data()});

    objectConstantBuffer = CreateBuffer(
        device, BufferDesc{roundUpConstantBufferSize(sizeof(Object)), D3D11_BIND_CONSTANT_BUFFER,
                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE});
}

void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c) {
    Vec3f Vert[][2] = {
        {{x1, y2, z1}, {z1, x1, 0.0f}}, {{x2, y2, z1}, {z1, x2, 0.0f}},
        {{x2, y2, z2}, {z2, x2, 0.0f}}, {{x1, y2, z2}, {z2, x1, 0.0f}},
        {{x1, y1, z1}, {z1, x1, 0.0f}}, {{x2, y1, z1}, {z1, x2, 0.0f}},
        {{x2, y1, z2}, {z2, x2, 0.0f}}, {{x1, y1, z2}, {z2, x1, 0.0f}},
        {{x1, y1, z2}, {z2, y1, 0.0f}}, {{x1, y1, z1}, {z1, y1, 0.0f}},
        {{x1, y2, z1}, {z1, y2, 0.0f}}, {{x1, y2, z2}, {z2, y2, 0.0f}},
        {{x2, y1, z2}, {z2, y1, 0.0f}}, {{x2, y1, z1}, {z1, y1, 0.0f}},
        {{x2, y2, z1}, {z1, y2, 0.0f}}, {{x2, y2, z2}, {z2, y2, 0.0f}},
        {{x1, y1, z1}, {x1, y1, 0.0f}}, {{x2, y1, z1}, {x2, y1, 0.0f}},
        {{x2, y2, z1}, {x2, y2, 0.0f}}, {{x1, y2, z1}, {x1, y2, 0.0f}},
        {{x1, y1, z2}, {x1, y1, 0.0f}}, {{x2, y1, z2}, {x2, y1, 0.0f}},
        {{x2, y2, z2}, {x2, y2, 0.0f}}, {{x1, y2, z2}, {x1, y2, 0.0f}},
    };

    uint32_t CubeIndices[] = {0,  1,  3,  3,  1,  2,  5,  4,  6,  6,  4,  7,
                              8,  9,  11, 11, 9,  10, 13, 12, 14, 14, 12, 15,
                              16, 17, 19, 19, 17, 18, 21, 20, 22, 22, 20, 23};

    for (auto idx : CubeIndices) AddIndex(static_cast<uint16_t>(idx + Vertices.size()));

    for (const auto& v : Vert) {
        Vertex vvv{v[0], {}, v[1].xy()};
        float dist1 = magnitude(vvv.position - Vec3f{-2.0f, 4.0f, -2.0f});
        float dist2 = magnitude(vvv.position - Vec3f{3.0f, 4.0f, -3.0f});
        float dist3 = magnitude(vvv.position - Vec3f{-4.0f, 3.0f, 25.0f});
        int bri = rand() % 160;
        float RRR = c.R * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float GGG = c.G * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float BBB = c.B * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        vvv.color.R = RRR > 255 ? 255 : (unsigned char)RRR;
        vvv.color.G = GGG > 255 ? 255 : (unsigned char)GGG;
        vvv.color.B = BBB > 255 ? 255 : (unsigned char)BBB;
        AddVertex(vvv);
    }
}

Texture::Texture(const char* name_, ID3D11Device* device, ID3D11DeviceContext* deviceContext,
                 ovrSizei size, int mipLevels, unsigned char* data)
    : name(name_) {
    Tex = CreateTexture2D(
        device, Texture2DDesc(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, size.w, size.h).mipLevels(mipLevels),
        "ImageBuffer::Tex - "s + name);

    TexSv = CreateShaderResourceView(device, Tex.Get(), "ImageBuffer::TexSv - "s + name);

    if (data)  // Note data is trashed, as is width and height
    {
        for (int level = 0; level < mipLevels; level++) {
            deviceContext->UpdateSubresource(Tex.Get(), level, NULL, data, size.w * 4, size.h * 4);
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

ShaderFill::ShaderFill(std::unique_ptr<Texture>&& t)
    : OneTexture(std::move(t)) {
}

template <>
inline DXGI_FORMAT getDXGIFormat<Model::Color>() {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

Scene::Scene(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* deviceContext,
             PipelineStateObjectManager& pipelineStateObjectManager,
             Texture2DManager& texture2DManager) {
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
            "generated_texture", device, deviceContext, ovrSizei{ texWidthHeight, texWidthHeight }, 8,
            (unsigned char*)tex_pixels[k]);
        generated_texture[k] = std::make_unique<ShaderFill>(std::move(t));
    }

    // Construct geometry
    std::unique_ptr<Model> m =
        make_unique<Model>(Vec3f{}, std::move(generated_texture[2]));  // Moving box
    m->AddSolidColorBox(0, 0, 0, +1.0f, +1.0f, 1.0f, Model::Color(64, 64, 64));
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vec3f{}, std::move(generated_texture[1])));  // Walls
    m->AddSolidColorBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Left Wall
    m->AddSolidColorBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f,
                        Model::Color(128, 128, 128));  // Back Wall
    m->AddSolidColorBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Right Wall
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vec3f{}, std::move(generated_texture[0])));  // Floors
    m->AddSolidColorBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f,
                        Model::Color(128, 128, 128));  // Main floor
    m->AddSolidColorBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f,
                        Model::Color(128, 128, 128));  // Bottom floor
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vec3f{}, std::move(generated_texture[4])));  // Ceiling
    m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, Model::Color(128, 128, 128));
    m->AllocateBuffers(device);
    Add(move(m));

    m.reset(new Model(Vec3f{}, std::move(generated_texture[3])));  // Fixtures & furniture
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
    heightField = make_unique<HeightField>(Vec3f{-1.0f, 0.8f, 0.0f});
    heightField->AddVertices(dx11, device, deviceContext, pipelineStateObjectManager, texture2DManager);

    sphere = make_unique<Sphere>();
    sphere->GenerateVerts(*device, pipelineStateObjectManager);

    PipelineStateObjectDesc desc;
    desc.vertexShader = "simplevs.hlsl";
    desc.pixelShader = "simpleps.hlsl";
    desc.inputElementDescs = {
        MAKE_INPUT_ELEMENT_DESC(Model::Vertex, position),
        MAKE_INPUT_ELEMENT_DESC(Model::Vertex, color),
        MAKE_INPUT_ELEMENT_DESC(Model::Vertex, texcoord),
    };
    pipelineStateObject = pipelineStateObjectManager.get(desc);

    cameraConstantBuffer = CreateBuffer(
        device, BufferDesc{roundUpConstantBufferSize(sizeof(Camera)), D3D11_BIND_CONSTANT_BUFFER,
                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE},
        "Scene::cameraConstantBuffer");

    lightingConstantBuffer = CreateBuffer(
        device, BufferDesc{roundUpConstantBufferSize(sizeof(Lighting)), D3D11_BIND_CONSTANT_BUFFER,
                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE},
        "Scene::lightingConstantBuffer");

    ThrowOnFailure(DirectX::CreateDDSTextureFromFile(device, LR"(data\rnl_cube_pmrem.dds)",
                                                     &pmremEnvMapTex, &pmremEnvMapSRV));
    ThrowOnFailure(DirectX::CreateDDSTextureFromFile(device, LR"(data\rnl_cube_irrad.dds)",
                                                     &irradEnvMapTex, &irradEnvMapSRV));
    [this, device] {
        const auto desc = SamplerDesc{};
        ThrowOnFailure(device->CreateSamplerState(&desc, &linearSampler));
    }();
    [this, device] {
        const auto desc = SamplerDesc{}
                              .filter(D3D11_FILTER_ANISOTROPIC)
                              .address(D3D11_TEXTURE_ADDRESS_WRAP)
                              .maxAnisotropy(8);
        ThrowOnFailure(device->CreateSamplerState(&desc, &standardTextureSampler));
    }();

    lighting.lightPos = {0.0f, 3.7f, -2.0f, 1.0f};
    lighting.lightColor = Vec4f{2.0f};
    lighting.lightAmbient = Vec4f{2.0f};
}

void Scene::showGui() {
    if (ImGui::CollapsingHeader("Scene")) {
        static auto lightColor = Vec4f{1.0f};
        ImGui::ColorEdit3("Light color", lightColor.data());
        static float lightIntensity = 10.0f;
        ImGui::SliderFloat("Light intensity", &lightIntensity, 0.0f, 16.0f, "intensity = %.3f");
        lighting.lightColor = lightColor * lightIntensity;
        ImGui::DragFloat3("Light position", lighting.lightPos.data(), 0.1f);
    }
    heightField->showGui();
}

void Scene::Render(ID3D11DeviceContext* context, ShaderFill* fill, ID3D11Buffer* vertices,
                   ID3D11Buffer* indices, UINT stride, int count, ID3D11Buffer& objectConstantBuffer) {
    IASetVertexBuffers(context, 0, {vertices}, {stride});
    VSSetConstantBuffers(context, objectConstantBufferOffset, {&objectConstantBuffer});

    context->IASetInputLayout(pipelineStateObject.get()->inputLayout.get());
    context->IASetIndexBuffer(indices, DXGI_FORMAT_R16_UINT, 0);

    if (fill && fill->OneTexture) {
        PSSetShaderResources(context, materialSRVOffset,
                             {fill->OneTexture->TexSv.Get()});
    } else {
        PSSetShaderResources(context, materialSRVOffset, {nullptr});
    }
    context->DrawIndexed(count, 0, 0);
}

void Scene::Render(DirectX11& dx11, const mathlib::Vec3f& eye, const mathlib::Mat4f& view,
                   const mathlib::Mat4f& proj) {
    dx11.applyState(*dx11.Context.Get(), *pipelineStateObject.get());

    PSSetSamplers(dx11.Context, 0,
                  {linearSampler.Get(), standardTextureSampler.Get()});

    Camera camera;
    camera.proj = proj;
    camera.view = view;
    camera.eye = eye;

    [this, &camera, &dx11] {
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        dx11.Context->Map(cameraConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                          &mappedResource);
        memcpy(mappedResource.pData, &camera, sizeof(camera));
        dx11.Context->Unmap(cameraConstantBuffer.Get(), 0);
    }();

    [this, &dx11] {
        auto mapHandle = MapHandle{dx11.Context.Get(), lightingConstantBuffer.Get()};
        memcpy(mapHandle.mappedSubresource().pData, &lighting, sizeof(lighting));
    }();

    VSSetConstantBuffers(dx11.Context, 0, {cameraConstantBuffer.Get()});

    PSSetConstantBuffers(dx11.Context, 2, {lightingConstantBuffer.Get()});

    PSSetShaderResources(dx11.Context, 0,
                         {pmremEnvMapSRV.Get(), irradEnvMapSRV.Get()});

    for (auto& model : Models) {
        Object object;
        object.world = model->GetMatrix();

        [this, &object, &dx11, &model] {
            auto mapHandle = MapHandle{dx11.Context.Get(), model->objectConstantBuffer.Get()};
            memcpy(mapHandle.mappedSubresource().pData, &object, sizeof(object));
        }();

        Render(dx11.Context.Get(), model->Fill.get(), model->VertexBuffer.Get(),
               model->IndexBuffer.Get(), sizeof(Model::Vertex), model->Indices.size(),
               *model->objectConstantBuffer.Get());
    }

    heightField->Render(dx11, dx11.Context.Get());
    sphere->Render(dx11, dx11.Context.Get());
}
