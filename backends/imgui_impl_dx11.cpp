#ifndef IMGUI_DISABLE

    #include "imgui.h"
    #include "imgui_common_tools.h"
    #include "imgui_image_render.h"
    #include "imgui_impl_dx11.h"

    // DirectX
    #include <stdio.h>
    #include <d3d11.h>
    #include <d3dcompiler.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
    #endif

using namespace ImGui;

// DirectX11 data
struct ImGui_ImplDX11_Data
{
    ID3D11Device             *pd3dDevice;
    ID3D11DeviceContext      *pd3dDeviceContext;
    IDXGIFactory             *pFactory;
    ID3D11Buffer             *pVB;
    ID3D11Buffer             *pIB;
    ID3D11VertexShader       *pVertexShader;
    ID3D11InputLayout        *pInputLayout;
    ID3D11Buffer             *pVertexConstantBuffer;
    ID3D11PixelShader        *pPixelShader;
    ID3D11SamplerState       *pFontSampler;
    ID3D11ShaderResourceView *pFontTextureView;
    RenderSource              fontRenderSource;
    ID3D11RasterizerState    *pRasterizerState;
    ID3D11BlendState         *pBlendState;
    ID3D11DepthStencilState  *pDepthStencilState;
    int                       VertexBufferSize;
    int                       IndexBufferSize;

    ID3D11Buffer       *pPixelConstantBuffer;
    ID3D11SamplerState *pNearestSampler;

    ImGui_ImplDX11_Data()
    {
        memset((void *)this, 0, sizeof(*this));
        VertexBufferSize = 5000;
        IndexBufferSize  = 10000;
    }
};

struct VERTEX_CONSTANT_BUFFER_DX11
{
    float mvp[4][4];
};

struct PS_CONSTANT_BUFFER
{
    int32_t format;
    int32_t useAreaSample;
    int32_t textureColorRange;
    float   textureSize[2];
    float   textureShowingSize[2];
    float   renderSize[2];
    uint8_t padding[12];
};
static_assert(sizeof(PS_CONSTANT_BUFFER) % 16 == 0, "PS_CONSTANT_BUFFER size must be multiple of 16");

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple
// windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplDX11_Data *ImGui_ImplDX11_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data *)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Forward Declarations
static void ImGui_ImplDX11_InitMultiViewportSupport();
static void ImGui_ImplDX11_ShutdownMultiViewportSupport();

// Functions
static void ImGui_ImplDX11_SetupRenderState(ImDrawData *draw_data, ID3D11DeviceContext *device_ctx)
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();

    // Setup viewport
    D3D11_VIEWPORT vp;
    memset(&vp, 0, sizeof(D3D11_VIEWPORT));
    vp.Width    = draw_data->DisplaySize.x;
    vp.Height   = draw_data->DisplaySize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0;
    device_ctx->RSSetViewports(1, &vp);

    // Setup shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;
    device_ctx->IASetInputLayout(bd->pInputLayout);
    device_ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);
    device_ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_ctx->VSSetShader(bd->pVertexShader, nullptr, 0);
    device_ctx->VSSetConstantBuffers(0, 1, &bd->pVertexConstantBuffer);
    device_ctx->PSSetShader(bd->pPixelShader, nullptr, 0);
    device_ctx->PSSetSamplers(0, 1, &bd->pFontSampler);
    device_ctx->GSSetShader(nullptr, nullptr, 0);
    device_ctx->HSSetShader(nullptr, nullptr,
                            0); // In theory we should backup and restore this as well.. very infrequently used..
    device_ctx->DSSetShader(nullptr, nullptr,
                            0); // In theory we should backup and restore this as well.. very infrequently used..
    device_ctx->CSSetShader(nullptr, nullptr,
                            0); // In theory we should backup and restore this as well.. very infrequently used..

    // Setup blend state
    const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
    device_ctx->OMSetBlendState(bd->pBlendState, blend_factor, 0xffffffff);
    device_ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);
    device_ctx->RSSetState(bd->pRasterizerState);
}

