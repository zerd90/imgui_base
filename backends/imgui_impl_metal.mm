// dear imgui: Renderer Backend for Metal
// This needs to be used along with a Platform Backend (e.g. OSX)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'MTLTexture' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.
//  [X] Renderer: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2024-XX-XX: Metal: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2022-08-23: Metal: Update deprecated property 'sampleCount'->'rasterSampleCount'.
//  2022-07-05: Metal: Add dispatch synchronization.
//  2022-06-30: Metal: Use __bridge for ARC based systems.
//  2022-06-01: Metal: Fixed null dereference on exit inside command buffer completion handler.
//  2022-04-27: Misc: Store backend data in a per-context struct, allowing to use this backend with multiple contexts.
//  2022-01-03: Metal: Ignore ImDrawCmd where ElemCount == 0 (very rare but can technically be manufactured by user code).
//  2021-12-30: Metal: Added Metal C++ support. Enable with '#define IMGUI_IMPL_METAL_CPP' in your imconfig.h file.
//  2021-08-24: Metal: Fixed a crash when clipping rect larger than framebuffer is submitted. (#4464)
//  2021-05-19: Metal: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-02-18: Metal: Change blending equation to preserve alpha in output buffer.
//  2021-01-25: Metal: Fixed texture storage mode when building on Mac Catalyst.
//  2019-05-29: Metal: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.
//  2019-04-30: Metal: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2019-02-11: Metal: Projecting clipping rectangles correctly using draw_data->FramebufferScale to allow multi-viewports for retina display.
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-07-05: Metal: Added new Metal backend implementation.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_metal.h"
#include "ImGuiCommonTools.h"
#include "ImGuiImageRender.h"
#import <time.h>
#import <Metal/Metal.h>

// Forward Declarations
static void ImGui_ImplMetal_InitMultiViewportSupport();
static void ImGui_ImplMetal_ShutdownMultiViewportSupport();
static void ImGui_ImplMetal_CreateDeviceObjectsForPlatformWindows();
static void ImGui_ImplMetal_InvalidateDeviceObjectsForPlatformWindows();

#pragma mark - Support classes

// A wrapper around a MTLBuffer object that knows the last time it was reused
@interface MetalBuffer : NSObject
@property (nonatomic, strong) id<MTLBuffer> buffer;
@property (nonatomic, assign) double        lastReuseTime;
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer;
@end

// An object that encapsulates the data necessary to uniquely identify a
// render pipeline state. These are used as cache keys.
@interface FramebufferDescriptor : NSObject<NSCopying>
@property (nonatomic, assign) unsigned long  sampleCount;
@property (nonatomic, assign) MTLPixelFormat colorPixelFormat;
@property (nonatomic, assign) MTLPixelFormat depthPixelFormat;
@property (nonatomic, assign) MTLPixelFormat stencilPixelFormat;
- (instancetype)initWithRenderPassDescriptor:(MTLRenderPassDescriptor*)renderPassDescriptor;
@end

// A singleton that stores long-lived objects that are needed by the Metal
// renderer backend. Stores the render pipeline state cache and the default
// font texture, and manages the reusable buffer cache.
@interface MetalContext : NSObject
@property (nonatomic, strong) id<MTLDevice>                 device;
@property (nonatomic, strong) id<MTLDepthStencilState>      depthStencilState;
@property (nonatomic, strong) FramebufferDescriptor*        framebufferDescriptor; // framebuffer descriptor for current frame; transient
@property (nonatomic, strong) NSMutableDictionary*          renderPipelineStateCache; // pipeline cache; keyed on framebuffer descriptors
@property (nonatomic, strong, nullable) id<MTLTexture>      fontTexture;
@property (nonatomic, strong) NSMutableArray<MetalBuffer*>* bufferCache;
@property (nonatomic, assign) double                        lastBufferCachePurge;
@property (nonatomic, strong) id<MTLSamplerState>           linearSampler;
@property (nonatomic, strong) id<MTLSamplerState>           nearestSampler;
@property (nonatomic, strong) id<MTLTexture>                dummyTexture;
- (MetalBuffer*)dequeueReusableBufferOfLength:(NSUInteger)length device:(id<MTLDevice>)device;
- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptor:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device;
@end

struct MetalFragUniforms
{
    float textureSize[2];
    float textureShowingSize[2];
    float renderSize[2];
    int   format;
    int   useAreaSample;
    int   textureColorRange;
    int   pad;
};
static_assert(sizeof(MetalFragUniforms) == 40, "MetalFragUniforms layout must match the MSL struct");

struct ImGui_ImplMetal_Data
{
    MetalContext*          SharedMetalContext;
    ImGui::RenderSource    FontRenderSource;

    ImGui_ImplMetal_Data() : SharedMetalContext(nil) {}
};

