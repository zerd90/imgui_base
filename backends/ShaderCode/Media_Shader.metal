#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 projectionMatrix;
};

struct FragUniforms
{
    float2 textureSize;
    float2 textureShowingSize;
    float2 renderSize;
    int    format;
    int    useAreaSample;
    int    textureColorRange;
    int    pad;
};

struct VertexIn
{
    float2 position [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
    uchar4 color [[attribute(2)]];
};

struct VertexOut
{
    float4 position [[position]];
    float2 texCoords;
    float4 color;
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]], constant Uniforms &uniforms [[buffer(1)]])
{
    VertexOut out;
    out.position  = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoords = in.texCoords;
    out.color     = float4(in.color) / float4(255.0);
    return out;
}

float4 samplePlane(texture2d<float, access::sample> tex, sampler samp, float2 uv, float2 textureSize, float2 textureShowingSize,
                   float2 renderSize, int useAreaSample)
{
    float2 safeRender = max(renderSize, float2(1.0));
    float2 texScale   = textureShowingSize / safeRender;
    if (useAreaSample == 0 || (texScale.x < 2.0 && texScale.y < 2.0) || textureSize.x < 1.0 || textureSize.y < 1.0)
        return tex.sample(samp, uv);

    int blockX = (int)texScale.x;
    int blockY = (int)texScale.y;
    if (blockX < 1)
        blockX = 1;
    if (blockY < 1)
        blockY = 1;
    if (blockX > 4)
        blockX = 4;
    if (blockY > 4)
        blockY = 4;

    float4 color  = float4(0.0);
    int    weight = 0;
    float2 origin = uv * textureSize;
    for (int i = 0; i < 4; ++i)
    {
        if (i >= blockX)
            break;
        for (int j = 0; j < 4; ++j)
        {
            if (j >= blockY)
                break;
            float2 offset   = float2(float(i) - float(blockX) * 0.5, float(j) - float(blockY) * 0.5);
            float2 sampleUV = (origin + offset) / textureSize;
            color += tex.sample(samp, sampleUV);
            weight += 1;
        }
    }
    return color / float(max(weight, 1));
}

float3 yuvToRgb(float3 yuv, int colorRange)
{
    if (colorRange == 0)
        yuv -= float3(0.0, 0.5, 0.5);
    else
    {
        yuv.x -= 0.06275;
        yuv -= float3(0.0, 0.50196, 0.50196);
    }
    float3x3 yuv2rgb = float3x3(float3(1.164, 1.164, 1.164), float3(0.0, -0.39465, 2.03211), float3(1.596, -0.81300, 0.0));
    return clamp(yuv2rgb * yuv, float3(0.0), float3(1.0));
}

fragment float4 fragment_main(VertexOut in [[stage_in]], constant FragUniforms &u [[buffer(0)]],
                              texture2d<float, access::sample> tex0 [[texture(0)]],
                              texture2d<float, access::sample> tex1 [[texture(1)]],
                              texture2d<float, access::sample> tex2 [[texture(2)]], sampler samp [[sampler(0)]])
{
    if (u.format < 0)
        return in.color * tex0.sample(samp, in.texCoords);

    if (u.format == 0)
        return samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample);
    if (u.format == 1)
    {
        float4 rgba = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample);
        return float4(rgba.b, rgba.g, rgba.r, rgba.a);
    }
    if (u.format == 2)
    {
        float3 yuv =
            float3(samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r,
                   samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r,
                   samplePlane(tex2, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r);
        return float4(yuvToRgb(yuv, u.textureColorRange), 1.0);
    }
    if (u.format == 3)
    {
        float y   = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float v   = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float uCh = samplePlane(tex2, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        return float4(yuvToRgb(float3(y, uCh, v), u.textureColorRange), 1.0);
    }
    if (u.format == 4)
    {
        float  y  = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float2 uv = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).rg;
        return float4(yuvToRgb(float3(y, uv), u.textureColorRange), 1.0);
    }
    if (u.format == 5)
    {
        float  y  = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        float2 vu = samplePlane(tex1, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).rg;
        return float4(yuvToRgb(float3(y, vu.y, vu.x), u.textureColorRange), 1.0);
    }
    if (u.format == 6)
    {
        float y = samplePlane(tex0, samp, in.texCoords, u.textureSize, u.textureShowingSize, u.renderSize, u.useAreaSample).r;
        return float4(y, y, y, 1.0);
    }
    return float4(0.0);
}