// Render function
void ImGui_ImplDX11_RenderDrawData(ImDrawData *draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX11_Data *bd     = ImGui_ImplDX11_GetBackendData();
    ID3D11DeviceContext *device = bd->pd3dDeviceContext;

    // Create and grow vertex/index buffers if needed
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB)
        {
            bd->pVB->Release();
            bd->pVB = nullptr;
        }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D11_BUFFER_DESC desc;
        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth      = bd->VertexBufferSize * sizeof(ImDrawVert);
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags      = 0;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVB) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB)
        {
            bd->pIB->Release();
            bd->pIB = nullptr;
        }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D11_BUFFER_DESC desc;
        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth      = bd->IndexBufferSize * sizeof(ImDrawIdx);
        desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pIB) < 0)
            return;
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    if (device->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
        return;
    if (device->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
        return;
    ImDrawVert *vtx_dst = (ImDrawVert *)vtx_resource.pData;
    ImDrawIdx  *idx_dst = (ImDrawIdx *)idx_resource.pData;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *draw_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += draw_list->VtxBuffer.Size;
        idx_dst += draw_list->IdxBuffer.Size;
    }
    device->Unmap(bd->pVB, 0);
    device->Unmap(bd->pIB, 0);

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to
    // draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        if (device->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
            return;
        VERTEX_CONSTANT_BUFFER_DX11 *constant_buffer = (VERTEX_CONSTANT_BUFFER_DX11 *)mapped_resource.pData;
        float                        L               = draw_data->DisplayPos.x;
        float                        R               = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float                        T               = draw_data->DisplayPos.y;
        float                        B               = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float                        mvp[4][4]       = {
            {2.0f / (R - L),    0.0f,              0.0f, 0.0f},
            {0.0f,              2.0f / (T - B),    0.0f, 0.0f},
            {0.0f,              0.0f,              0.5f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
        };
        memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
        device->Unmap(bd->pVertexConstantBuffer, 0);
    }

    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and
    // verbose. Close your eyes!)
    struct BACKUP_DX11_STATE
    {
        UINT                      ScissorRectsCount, ViewportsCount;
        D3D11_RECT                ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT            Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState    *RS;
        ID3D11BlendState         *BlendState;
        FLOAT                     BlendFactor[4];
        UINT                      SampleMask;
        UINT                      StencilRef;
        ID3D11DepthStencilState  *DepthStencilState;
        ID3D11ShaderResourceView *PSShaderResource;
        ID3D11SamplerState       *PSSampler;
        ID3D11PixelShader        *PS;
        ID3D11VertexShader       *VS;
        ID3D11GeometryShader     *GS;
        UINT                      PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance      *PSInstances[256], *VSInstances[256],
            *GSInstances[256]; // 256 is max according to PSSetShader documentation
        D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;
        ID3D11Buffer            *IndexBuffer, *VertexBuffer, *VSConstantBuffer;
        UINT                     IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT              IndexBufferFormat;
        ID3D11InputLayout       *InputLayout;
    };
    BACKUP_DX11_STATE old = {};
    old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    device->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
    device->RSGetViewports(&old.ViewportsCount, old.Viewports);
    device->RSGetState(&old.RS);
    device->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
    device->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
    device->PSGetShaderResources(0, 1, &old.PSShaderResource);
    device->PSGetSamplers(0, 1, &old.PSSampler);
    old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
    device->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
    device->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
    device->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
    device->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

    device->IAGetPrimitiveTopology(&old.PrimitiveTopology);
    device->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
    device->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
    device->IAGetInputLayout(&old.InputLayout);

    // Setup desired DX state
    ImGui_ImplDX11_SetupRenderState(draw_data, device);

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO           &platform_io = ImGui::GetPlatformIO();
    ImGui_ImplDX11_RenderState render_state;
    render_state.Device              = bd->pd3dDevice;
    render_state.DeviceContext       = bd->pd3dDeviceContext;
    render_state.SamplerDefault      = bd->pFontSampler;
    platform_io.Renderer_RenderState = &render_state;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    device->Map(bd->pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    PS_CONSTANT_BUFFER *constantBuffer = (PS_CONSTANT_BUFFER *)mappedResource.pData;
    constantBuffer->format             = -1;
    device->Unmap(bd->pPixelConstantBuffer, 0);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int    global_idx_offset = 0;
    int    global_vtx_offset = 0;
    ImVec2 clip_off          = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer
                // to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX11_SetupRenderState(draw_data, device);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                const D3D11_RECT r = {(LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y};
                device->RSSetScissorRects(1, &r);

                // Bind texture, Draw

                RenderSource             *render_source = (RenderSource *)pcmd->GetTexID();
                ID3D11ShaderResourceView *texture_srv   = nullptr;
                if (render_source)
                    texture_srv = (ID3D11ShaderResourceView *)(render_source->textureID[0]);
                device->PSSetShaderResources(0, 1, &texture_srv);
                ID3D11SamplerState *currentSampler  = nullptr;
                bool                needRenderImage = false;
                if (pcmd->ElemCount == 6) // maybe is a picture
                {
                    ImDrawVert vertices[6];
                    auto      *pVertices = draw_list->VtxBuffer.Data + pcmd->VtxOffset;
                    auto      *indices   = draw_list->IdxBuffer.Data + pcmd->IdxOffset;
                    for (int i = 0; i < 6; i++)
                    {
                        int idx     = indices[i];
                        vertices[i] = pVertices[idx];
                    }
                    ImVec2 texturePos, textureSize;
                    ImVec2 renderPos, renderSize;
                    if (checkTextureRect(vertices, texturePos, textureSize, renderPos, renderSize)
                        && (render_source->sampleType != ImGuiImageSampleType_Linear
                            || render_source->imageFormat != ImGuiImageFormat_RGBA))
                    {
                        needRenderImage = true;

                        ID3D11ShaderResourceView *srvs[IMGUI_IMAGE_MAX_PLANES];
                        unsigned int              planes = getPlaneCount(render_source->imageFormat);
                        for (unsigned int i = 0; i < planes; i++)
                        {
                            srvs[i] = (ID3D11ShaderResourceView *)render_source->textureID[i];
                        }

                        device->PSSetShaderResources(0, planes, srvs);

                        ZeroMemory(&mappedResource, sizeof(mappedResource));
                        device->Map(bd->pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                        constantBuffer                    = (PS_CONSTANT_BUFFER *)mappedResource.pData;
                        constantBuffer->format            = getTextureFormat(render_source->imageFormat);
                        constantBuffer->textureColorRange = render_source->colorRange;
                        constantBuffer->useAreaSample     = (render_source->sampleType == ImGuiImageSampleType_Area ? 1 : 0);
                        // printf("format: %d, colorRange: %d, useAreaSample: %d\n", constantBuffer->format,
                        //        constantBuffer->textureColorRange, constantBuffer->useAreaSample);

                        constantBuffer->textureSize[0]        = (float)render_source->width;
                        constantBuffer->textureSize[1]        = (float)render_source->height;
                        constantBuffer->textureShowingSize[0] = render_source->width * textureSize.x;
                        constantBuffer->textureShowingSize[1] = render_source->height * textureSize.y;
                        constantBuffer->renderSize[0]         = renderSize.x;
                        constantBuffer->renderSize[1]         = renderSize.y;

                        device->Unmap(bd->pPixelConstantBuffer, 0);

                        if (render_source->sampleType == ImGuiImageSampleType_Nearest)
                        {
                            device->PSGetSamplers(0, 1, &currentSampler);
                            device->PSSetSamplers(0, 1, &bd->pNearestSampler);
                        }
                    }
                }
                device->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
                if (currentSampler != nullptr)
                {
                    device->PSSetSamplers(0, 1, &currentSampler);
                }
                if (needRenderImage)
                {
                    ZeroMemory(&mappedResource, sizeof(mappedResource));
                    device->Map(bd->pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                    constantBuffer         = (PS_CONSTANT_BUFFER *)mappedResource.pData;
                    constantBuffer->format = -1;
                    device->Unmap(bd->pPixelConstantBuffer, 0);
                }
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
    platform_io.Renderer_RenderState = NULL;

    // Restore modified DX state
    device->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
    device->RSSetViewports(old.ViewportsCount, old.Viewports);
    device->RSSetState(old.RS);
    if (old.RS)
        old.RS->Release();
    device->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask);
    if (old.BlendState)
        old.BlendState->Release();
    device->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef);
    if (old.DepthStencilState)
        old.DepthStencilState->Release();
    device->PSSetShaderResources(0, 1, &old.PSShaderResource);
    if (old.PSShaderResource)
        old.PSShaderResource->Release();
    device->PSSetSamplers(0, 1, &old.PSSampler);
    if (old.PSSampler)
        old.PSSampler->Release();
    device->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount);
    if (old.PS)
        old.PS->Release();
    for (UINT i = 0; i < old.PSInstancesCount; i++)
        if (old.PSInstances[i])
            old.PSInstances[i]->Release();
    device->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount);
    if (old.VS)
        old.VS->Release();
    device->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer);
    if (old.VSConstantBuffer)
        old.VSConstantBuffer->Release();
    device->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount);
    if (old.GS)
        old.GS->Release();
    for (UINT i = 0; i < old.VSInstancesCount; i++)
        if (old.VSInstances[i])
            old.VSInstances[i]->Release();
    device->IASetPrimitiveTopology(old.PrimitiveTopology);
    device->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset);
    if (old.IndexBuffer)
        old.IndexBuffer->Release();
    device->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
    if (old.VertexBuffer)
        old.VertexBuffer->Release();
    device->IASetInputLayout(old.InputLayout);
    if (old.InputLayout)
        old.InputLayout->Release();
}