static id<MTLTexture> ImGui_ImplMetal_TextureFromID(uintptr_t textureID)
{
    if (textureID == 0)
        return nil;
    return (__bridge id<MTLTexture>)(void*)textureID;
}

static ImGui_ImplMetal_Data*    ImGui_ImplMetal_GetBackendData()    { return ImGui::GetCurrentContext() ? (ImGui_ImplMetal_Data*)ImGui::GetIO().BackendRendererUserData : nullptr; }
static void                     ImGui_ImplMetal_DestroyBackendData(){ IM_DELETE(ImGui_ImplMetal_GetBackendData()); }

static inline CFTimeInterval    GetMachAbsoluteTimeInSeconds()      { return (CFTimeInterval)(double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9); }

#ifdef IMGUI_IMPL_METAL_CPP

#pragma mark - Dear ImGui Metal C++ Backend API

bool ImGui_ImplMetal_Init(MTL::Device* device)
{
    return ImGui_ImplMetal_Init((__bridge id<MTLDevice>)(device));
}

void ImGui_ImplMetal_NewFrame(MTL::RenderPassDescriptor* renderPassDescriptor)
{
    ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)(renderPassDescriptor));
}

void ImGui_ImplMetal_RenderDrawData(ImDrawData* draw_data,
                                    MTL::CommandBuffer* commandBuffer,
                                    MTL::RenderCommandEncoder* commandEncoder)
{
    ImGui_ImplMetal_RenderDrawData(draw_data,
                                   (__bridge id<MTLCommandBuffer>)(commandBuffer),
                                   (__bridge id<MTLRenderCommandEncoder>)(commandEncoder));

}

bool ImGui_ImplMetal_CreateFontsTexture(MTL::Device* device)
{
    return ImGui_ImplMetal_CreateFontsTexture((__bridge id<MTLDevice>)(device));
}

bool ImGui_ImplMetal_CreateDeviceObjects(MTL::Device* device)
{
    return ImGui_ImplMetal_CreateDeviceObjects((__bridge id<MTLDevice>)(device));
}

#endif // #ifdef IMGUI_IMPL_METAL_CPP

#pragma mark - Dear ImGui Metal Backend API

bool ImGui_ImplMetal_Init(id<MTLDevice> device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    ImGui_ImplMetal_Data* bd = IM_NEW(ImGui_ImplMetal_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_metal";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    bd->SharedMetalContext = [[MetalContext alloc] init];
    bd->SharedMetalContext.device = device;

    ImGui_ImplMetal_InitMultiViewportSupport();

    return true;
}

void ImGui_ImplMetal_Shutdown()
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGui_ImplMetal_ShutdownMultiViewportSupport();
    ImGui_ImplMetal_DestroyDeviceObjects();
    ImGui_ImplMetal_DestroyBackendData();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports);
}

void ImGui_ImplMetal_NewFrame(MTLRenderPassDescriptor* renderPassDescriptor)
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    IM_ASSERT(bd != nil && "Context or backend not initialized! Did you call ImGui_ImplMetal_Init()?");
    bd->SharedMetalContext.framebufferDescriptor = [[FramebufferDescriptor alloc] initWithRenderPassDescriptor:renderPassDescriptor];

    if (bd->SharedMetalContext.depthStencilState == nil)
        ImGui_ImplMetal_CreateDeviceObjects(bd->SharedMetalContext.device);
}

static void ImGui_ImplMetal_SetupRenderState(ImDrawData* drawData, id<MTLCommandBuffer> commandBuffer,
    id<MTLRenderCommandEncoder> commandEncoder, id<MTLRenderPipelineState> renderPipelineState,
    MetalBuffer* vertexBuffer, size_t vertexBufferOffset)
{
    IM_UNUSED(commandBuffer);
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    [commandEncoder setCullMode:MTLCullModeNone];
    [commandEncoder setDepthStencilState:bd->SharedMetalContext.depthStencilState];

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to
    // draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
    MTLViewport viewport =
    {
        .originX = 0.0,
        .originY = 0.0,
        .width = (double)(drawData->DisplaySize.x * drawData->FramebufferScale.x),
        .height = (double)(drawData->DisplaySize.y * drawData->FramebufferScale.y),
        .znear = 0.0,
        .zfar = 1.0
    };
    [commandEncoder setViewport:viewport];

    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    float N = (float)viewport.znear;
    float F = (float)viewport.zfar;
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,   0.0f },
        { 0.0f,         0.0f,        1/(F-N),   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T), N/(F-N),   1.0f },
    };
    [commandEncoder setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1];

    [commandEncoder setRenderPipelineState:renderPipelineState];

    [commandEncoder setVertexBuffer:vertexBuffer.buffer offset:0 atIndex:0];
    [commandEncoder setVertexBufferOffset:vertexBufferOffset atIndex:0];
}

