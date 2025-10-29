
#define TextureFormat_Normal -1
#define TextureFormat_RGBA 0
#define TextureFormat_BGRA 1
#define TextureFormat_YUVPlaner 2
#define TextureFormat_YVUPlaner 3
#define TextureFormat_NV12 4
#define TextureFormat_NV21 5
#define TextureFormat_Gray 6

#define YUV_ColorRange_0_255 0
#define YUV_ColorRange_16_235 1

struct PS_INPUT {
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 texCoord : TEXCOORD0;
};

sampler gSampler;
Texture2D gTextures[4];
cbuffer ConstantBuffer : register(b0) {
  float2 textureSize;
  float2 textureShowingSize;
  float2 renderSize;
  int format;
  int useAreaSample;
  int textureColorRange;
};

float3 yuvToRgb(float3 yuv) {
  if (textureColorRange == YUV_ColorRange_0_255)
    yuv = yuv - float3(0, 0.5, 0.5);
  else {
    yuv.r = (yuv.r - 0.06275);
    yuv = yuv - float3(0, 0.50196, 0.50196);
  }
  float3x3 yuv2rgb = float3x3(float3(1.164383, 1.164383, 1.164383),
                              float3(0.000000, -0.391762, 2.017232),
                              float3(1.596027, -0.812968, 0.000000));
  float3 rgb = mul(yuv, yuv2rgb);
  return rgb;
}

float4 simpleSampleTexture(int idx, float2 uv) {
  return gTextures[idx].Sample(gSampler, uv);
}

float4 sampleTexture(int idx, float2 uv) {
  float2 texScale = textureShowingSize / renderSize;

  if (useAreaSample == 0 || texScale.x < 2.0 || texScale.y < 2.0) {
    return simpleSampleTexture(idx, uv);
  }

  int2 blockSize = texScale;

  if (blockSize.x > 4)
    blockSize.x = 4;
  if (blockSize.y > 4)
    blockSize.y = 4;

  int weight = 0;
  float4 color = float4(0.0, 0.0, 0.0, 0.0);

  float2 origUv = uv * textureSize;

  float2 offset;
  for (int i = 0; i < blockSize.x; i++) {
    offset.x = i - blockSize.x / 2;
    for (int j = 0; j < blockSize.y; j++) {
      offset.y = j - blockSize.y / 2;
      float2 sampleUV = origUv + offset;
      sampleUV /= textureSize;

      float4 sampleColor = simpleSampleTexture(idx, sampleUV);
      color += sampleColor;
      weight += 1;
    }
  }

  return color / weight;
}

float4 main(PS_INPUT input) : SV_Target {

  if (format == TextureFormat_Normal) {
    return input.col * gTextures[0].Sample(gSampler, input.texCoord);
  } else if (format == TextureFormat_RGBA) {

    return sampleTexture(0, input.texCoord);

  } else if (format == TextureFormat_BGRA) {

    float4 rgba = sampleTexture(0, input.texCoord);

    return float4(rgba.b, rgba.g, rgba.r, rgba.a);

  } else if (format == TextureFormat_YUVPlaner) {

    float y = sampleTexture(0, input.texCoord).r;
    float u = sampleTexture(1, input.texCoord).r;
    float v = sampleTexture(2, input.texCoord).r;

    float3 yuv = float3(y, u, v);
    float3 rgb = yuvToRgb(yuv);

    return float4(rgb, 1.0);

  } else if (format == TextureFormat_YVUPlaner) {

    float y = sampleTexture(0, input.texCoord).r;
    float v = sampleTexture(1, input.texCoord).r;
    float u = sampleTexture(2, input.texCoord).r;

    float3 yuv = float3(y, u, v);
    float3 rgb = yuvToRgb(yuv);

    return float4(rgb, 1.0);

  } else if (format == TextureFormat_NV12) {

    float y = sampleTexture(0, input.texCoord).r;
    float2 uv = sampleTexture(1, input.texCoord).rg;
    float3 yuv = float3(y, uv);
    float3 rgb = yuvToRgb(yuv);

    return float4(rgb, 1.0);

  } else if (format == TextureFormat_NV21) {

    float y = sampleTexture(0, input.texCoord).r;
    float2 uv = sampleTexture(1, input.texCoord).gr;

    float3 yuv = float3(y, uv);
    float3 rgb = yuvToRgb(yuv);

    return float4(rgb, 1.0);
  } else if (format == TextureFormat_Gray) {

    float y = sampleTexture(0, input.texCoord).r;

    return float4(y, y, y, 1.0);
  }

  return float4(0.0, 0.0, 0.0, 1.0);
}