static void ImGui_ImplDX11_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO             &io = ImGui::GetIO();
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    unsigned char       *pixels;
    int                  width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Upload texture to graphics system
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width            = width;
        desc.Height           = height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DEFAULT;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags   = 0;

        ID3D11Texture2D       *pTexture = nullptr;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem          = pixels;
        subResource.SysMemPitch      = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
        IM_ASSERT(pTexture != nullptr);

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
        pTexture->Release();
    }

    // Store our identifier
    bd->fontRenderSource.textureID[0] = (uintptr_t)bd->pFontTextureView;
    io.Fonts->SetTexID((ImTextureID)&bd->fontRenderSource);

    // Create texture sampler
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or
    // 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    {
        D3D11_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MipLODBias     = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD         = 0.f;
        desc.MaxLOD         = 0.f;
        bd->pd3dDevice->CreateSamplerState(&desc, &bd->pFontSampler);
    }
}

    #include "imgui_impl_dx11_pixel_shader.inl"

bool ImGui_ImplDX11_CreateDeviceObjects()
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;
    if (bd->pFontSampler)
        ImGui_ImplDX11_InvalidateDeviceObjects();

    // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of
    // d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A) If you would like to use this DX11 sample code but remove this
    // dependency you can:
    //  1) compile once, save the compiled shader blobs into a file or source code and pass them to
    //  CreateVertexShader()/CreatePixelShader() [preferred solution] 2) use code to detect any version of the DLL and
    //  grab a pointer to D3DCompile from the DLL.
    // See https://github.com/ocornut/imgui/pull/638 for sources and details.

    // Create the vertex shader
    {
        static const char *vertexShader = "cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

        ID3DBlob *vertexShaderBlob;
        if (FAILED(D3DCompile(vertexShader, strlen(vertexShader), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0,
                              &vertexShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const
                          // char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr,
                                               &bd->pVertexShader)
            != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }

        // Create the input layout
        D3D11_INPUT_ELEMENT_DESC local_layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (bd->pd3dDevice->CreateInputLayout(local_layout, 3, vertexShaderBlob->GetBufferPointer(),
                                              vertexShaderBlob->GetBufferSize(), &bd->pInputLayout)
            != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }
        vertexShaderBlob->Release();

        // Create the constant buffer
        {
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth      = sizeof(VERTEX_CONSTANT_BUFFER_DX11);
            desc.Usage          = D3D11_USAGE_DYNAMIC;
            desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags      = 0;
            bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVertexConstantBuffer);
        }
    }

    // Create the pixel shader
    {
        ID3DBlob *pixelShaderBlob;
        if (FAILED(D3DCompile(Dx11_PixelShader, strlen(Dx11_PixelShader), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0,
                              &pixelShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const
                          // char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr,
                                              &bd->pPixelShader)
            != S_OK)
        {
            pixelShaderBlob->Release();
            return false;
        }
        pixelShaderBlob->Release();
    }

    D3D11_BUFFER_DESC constantBufferDesc;
    ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
    constantBufferDesc.ByteWidth      = sizeof(PS_CONSTANT_BUFFER);
    constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantBufferDesc.MiscFlags      = 0;

    bd->pd3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &bd->pPixelConstantBuffer);

    bd->pd3dDeviceContext->PSSetConstantBuffers(0, 1, &bd->pPixelConstantBuffer);

    D3D11_SAMPLER_DESC samplerDesc;

    ZeroMemory(&samplerDesc, sizeof(samplerDesc));
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD         = 0;
    samplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;

    bd->pd3dDevice->CreateSamplerState(&samplerDesc, &bd->pNearestSampler);

    // Create the blending setup
    {
        D3D11_BLEND_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.AlphaToCoverageEnable                 = false;
        desc.RenderTarget[0].BlendEnable           = true;
        desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bd->pd3dDevice->CreateBlendState(&desc, &bd->pBlendState);
    }

    // Create the rasterizer state
    {
        D3D11_RASTERIZER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.FillMode        = D3D11_FILL_SOLID;
        desc.CullMode        = D3D11_CULL_NONE;
        desc.ScissorEnable   = true;
        desc.DepthClipEnable = true;
        bd->pd3dDevice->CreateRasterizerState(&desc, &bd->pRasterizerState);
    }

    // Create depth-stencil State
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.DepthEnable             = false;
        desc.DepthWriteMask          = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc               = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable           = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc                                                                      = D3D11_COMPARISON_ALWAYS;
        desc.BackFace                                                                                   = desc.FrontFace;
        bd->pd3dDevice->CreateDepthStencilState(&desc, &bd->pDepthStencilState);
    }

    ImGui_ImplDX11_CreateFontsTexture();

    return true;
}