// Metal Render function.
void ImGui_ImplMetal_RenderDrawData(ImDrawData* drawData, id<MTLCommandBuffer> commandBuffer, id<MTLRenderCommandEncoder> commandEncoder)
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    MetalContext* ctx = bd->SharedMetalContext;

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0 || drawData->CmdListsCount == 0)
        return;

    // Try to retrieve a render pipeline state that is compatible with the framebuffer config for this frame
    // The hit rate for this cache should be very near 100%.
    id<MTLRenderPipelineState> renderPipelineState = ctx.renderPipelineStateCache[ctx.framebufferDescriptor];
    if (renderPipelineState == nil)
    {
        // No luck; make a new render pipeline state
        renderPipelineState = [ctx renderPipelineStateForFramebufferDescriptor:ctx.framebufferDescriptor device:commandBuffer.device];

        // Cache render pipeline state for later reuse
        ctx.renderPipelineStateCache[ctx.framebufferDescriptor] = renderPipelineState;
    }

    size_t vertexBufferLength = (size_t)drawData->TotalVtxCount * sizeof(ImDrawVert);
    size_t indexBufferLength = (size_t)drawData->TotalIdxCount * sizeof(ImDrawIdx);
    MetalBuffer* vertexBuffer = [ctx dequeueReusableBufferOfLength:vertexBufferLength device:commandBuffer.device];
    MetalBuffer* indexBuffer = [ctx dequeueReusableBufferOfLength:indexBufferLength device:commandBuffer.device];

    ImGui_ImplMetal_SetupRenderState(drawData, commandBuffer, commandEncoder, renderPipelineState, vertexBuffer, 0);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = drawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    size_t vertexBufferOffset = 0;
    size_t indexBufferOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = drawData->CmdLists[n];

        memcpy((char*)vertexBuffer.buffer.contents + vertexBufferOffset, draw_list->VtxBuffer.Data, (size_t)draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy((char*)indexBuffer.buffer.contents + indexBufferOffset, draw_list->IdxBuffer.Data, (size_t)draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplMetal_SetupRenderState(drawData, commandBuffer, commandEncoder, renderPipelineState, vertexBuffer, vertexBufferOffset);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as setScissorRect() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;
                if (pcmd->ElemCount == 0) // drawIndexedPrimitives() validation doesn't accept this
                    continue;

                // Apply scissor/clipping rectangle
                MTLScissorRect scissorRect =
                {
                    .x = NSUInteger(clip_min.x),
                    .y = NSUInteger(clip_min.y),
                    .width = NSUInteger(clip_max.x - clip_min.x),
                    .height = NSUInteger(clip_max.y - clip_min.y)
                };
                [commandEncoder setScissorRect:scissorRect];

                // ImTextureID is a RenderSource*, matching the DX11/OpenGL backends.
                ImGui::RenderSource* renderSource = (ImGui::RenderSource*)pcmd->GetTexID();
                id<MTLTexture>       tex0         = ctx.dummyTexture;
                id<MTLTexture>       tex1         = ctx.dummyTexture;
                id<MTLTexture>       tex2         = ctx.dummyTexture;
                id<MTLSamplerState>  sampler      = ctx.linearSampler;
                MetalFragUniforms    uniforms     = {};
                uniforms.format                   = -1;

                if (renderSource != nullptr && renderSource->textureID[0] != 0)
                    tex0 = ImGui_ImplMetal_TextureFromID(renderSource->textureID[0]);

                if (renderSource != nullptr && pcmd->ElemCount == 6)
                {
                    ImDrawVert vertices[6];
                    const ImDrawVert* pVertices = draw_list->VtxBuffer.Data + pcmd->VtxOffset;
                    const ImDrawIdx*  indices   = draw_list->IdxBuffer.Data + pcmd->IdxOffset;
                    for (int i = 0; i < 6; i++)
                        vertices[i] = pVertices[indices[i]];

                    ImVec2 texturePos, textureSize, renderPos, renderSize;
                    if (ImGui::checkTextureRect(vertices, texturePos, textureSize, renderPos, renderSize)
                        && (renderSource->sampleType != ImGui::ImGuiImageSampleType_Linear
                            || renderSource->imageFormat != ImGui::ImGuiImageFormat_RGBA))
                    {
                        unsigned int planes = ImGui::getPlaneCount(renderSource->imageFormat);
                        if (planes > 0)
                            tex0 = ImGui_ImplMetal_TextureFromID(renderSource->textureID[0]);
                        if (planes > 1)
                            tex1 = ImGui_ImplMetal_TextureFromID(renderSource->textureID[1]);
                        if (planes > 2)
                            tex2 = ImGui_ImplMetal_TextureFromID(renderSource->textureID[2]);
                        if (tex0 == nil)
                            tex0 = ctx.dummyTexture;
                        if (tex1 == nil)
                            tex1 = ctx.dummyTexture;
                        if (tex2 == nil)
                            tex2 = ctx.dummyTexture;

                        uniforms.format              = ImGui::getTextureFormat(renderSource->imageFormat);
                        uniforms.useAreaSample       = renderSource->sampleType == ImGui::ImGuiImageSampleType_Area ? 1 : 0;
                        uniforms.textureColorRange   = (int)renderSource->colorRange;
                        uniforms.textureSize[0]      = (float)renderSource->width;
                        uniforms.textureSize[1]      = (float)renderSource->height;
                        uniforms.textureShowingSize[0] = (float)renderSource->width * textureSize.x;
                        uniforms.textureShowingSize[1] = (float)renderSource->height * textureSize.y;
                        uniforms.renderSize[0]       = renderSize.x;
                        uniforms.renderSize[1]       = renderSize.y;
                        if (renderSource->sampleType == ImGui::ImGuiImageSampleType_Nearest)
                            sampler = ctx.nearestSampler;
                    }
                }

                [commandEncoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                [commandEncoder setFragmentSamplerState:sampler atIndex:0];
                [commandEncoder setFragmentTexture:tex0 atIndex:0];
                [commandEncoder setFragmentTexture:tex1 atIndex:1];
                [commandEncoder setFragmentTexture:tex2 atIndex:2];

                [commandEncoder setVertexBufferOffset:(vertexBufferOffset + pcmd->VtxOffset * sizeof(ImDrawVert)) atIndex:0];
                [commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                           indexCount:pcmd->ElemCount
                                            indexType:sizeof(ImDrawIdx) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                                          indexBuffer:indexBuffer.buffer
                                    indexBufferOffset:indexBufferOffset + pcmd->IdxOffset * sizeof(ImDrawIdx)];
            }
        }

        vertexBufferOffset += (size_t)draw_list->VtxBuffer.Size * sizeof(ImDrawVert);
        indexBufferOffset += (size_t)draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
    }

    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>)
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
            if (bd != nullptr)
            {
                @synchronized(bd->SharedMetalContext.bufferCache)
                {
                    [bd->SharedMetalContext.bufferCache addObject:vertexBuffer];
                    [bd->SharedMetalContext.bufferCache addObject:indexBuffer];
                }
            }
        });
    }];
}

