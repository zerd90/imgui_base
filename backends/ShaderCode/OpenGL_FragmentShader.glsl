#version 330 core
#define TextureFormat_Normal    -1
#define TextureFormat_RGBA      0
#define TextureFormat_BGRA      1
#define TextureFormat_YUVPlaner 2
#define TextureFormat_YVUPlaner 3
#define TextureFormat_NV12      4
#define TextureFormat_NV21      5
#define TextureFormat_Gray      6

#define YUV_ColorRange_0_255  0
#define YUV_ColorRange_16_235 1

uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform sampler2D Texture2;
uniform int       TextureFormat;
uniform int       TextureColorRange;
uniform vec2      TextureShowingSize;
uniform vec2      TextureSize;
uniform vec2      RenderSize;
uniform int       UseAreaSample;

in vec2  Frag_UV;
in vec4  Frag_Color;
out vec4 Out_Color;

vec3 yuvToRgb(vec3 yuv)
{
    if (TextureColorRange == YUV_ColorRange_0_255)
    {
        yuv = yuv - vec3(0, 0.5, 0.5);
    }
    else
    {
        yuv.r = (yuv.r - 0.06275);
        yuv   = yuv - vec3(0, 0.50196, 0.50196);
    }
    vec3 rgb = mat3(1.164, 1.164, 1.164, 0., -0.39465, 2.03211, 1.596, -0.81300, 0) * yuv;

    return clamp(rgb, 0.0, 1.0);
}

vec4 simpleSampleTexture(int idx, vec2 uv)
{
    if (idx == 0)
        return texture(Texture0, uv);
    else if (idx == 1)
        return texture(Texture1, uv);
    else if (idx == 2)
        return texture(Texture2, uv);
}

vec4 sampleTexture(int idx, vec2 uv)
{
    vec2 texScale = TextureShowingSize / RenderSize;

    if (UseAreaSample == 0 || (texScale.x < 2.0 && texScale.y < 2.0))
    {
        return simpleSampleTexture(idx, uv);
    }

    vec2 blockSize = texScale;

    if (blockSize.x > 4)
        blockSize.x = 4;
    if (blockSize.y > 4)
        blockSize.y = 4;

    int  weight = 0;
    vec4 color  = vec4(0.0, 0.0, 0.0, 0.0);

    vec2 origUv;
    origUv.x = uv.x * TextureSize.x;
    origUv.y = uv.y * TextureSize.y;

    vec2 offset;
    for (int i = 0; i < blockSize.x; i++)
    {
        offset.x = i - blockSize.x / 2;
        for (int j = 0; j < blockSize.y; j++)
        {
            offset.y      = j - blockSize.y / 2;
            vec2 sampleUV = origUv + offset;
            sampleUV.x /= TextureSize.x;
            sampleUV.y /= TextureSize.y;

            vec4 sampleColor = simpleSampleTexture(idx, sampleUV);
            color += sampleColor;
            weight += 1;
        }
    }

    return color / weight;
}

void main()
{
    if (TextureFormat == TextureFormat_Normal)
    {
        Out_Color = Frag_Color * texture(Texture0, Frag_UV);
    }
    else if (TextureFormat == TextureFormat_RGBA)
    {
        Out_Color = sampleTexture(0, Frag_UV);
    }
    else if (TextureFormat == TextureFormat_BGRA)
    {
        vec4 rgba = sampleTexture(0, Frag_UV);

        Out_Color = vec4(rgba.b, rgba.g, rgba.r, rgba.a);
    }
    else if (TextureFormat == TextureFormat_YUVPlaner)
    {
        vec3 yuv;

        yuv.r = sampleTexture(0, Frag_UV).r;
        yuv.g = sampleTexture(1, Frag_UV).r;
        yuv.b = sampleTexture(2, Frag_UV).r;

        vec3 rgb = yuvToRgb(yuv);

        Out_Color = vec4(rgb, 1.0);
    }
    else if (TextureFormat == TextureFormat_YVUPlaner)
    {
        float y = sampleTexture(0, Frag_UV).r;
        float v = sampleTexture(1, Frag_UV).r;
        float u = sampleTexture(2, Frag_UV).r;

        vec3 yuv = vec3(y, u, v);
        vec3 rgb = yuvToRgb(yuv);

        Out_Color = vec4(rgb, 1.0);
    }
    else if (TextureFormat == TextureFormat_NV12)
    {
        float y   = sampleTexture(0, Frag_UV).r;
        vec2  uv  = sampleTexture(1, Frag_UV).rg;
        vec3  yuv = vec3(y, uv);
        vec3  rgb = yuvToRgb(yuv);

        Out_Color = vec4(rgb, 1.0);
    }
    else if (TextureFormat == TextureFormat_NV21)
    {
        float y  = sampleTexture(0, Frag_UV).r;
        vec2  uv = sampleTexture(1, Frag_UV).gr;

        vec3 yuv = vec3(y, uv);
        vec3 rgb = yuvToRgb(yuv);

        Out_Color = vec4(rgb, 1.0);
    }
    else if (TextureFormat == TextureFormat_Gray)
    {
        float y = sampleTexture(0, Frag_UV).r;

        Out_Color = vec4(y, y, y, 1.0);
    }
    else
    {
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