void ImGui_ImplDX11_InvalidateDeviceObjects()
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return;

    if (bd->pFontSampler)
    {
        bd->pFontSampler->Release();
        bd->pFontSampler = nullptr;
    }
    if (bd->pFontTextureView)
    {
        bd->pFontTextureView->Release();
        bd->pFontTextureView = nullptr;
        bd->fontRenderSource = RenderSource();
        ImGui::GetIO().Fonts->SetTexID(0);
    } // We copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
    if (bd->pIB)
    {
        bd->pIB->Release();
        bd->pIB = nullptr;
    }
    if (bd->pVB)
    {
        bd->pVB->Release();
        bd->pVB = nullptr;
    }
    if (bd->pBlendState)
    {
        bd->pBlendState->Release();
        bd->pBlendState = nullptr;
    }
    if (bd->pDepthStencilState)
    {
        bd->pDepthStencilState->Release();
        bd->pDepthStencilState = nullptr;
    }
    if (bd->pRasterizerState)
    {
        bd->pRasterizerState->Release();
        bd->pRasterizerState = nullptr;
    }
    if (bd->pPixelShader)
    {
        bd->pPixelShader->Release();
        bd->pPixelShader = nullptr;
    }
    if (bd->pVertexConstantBuffer)
    {
        bd->pVertexConstantBuffer->Release();
        bd->pVertexConstantBuffer = nullptr;
    }
    if (bd->pInputLayout)
    {
        bd->pInputLayout->Release();
        bd->pInputLayout = nullptr;
    }
    if (bd->pVertexShader)
    {
        bd->pVertexShader->Release();
        bd->pVertexShader = nullptr;
    }
}