bool ImGui_ImplMetal_CreateFontsTexture(id<MTLDevice> device)
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    // We are retrieving and uploading the font atlas as a 4-channels RGBA texture here.
    // In theory we could call GetTexDataAsAlpha8() and upload a 1-channel texture to save on memory access bandwidth.
    // However, using a shader designed for 1-channel texture would make it less obvious to use the ImTextureID facility to render users own textures.
    // You can make that change in your implementation.
    unsigned char* pixels;
    int width, height;
    ImGui::waitFontPixPreload();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                 width:(NSUInteger)width
                                                                                                height:(NSUInteger)height
                                                                                             mipmapped:NO];
    textureDescriptor.usage = MTLTextureUsageShaderRead;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    textureDescriptor.storageMode = MTLStorageModeManaged;
#else
    textureDescriptor.storageMode = MTLStorageModeShared;
#endif
    id <MTLTexture> texture = [device newTextureWithDescriptor:textureDescriptor];
    [texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)width, (NSUInteger)height) mipmapLevel:0 withBytes:pixels bytesPerRow:(NSUInteger)width * 4];
    bd->SharedMetalContext.fontTexture = texture;
    bd->FontRenderSource               = ImGui::RenderSource();
    bd->FontRenderSource.textureID[0]  = (uintptr_t)(__bridge void*)texture;
    bd->FontRenderSource.imageFormat   = ImGui::ImGuiImageFormat_RGBA;
    bd->FontRenderSource.width         = width;
    bd->FontRenderSource.height        = height;
    bd->FontRenderSource.sampleType    = ImGui::ImGuiImageSampleType_Linear;
    io.Fonts->SetTexID((ImTextureID)(intptr_t)&bd->FontRenderSource);

    return (bd->SharedMetalContext.fontTexture != nil);
}

void ImGui_ImplMetal_DestroyFontsTexture()
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    bd->FontRenderSource = ImGui::RenderSource();
    bd->SharedMetalContext.fontTexture = nil;
    io.Fonts->SetTexID(0);
}

bool ImGui_ImplMetal_CreateDeviceObjects(id<MTLDevice> device)
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    MTLDepthStencilDescriptor* depthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDescriptor.depthWriteEnabled = NO;
    depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    bd->SharedMetalContext.depthStencilState = [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

    MTLSamplerDescriptor* linearDesc = [[MTLSamplerDescriptor alloc] init];
    linearDesc.minFilter = MTLSamplerMinMagFilterLinear;
    linearDesc.magFilter = MTLSamplerMinMagFilterLinear;
    linearDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    linearDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    bd->SharedMetalContext.linearSampler = [device newSamplerStateWithDescriptor:linearDesc];

    MTLSamplerDescriptor* nearestDesc = [[MTLSamplerDescriptor alloc] init];
    nearestDesc.minFilter = MTLSamplerMinMagFilterNearest;
    nearestDesc.magFilter = MTLSamplerMinMagFilterNearest;
    nearestDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    nearestDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    bd->SharedMetalContext.nearestSampler = [device newSamplerStateWithDescriptor:nearestDesc];

    MTLTextureDescriptor* dummyDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:1
                                                                                         height:1
                                                                                      mipmapped:NO];
    dummyDesc.usage = MTLTextureUsageShaderRead;
    dummyDesc.storageMode = device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
    id<MTLTexture> dummy = [device newTextureWithDescriptor:dummyDesc];
    const uint8_t dummyPixel[4] = {0, 0, 0, 0};
    [dummy replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:dummyPixel bytesPerRow:4];
    bd->SharedMetalContext.dummyTexture = dummy;

    ImGui_ImplMetal_CreateDeviceObjectsForPlatformWindows();
    ImGui_ImplMetal_CreateFontsTexture(device);

    return true;
}

void ImGui_ImplMetal_DestroyDeviceObjects()
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGui_ImplMetal_DestroyFontsTexture();
    ImGui_ImplMetal_InvalidateDeviceObjectsForPlatformWindows();
    [bd->SharedMetalContext.renderPipelineStateCache removeAllObjects];
}

#pragma mark - Multi-viewport support

#import <QuartzCore/CAMetalLayer.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#endif

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

struct ImGuiViewportDataMetal
{
    CAMetalLayer*               MetalLayer;
    id<MTLCommandQueue>         CommandQueue;
    MTLRenderPassDescriptor*    RenderPassDescriptor;
    void*                       Handle = nullptr;
    bool                        FirstFrame = true;
};

static void ImGui_ImplMetal_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGuiViewportDataMetal* data = IM_NEW(ImGuiViewportDataMetal)();
    viewport->RendererUserData = data;

    // PlatformHandleRaw should always be a NSWindow*, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
    // Some back-ends will leave PlatformHandleRaw == 0, in which case we assume PlatformHandle will contain the NSWindow*.
    void* handle = viewport->PlatformHandleRaw ? viewport->PlatformHandleRaw : viewport->PlatformHandle;
    IM_ASSERT(handle != nullptr);

    id<MTLDevice> device = bd->SharedMetalContext.device;
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.device = device;
    layer.framebufferOnly = YES;
    layer.pixelFormat = bd->SharedMetalContext.framebufferDescriptor.colorPixelFormat;
#if TARGET_OS_OSX
    NSWindow* window = (__bridge NSWindow*)handle;
    NSView* view = window.contentView;
    view.layer = layer;
    view.wantsLayer = YES;
#endif
    data->MetalLayer = layer;
    data->CommandQueue = [device newCommandQueue];
    data->RenderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
    data->Handle = handle;
}

static void ImGui_ImplMetal_DestroyWindow(ImGuiViewport* viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == 0 since we didn't create the data for it.
    if (ImGuiViewportDataMetal* data = (ImGuiViewportDataMetal*)viewport->RendererUserData)
        IM_DELETE(data);
    viewport->RendererUserData = nullptr;
}

inline static CGSize MakeScaledSize(CGSize size, CGFloat scale)
{
    return CGSizeMake(size.width * scale, size.height * scale);
}

static void ImGui_ImplMetal_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataMetal* data = (ImGuiViewportDataMetal*)viewport->RendererUserData;
    data->MetalLayer.drawableSize = MakeScaledSize(CGSizeMake(size.x, size.y), viewport->DpiScale);
}

static void ImGui_ImplMetal_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataMetal* data = (ImGuiViewportDataMetal*)viewport->RendererUserData;

#if TARGET_OS_OSX
    void* handle = viewport->PlatformHandleRaw ? viewport->PlatformHandleRaw : viewport->PlatformHandle;
    NSWindow* window = (__bridge NSWindow*)handle;

    // Always render the first frame, regardless of occlusionState, to avoid an initial flicker
    if ((window.occlusionState & NSWindowOcclusionStateVisible) == 0 && !data->FirstFrame)
    {
        // Do not render windows which are completely occluded. Calling -[CAMetalLayer nextDrawable] will hang for
        // approximately 1 second if the Metal layer is completely occluded.
        return;
    }
    data->FirstFrame = false;

    viewport->DpiScale = (float)window.backingScaleFactor;
    if (data->MetalLayer.contentsScale != viewport->DpiScale)
    {
        data->MetalLayer.contentsScale = viewport->DpiScale;
        data->MetalLayer.drawableSize = MakeScaledSize(window.frame.size, viewport->DpiScale);
    }
    viewport->DrawData->FramebufferScale = ImVec2(viewport->DpiScale, viewport->DpiScale);