bool ImGui_ImplDX11_Init(ID3D11Device *device, ID3D11DeviceContext *device_context)
{
    ImGuiIO &io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX11_Data *bd    = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName     = "imgui_impl_dx11";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing
                                                               // for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)

    // Get factory from device
    IDXGIDevice  *pDXGIDevice  = nullptr;
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory *pFactory     = nullptr;

    if (device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK)
        if (pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK)
            if (pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK)
            {
                bd->pd3dDevice        = device;
                bd->pd3dDeviceContext = device_context;
                bd->pFactory          = pFactory;
            }
    if (pDXGIDevice)
        pDXGIDevice->Release();
    if (pDXGIAdapter)
        pDXGIAdapter->Release();
    bd->pd3dDevice->AddRef();
    bd->pd3dDeviceContext->AddRef();

    ImGui_ImplDX11_InitMultiViewportSupport();

    return true;
}

void ImGui_ImplDX11_Shutdown()
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplDX11_ShutdownMultiViewportSupport();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    if (bd->pFactory)
    {
        bd->pFactory->Release();
    }
    if (bd->pd3dDevice)
    {
        bd->pd3dDevice->Release();
    }
    if (bd->pd3dDeviceContext)
    {
        bd->pd3dDeviceContext->Release();
    }
    io.BackendRendererName     = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports);
    IM_DELETE(bd);
}

void ImGui_ImplDX11_NewFrame()
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX11_Init()?");

    if (!bd->pFontSampler)
        ImGui_ImplDX11_CreateDeviceObjects();
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports
// simultaneously. If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you
// completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend
// data.
struct ImGui_ImplDX11_ViewportData
{
    IDXGISwapChain         *SwapChain;
    ID3D11RenderTargetView *RTView;

    ImGui_ImplDX11_ViewportData()
    {
        SwapChain = nullptr;
        RTView    = nullptr;
    }
    ~ImGui_ImplDX11_ViewportData() { IM_ASSERT(SwapChain == nullptr && RTView == nullptr); }
};