#endif

    id <CAMetalDrawable> drawable = [data->MetalLayer nextDrawable];
    if (drawable == nil)
        return;

    MTLRenderPassDescriptor* renderPassDescriptor = data->RenderPassDescriptor;
    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    if ((viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0)
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;

    id <MTLCommandBuffer> commandBuffer = [data->CommandQueue commandBuffer];
    id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    ImGui_ImplMetal_RenderDrawData(viewport->DrawData, commandBuffer, renderEncoder);
    [renderEncoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

static void ImGui_ImplMetal_InitMultiViewportSupport()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_ImplMetal_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplMetal_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplMetal_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplMetal_RenderWindow;
}

static void ImGui_ImplMetal_ShutdownMultiViewportSupport()
{
    ImGui::DestroyPlatformWindows();
}

static void ImGui_ImplMetal_CreateDeviceObjectsForPlatformWindows()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    for (int i = 1; i < platform_io.Viewports.Size; i++)
        if (!platform_io.Viewports[i]->RendererUserData)
            ImGui_ImplMetal_CreateWindow(platform_io.Viewports[i]);
}

static void ImGui_ImplMetal_InvalidateDeviceObjectsForPlatformWindows()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    for (int i = 1; i < platform_io.Viewports.Size; i++)
        if (platform_io.Viewports[i]->RendererUserData)
            ImGui_ImplMetal_DestroyWindow(platform_io.Viewports[i]);
}

#pragma mark - MetalBuffer implementation

@implementation MetalBuffer
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer
{
    if ((self = [super init]))
    {
        _buffer = buffer;
        _lastReuseTime = GetMachAbsoluteTimeInSeconds();
    }
    return self;
}
@end

#pragma mark - FramebufferDescriptor implementation

@implementation FramebufferDescriptor
- (instancetype)initWithRenderPassDescriptor:(MTLRenderPassDescriptor*)renderPassDescriptor
{
    if ((self = [super init]))
    {
        _sampleCount = renderPassDescriptor.colorAttachments[0].texture.sampleCount;
        _colorPixelFormat = renderPassDescriptor.colorAttachments[0].texture.pixelFormat;
        _depthPixelFormat = renderPassDescriptor.depthAttachment.texture.pixelFormat;
        _stencilPixelFormat = renderPassDescriptor.stencilAttachment.texture.pixelFormat;
    }
    return self;
}

- (nonnull id)copyWithZone:(nullable NSZone*)zone
{
    FramebufferDescriptor* copy = [[FramebufferDescriptor allocWithZone:zone] init];
    copy.sampleCount = self.sampleCount;
    copy.colorPixelFormat = self.colorPixelFormat;
    copy.depthPixelFormat = self.depthPixelFormat;
    copy.stencilPixelFormat = self.stencilPixelFormat;
    return copy;
}

- (NSUInteger)hash
{
    NSUInteger sc = _sampleCount & 0x3;
    NSUInteger cf = _colorPixelFormat & 0x3FF;
    NSUInteger df = _depthPixelFormat & 0x3FF;
    NSUInteger sf = _stencilPixelFormat & 0x3FF;
    NSUInteger hash = (sf << 22) | (df << 12) | (cf << 2) | sc;
    return hash;
}

- (BOOL)isEqual:(id)object
{
    FramebufferDescriptor* other = object;
    if (![other isKindOfClass:[FramebufferDescriptor class]])
        return NO;
    return other.sampleCount == self.sampleCount      &&
    other.colorPixelFormat   == self.colorPixelFormat &&
    other.depthPixelFormat   == self.depthPixelFormat &&
    other.stencilPixelFormat == self.stencilPixelFormat;
}

@end

#pragma mark - MetalContext implementation

@implementation MetalContext
- (instancetype)init
{
    if ((self = [super init]))
    {
        self.renderPipelineStateCache = [NSMutableDictionary dictionary];
        self.bufferCache = [NSMutableArray array];
        _lastBufferCachePurge = GetMachAbsoluteTimeInSeconds();
    }
    return self;
}

- (MetalBuffer*)dequeueReusableBufferOfLength:(NSUInteger)length device:(id<MTLDevice>)device
{
    uint64_t now = GetMachAbsoluteTimeInSeconds();

    @synchronized(self.bufferCache)
    {
        // Purge old buffers that haven't been useful for a while
        if (now - self.lastBufferCachePurge > 1.0)
        {
            NSMutableArray* survivors = [NSMutableArray array];
            for (MetalBuffer* candidate in self.bufferCache)
                if (candidate.lastReuseTime > self.lastBufferCachePurge)
                    [survivors addObject:candidate];
            self.bufferCache = [survivors mutableCopy];
            self.lastBufferCachePurge = now;
        }

        // See if we have a buffer we can reuse
        MetalBuffer* bestCandidate = nil;
        for (MetalBuffer* candidate in self.bufferCache)
            if (candidate.buffer.length >= length && (bestCandidate == nil || bestCandidate.lastReuseTime > candidate.lastReuseTime))
                bestCandidate = candidate;

        if (bestCandidate != nil)
        {
            [self.bufferCache removeObject:bestCandidate];
            bestCandidate.lastReuseTime = now;
            return bestCandidate;
        }
    }

    // No luck; make a new buffer
    id<MTLBuffer> backing = [device newBufferWithLength:length options:MTLResourceStorageModeShared];
    return [[MetalBuffer alloc] initWithBuffer:backing];
}

// Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptor:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device
{
    NSError* error = nil;

    NSString* shaderSource = [NSString stringWithUTF8String:R"msl(
#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float4x4 projectionMatrix;
};

struct FragUniforms {
    float2 textureSize;
    float2 textureShowingSize;
    float2 renderSize;
    int format;
    int useAreaSample;
    int textureColorRange;
    int pad;
};

struct VertexIn {
    float2 position  [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
    uchar4 color     [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoords;
    float4 color;
};

vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],
                             constant Uniforms &uniforms [[buffer(1)]]) {
    VertexOut out;
    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoords = in.texCoords;
    out.color = float4(in.color) / float4(255.0);
    return out;
}

float4 samplePlane(texture2d<float, access::sample> tex, sampler samp, float2 uv,
                   float2 textureSize, float2 textureShowingSize, float2 renderSize, int useAreaSample) {
    float2 safeRender = max(renderSize, float2(1.0));
    float2 texScale = textureShowingSize / safeRender;
    if (useAreaSample == 0 || (texScale.x < 2.0 && texScale.y < 2.0) || textureSize.x < 1.0 || textureSize.y < 1.0)
        return tex.sample(samp, uv);

    int blockX = (int)texScale.x;
    int blockY = (int)texScale.y;
    if (blockX < 1) blockX = 1;
    if (blockY < 1) blockY = 1;
    if (blockX > 4) blockX = 4;
    if (blockY > 4) blockY = 4;

    float4 color = float4(0.0);
    int weight = 0;
    float2 origin = uv * textureSize;
    for (int i = 0; i < 4; ++i) {
        if (i >= blockX)
            break;
        for (int j = 0; j < 4; ++j) {
            if (j >= blockY)
                break;
            float2 offset = float2(float(i) - float(blockX) * 0.5, float(j) - float(blockY) * 0.5);
            float2 sampleUV = (origin + offset) / textureSize;
            color += tex.sample(samp, sampleUV);
            weight += 1;
        }
    }
    return color / float(max(weight, 1));
}

float3 yuvToRgb(float3 yuv, int colorRange) {
    if (colorRange == 0)
        yuv -= float3(0.0, 0.5, 0.5);
    else {
        yuv.x -= 0.06275;
        yuv -= float3(0.0, 0.50196, 0.50196);
    }
    float3x3 yuv2rgb = float3x3(float3(1.164, 1.164, 1.164),
                                float3(0.0, -0.39465, 2.03211),
                                float3(1.596, -0.81300, 0.0));
    return clamp(yuv2rgb * yuv, float3(0.0), float3(1.0));
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              constant FragUniforms &u [[buffer(0)]],
                              texture2d<float, access::sample> tex0 [[texture(0)]],
                              texture2d<float, access::sample> tex1 [[texture(1)]],
                              texture2d<float, access::sample> tex2 [[texture(2)]],
                              sampler samp [[sampler(0)]]) {
    if (u.format < 0)
        return in.color * tex0.sample(samp, in.texCoords);

    if (u.format == 0)
        return samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample);
    if (u.format == 1) {
        float4 rgba = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample);
        return float4(rgba.b, rgba.g, rgba.r, rgba.a);
    }
    if (u.format == 2) {
        float3 yuv = float3(samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r,
                            samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r,
                            samplePlane(tex2, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r);
        return float4(yuvToRgb(yuv, u.textureColorRange), 1.0);
    }
    if (u.format == 3) {
        float y = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float v = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float uCh = samplePlane(tex2, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        return float4(yuvToRgb(float3(y, uCh, v), u.textureColorRange), 1.0);
    }
    if (u.format == 4) {
        float y = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float2 uv = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).rg;
        return float4(yuvToRgb(float3(y, uv), u.textureColorRange), 1.0);
    }
    if (u.format == 5) {
        float y = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float2 vu = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).rg;
        return float4(yuvToRgb(float3(y, vu.y, vu.x), u.textureColorRange), 1.0);
    }
    if (u.format == 6) {
        float y = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        return float4(y, y, y, 1.0);
    }
    return float4(0.0);
}
)msl"];

    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
    if (library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
        return nil;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

    if (vertexFunction == nil || fragmentFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        return nil;
    }

    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = offsetof(ImDrawVert, pos);
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2; // position
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].offset = offsetof(ImDrawVert, uv);
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2; // texCoords
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].offset = offsetof(ImDrawVert, col);
    vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4; // color
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = self.framebufferDescriptor.sampleCount;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.framebufferDescriptor.colorPixelFormat;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.depthAttachmentPixelFormat = self.framebufferDescriptor.depthPixelFormat;
    pipelineDescriptor.stencilAttachmentPixelFormat = self.framebufferDescriptor.stencilPixelFormat;

    id<MTLRenderPipelineState> renderPipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    return renderPipelineState;
}