static void ImGui_ImplDX11_CreateWindow(ImGuiViewport *viewport)
{
    ImGui_ImplDX11_Data         *bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData *vd = IM_NEW(ImGui_ImplDX11_ViewportData)();
    viewport->RendererUserData      = vd;

    // PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g.
    // GLFWWindow*, SDL_Window*). Some backends will leave PlatformHandleRaw == 0, in which case we assume
    // PlatformHandle will contain the HWND.
    HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
    IM_ASSERT(hwnd != 0);

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width   = (UINT)viewport->Size.x;
    sd.BufferDesc.Height  = (UINT)viewport->Size.y;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount        = 1;
    sd.OutputWindow       = hwnd;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags              = 0;

    IM_ASSERT(vd->SwapChain == nullptr && vd->RTView == nullptr);
    bd->pFactory->CreateSwapChain(bd->pd3dDevice, &sd, &vd->SwapChain);

    // Create the render target
    if (vd->SwapChain)
    {
        ID3D11Texture2D *pBackBuffer;
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_DestroyWindow(ImGuiViewport *viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == nullptr since we didn't create
    // the data for it.
    if (ImGui_ImplDX11_ViewportData *vd = (ImGui_ImplDX11_ViewportData *)viewport->RendererUserData)
    {
        if (vd->SwapChain)
            vd->SwapChain->Release();
        vd->SwapChain = nullptr;
        if (vd->RTView)
            vd->RTView->Release();
        vd->RTView = nullptr;
        IM_DELETE(vd);
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_ImplDX11_SetWindowSize(ImGuiViewport *viewport, ImVec2 size)
{
    ImGui_ImplDX11_Data         *bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData *vd = (ImGui_ImplDX11_ViewportData *)viewport->RendererUserData;
    if (vd->RTView)
    {
        vd->RTView->Release();
        vd->RTView = nullptr;
    }
    if (vd->SwapChain)
    {
        ID3D11Texture2D *pBackBuffer = nullptr;
        vd->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer == nullptr)
        {
            dbg("ImGui_ImplDX11_SetWindowSize() failed creating buffers.\n");
            return;
        }
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_RenderWindow(ImGuiViewport *viewport, void *)
{
    ImGui_ImplDX11_Data         *bd          = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData *vd          = (ImGui_ImplDX11_ViewportData *)viewport->RendererUserData;
    ImVec4                       clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    bd->pd3dDeviceContext->OMSetRenderTargets(1, &vd->RTView, nullptr);
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
        bd->pd3dDeviceContext->ClearRenderTargetView(vd->RTView, (float *)&clear_color);
    ImGui_ImplDX11_RenderDrawData(viewport->DrawData);
}

static void ImGui_ImplDX11_SwapBuffers(ImGuiViewport *viewport, void *)
{
    ImGui_ImplDX11_ViewportData *vd = (ImGui_ImplDX11_ViewportData *)viewport->RendererUserData;
    vd->SwapChain->Present(0, 0); // Present without vsync
}

static void ImGui_ImplDX11_InitMultiViewportSupport()
{
    ImGuiPlatformIO &platform_io       = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow  = ImGui_ImplDX11_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplDX11_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplDX11_SetWindowSize;
    platform_io.Renderer_RenderWindow  = ImGui_ImplDX11_RenderWindow;
    platform_io.Renderer_SwapBuffers   = ImGui_ImplDX11_SwapBuffers;
}

static void ImGui_ImplDX11_ShutdownMultiViewportSupport()
{
    ImGui::DestroyPlatformWindows();
}

    #define SAFE_RELEASE_RES(res) \
        do                        \
        {                         \
            if (res)              \
            {                     \
                (res)->Release(); \
                (res) = nullptr;  \
            }                     \
        } while (0)

int createImageTexture(ID3D11Texture2D **ptex, ID3D11ShaderResourceView **psrv, int texWidth, int texHeight, DXGI_FORMAT format)
{
    ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
    if (!bd || !bd->pd3dDevice || !bd->pd3dDeviceContext)
    {
        dbg("null\n");
        return -1;
    }

    SAFE_RELEASE_RES(*psrv);
    SAFE_RELEASE_RES(*ptex);

    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(textureDesc));

    textureDesc.Format           = format;
    textureDesc.Width            = texWidth;
    textureDesc.Height           = texHeight;
    textureDesc.MipLevels        = 1;
    textureDesc.ArraySize        = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage            = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags   = 0;
    textureDesc.MiscFlags        = 0;

    HRESULT hr = bd->pd3dDevice->CreateTexture2D(&textureDesc, 0, ptex);
    if (FAILED(hr))
    {
        dbg("CreateTexture2D failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
        return -1;
    }

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format                    = format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 0xFFFFFFFF;
    srvDesc.Texture2D.MostDetailedMip = 0;
    hr                                = bd->pd3dDevice->CreateShaderResourceView(*ptex, &srvDesc, psrv);
    if (FAILED(hr))
    {
        dbg("CreateShaderResourceView failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
        return -1;
    }
    return 0;
}

namespace ImGui
{
    bool updateImageTexture(ImageData &image, TextureSource &texture)
    {
        ImGui_ImplDX11_Data *bd = ImGui_ImplDX11_GetBackendData();
        if (!bd || !bd->pd3dDeviceContext || image.width <= 0 || image.height <= 0)
        {
            dbg("null\n");
            return false;
        }
        if (image.format == ImGuiImageFormat_Dx11) // DX11 texture create
        {
            ID3D11ShaderResourceView *texSRV[2]        = {(ID3D11ShaderResourceView *)texture.textureID[0],
                                                          (ID3D11ShaderResourceView *)texture.textureID[1]};
            ID3D11Texture2D          *nativeTexture[2] = {nullptr};

            HRESULT hr = S_OK;

            bool   needRebuild = true;
            HANDLE shareHandle = 0;
            // TODO: assume it's DXGI_FORMAT_NV12 here, need to support DXGI_FORMAT_YUY2 and DXGI_FORMAT_P010
            do
            {
                if (!texSRV[0] || !texSRV[1])
                    break;

                texSRV[0]->GetResource((ID3D11Resource **)&nativeTexture[0]);
                texSRV[1]->GetResource((ID3D11Resource **)&nativeTexture[1]);

                if (nativeTexture[0] != nativeTexture[1])
                    break;

                D3D11_TEXTURE2D_DESC desc;
                nativeTexture[0]->GetDesc(&desc);
                if (desc.Width != (UINT)image.width || desc.Height != (UINT)image.height || desc.Format != DXGI_FORMAT_NV12
                    || 0 == (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED))
                    break;

                SAFE_RELEASE_RES(nativeTexture[1]);
                needRebuild = false;
            } while (0);

            if (needRebuild)
            {
                SAFE_RELEASE_RES(nativeTexture[0]);
                SAFE_RELEASE_RES(nativeTexture[1]);
                SAFE_RELEASE_RES(texSRV[0]);
                SAFE_RELEASE_RES(texSRV[1]);
                bool error = true;
                do
                {
                    D3D11_TEXTURE2D_DESC textureDesc;
                    ZeroMemory(&textureDesc, sizeof(textureDesc));

                    textureDesc.Format           = DXGI_FORMAT_NV12;
                    textureDesc.Width            = image.width;
                    textureDesc.Height           = image.height;
                    textureDesc.MipLevels        = 1;
                    textureDesc.ArraySize        = 1;
                    textureDesc.SampleDesc.Count = 1;
                    textureDesc.Usage            = D3D11_USAGE_DEFAULT;
                    textureDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
                    textureDesc.CPUAccessFlags   = 0;
                    textureDesc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

                    hr = bd->pd3dDevice->CreateTexture2D(&textureDesc, 0, &nativeTexture[0]);
                    if (FAILED(hr))
                    {
                        dbg("CreateTexture2D failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                        break;
                    }

                    // Create texture view
                    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
                    ZeroMemory(&srvDesc, sizeof(srvDesc));
                    srvDesc.Format                    = DXGI_FORMAT_R8_UNORM;
                    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels       = 0xFFFFFFFF;
                    srvDesc.Texture2D.MostDetailedMip = 0;
                    hr = bd->pd3dDevice->CreateShaderResourceView(nativeTexture[0], &srvDesc, &texSRV[0]);
                    if (FAILED(hr))
                    {
                        dbg("CreateShaderResourceView failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                        break;
                    }
                    srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
                    hr             = bd->pd3dDevice->CreateShaderResourceView(nativeTexture[0], &srvDesc, &texSRV[1]);
                    if (FAILED(hr))
                    {
                        dbg("CreateShaderResourceView failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                        break;
                    }

                    error = false;
                } while (0);

                if (error)
                {
                    SAFE_RELEASE_RES(nativeTexture[0]);
                    SAFE_RELEASE_RES(texSRV[0]);
                    SAFE_RELEASE_RES(texSRV[1]);
                    return false;
                }
                texture.textureID[0] = (uintptr_t)texSRV[0];
                texture.textureID[1] = (uintptr_t)texSRV[1];
            }

            // Get shared handle
            IDXGIResource *dxgiResource = nullptr;
            hr                          = nativeTexture[0]->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiResource);
            SAFE_RELEASE_RES(nativeTexture[0]);
            if (FAILED(hr))
            {
                dbg("QueryInterface IDXGIResource failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                return false;
            }
            hr = dxgiResource->GetSharedHandle(&shareHandle);
            SAFE_RELEASE_RES(dxgiResource);
            if (FAILED(hr))
            {
                dbg("GetSharedHandle failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                return false;
            }

            ID3D11Texture2D *t_frame = (ID3D11Texture2D *)image.plane[0];
            int              t_index = (int)(intptr_t)image.plane[1];

            ID3D11Device *device = nullptr;
            t_frame->GetDevice(&device);
            if (nullptr == device)
            {
                dbg("GetDevice Fail\n");
                return false;
            }

            ID3D11DeviceContext *deviceCtx = nullptr;
            device->GetImmediateContext(&deviceCtx);
            if (nullptr == deviceCtx)
            {
                dbg("Get Context Fail\n");
                device->Release();
                return false;
            }

            ID3D11Texture2D *videoTexture = nullptr;
            hr = device->OpenSharedResource(shareHandle, __uuidof(ID3D11Texture2D), (void **)&videoTexture);
            if (FAILED(hr))
            {
                dbg("OpenSharedResource failed %s\n", utf8ToLocal(HResultToStr(hr)).c_str());
                device->Release();
                deviceCtx->Release();
                return false;
            }

            deviceCtx->CopySubresourceRegion(videoTexture, 0, 0, 0, 0, t_frame, t_index, 0);
            videoTexture->Release();
            deviceCtx->Flush();

            // ID3D11Query     *query;
            // D3D11_QUERY_DESC desc = {D3D11_QUERY_EVENT, 0};
            // device->CreateQuery(&desc, &query);
            // deviceCtx->End(query);
            // int waitCount = 100;
            // while (S_OK != deviceCtx->GetData(query, nullptr, 0, 0))
            // {
            //     if (waitCount-- <= 0)
            //     {
            //         dbg("Wait for texture copy timeout\n");
            //         break;
            //     }
            //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // }
            // query->Release();
            device->Release();
            deviceCtx->Release();
            printf("copy done\n");
            texture.imageFormat = ImGuiImageFormat_NV12;
        }
        else
        {
            unsigned int planeCount = getPlaneCount(image.format);
            if (planeCount <= 0)
            {
                dbg("unsupported format %d\n", image.format);
                return false;
            }
            for (unsigned int i = 0; i < planeCount; i++)
            {
                ID3D11ShaderResourceView *texSRV        = (ID3D11ShaderResourceView *)texture.textureID[i];
                ID3D11Texture2D          *nativeTexture = nullptr;

                unsigned int bytesPerPixel = 0;
                unsigned int width         = 0;
                unsigned int height        = 0;
                getPlaneInfo(image.format, image.width, image.height, i, &bytesPerPixel, &width, &height);

                DXGI_FORMAT dxgiFormat;
                switch (bytesPerPixel)
                {
                    case 1:
                        dxgiFormat = DXGI_FORMAT_R8_UNORM;
                        break;
                    case 2:
                        dxgiFormat = DXGI_FORMAT_R8G8_UNORM;
                        break;
                    case 4:
                        dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                        break;
                    default:
                        dbg("unsupported bytesPerPixel %d\n", bytesPerPixel);
                        return false;
                }

                if (texSRV)
                    texSRV->GetResource((ID3D11Resource **)&nativeTexture);
                do
                {
                    if (!nativeTexture)
                        break;

                    D3D11_TEXTURE2D_DESC desc;
                    nativeTexture->GetDesc(&desc);
                    if (desc.Width != (UINT)width || desc.Height != (UINT)height || desc.Format != dxgiFormat)
                    {
                        SAFE_RELEASE_RES(texSRV);
                        SAFE_RELEASE_RES(nativeTexture);
                    }
                } while (0);

                if (!texSRV || !nativeTexture)
                {
                    SAFE_RELEASE_RES(texSRV);
                    SAFE_RELEASE_RES(nativeTexture);
                    if (createImageTexture(&nativeTexture, &texSRV, width, height, dxgiFormat) < 0)
                    {
                        dbg("createImageTexture fail\n");
                        SAFE_RELEASE_RES(texSRV);
                        SAFE_RELEASE_RES(nativeTexture);
                        return false;
                    }
                }

                bd->pd3dDeviceContext->UpdateSubresource(nativeTexture, 0, 0, image.plane[i], (UINT)image.stride[i], 0);

                texture.textureID[i] = (uintptr_t)texSRV;
                SAFE_RELEASE_RES(nativeTexture);
            }
            texture.imageFormat = image.format;
        }
        texture.width      = image.width;
        texture.height     = image.height;
        texture.colorRange = image.colorRange;
        return true;
    }

    void freeTexture(TextureSource &texture)
    {
        for (int i = 0; i < IMGUI_IMAGE_MAX_PLANES; i++)
        {
            ID3D11ShaderResourceView *texSRV = (ID3D11ShaderResourceView *)texture.textureID[i];
            SAFE_RELEASE_RES(texSRV);
            texture.textureID[i] = 0;
        }
    }
} // namespace ImGui
//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