@end

static id<MTLTexture> ImGui_ImplMetal_CreateImageTexture(id<MTLDevice> device, unsigned int width, unsigned int height, MTLPixelFormat format)
{
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                                     width:width
                                                                                    height:height
                                                                                 mipmapped:NO];
    desc.usage       = MTLTextureUsageShaderRead;
    desc.storageMode = device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
    return [device newTextureWithDescriptor:desc];
}

static void ImGui_ImplMetal_ReleaseTextureID(uintptr_t& textureID)
{
    if (textureID == 0)
        return;
    id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)(void*)textureID;
    texture                = nil;
    textureID              = 0;
}

namespace ImGui
{
    bool updateImageTexture(ImageData& image, TextureSource& texture)
    {
        ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
        if (bd == nullptr || bd->SharedMetalContext.device == nil || image.width == 0 || image.height == 0)
            return false;

        unsigned int planeCount = getPlaneCount(image.format);
        if (planeCount == 0)
        {
            dbg("unsupported format %d\n", image.format);
            return false;
        }

        id<MTLDevice> device = bd->SharedMetalContext.device;
        for (unsigned int i = 0; i < planeCount; i++)
        {
            unsigned int bytesPerPixel = 0;
            unsigned int width         = 0;
            unsigned int height        = 0;
            if (getPlaneInfo(image.format, image.width, image.height, i, &bytesPerPixel, &width, &height) != 0 || image.plane[i] == nullptr)
            {
                dbg("invalid plane %u\n", i);
                return false;
            }

            MTLPixelFormat pixelFormat = MTLPixelFormatInvalid;
            switch (bytesPerPixel)
            {
                case 1: pixelFormat = MTLPixelFormatR8Unorm; break;
                case 2: pixelFormat = MTLPixelFormatRG8Unorm; break;
                case 4: pixelFormat = MTLPixelFormatRGBA8Unorm; break;
                default:
                    dbg("unsupported bytesPerPixel %u\n", bytesPerPixel);
                    return false;
            }

            id<MTLTexture> nativeTexture = ImGui_ImplMetal_TextureFromID(texture.textureID[i]);
            if (nativeTexture == nil || nativeTexture.width != width || nativeTexture.height != height || nativeTexture.pixelFormat != pixelFormat)
            {
                ImGui_ImplMetal_ReleaseTextureID(texture.textureID[i]);
                nativeTexture = ImGui_ImplMetal_CreateImageTexture(device, width, height, pixelFormat);
                if (nativeTexture == nil)
                    return false;
                texture.textureID[i] = (uintptr_t)(__bridge_retained void*)nativeTexture;
            }

            NSUInteger bytesPerRow = image.stride[i] != 0 ? image.stride[i] : width * bytesPerPixel;
            [nativeTexture replaceRegion:MTLRegionMake2D(0, 0, width, height)
                              mipmapLevel:0
                                withBytes:image.plane[i]
                              bytesPerRow:bytesPerRow];
        }

        for (unsigned int i = planeCount; i < IMGUI_IMAGE_MAX_PLANES; i++)
            ImGui_ImplMetal_ReleaseTextureID(texture.textureID[i]);

        texture.width       = image.width;
        texture.height      = image.height;
        texture.imageFormat = image.format;
        texture.colorRange  = image.colorRange;
        return true;
    }

    void freeTexture(TextureSource& texture)
    {
        for (unsigned int i = 0; i < IMGUI_IMAGE_MAX_PLANES; i++)
            ImGui_ImplMetal_ReleaseTextureID(texture.textureID[i]);
    }
} // namespace ImGui

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
