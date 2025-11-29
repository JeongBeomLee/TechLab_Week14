#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_InputTexture : register(t0);    // 이전 패스의 결과 또는 원본 씬 컬러
Texture2D<float4> g_CoCTexture : register(t1);      // CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

// 1차 패스: 0도 방향 블러 (수평)
#define BLUR_DIRECTION float2(1.0, 0.0) 

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // CoC 맵에서 현재 픽셀의 CoC 값과 깊이 값을 읽어옴
    float4 cocSample = g_CoCTexture.Sample(g_LinearSampler, texcoord);
    float farCoC = cocSample.r;
    float nearCoC = cocSample.g;

    // Near, Far 중 더 큰 CoC 값을 블러 반경으로 사용
    float cocRadius = max(nearCoC, farCoC);

    // 초점 영역(CoC가 거의 0)이면 원본 이미지 그대로 사용
    if (cocRadius < 0.01)
    {
        float4 originalColor = g_InputTexture.Sample(g_LinearSampler, texcoord);
        originalColor.a = cocSample.a;
        return originalColor;
    }
    
    float4 finalColor = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    int sampleCount = FDepthOfFieldBuffer.BlurSampleCount;
    if (sampleCount == 0) sampleCount = 1;

    float blurScale = cocRadius * FDepthOfFieldBuffer.MaxCoc;

    [loop]
    for (int i = -sampleCount; i <= sampleCount; ++i)
    {
        float weight = (sampleCount - abs(i)) / (float)sampleCount;
        float2 offset = BLUR_DIRECTION * (i / (float)sampleCount) * blurScale * ScreenSize.zw;
        finalColor += g_InputTexture.Sample(g_LinearSampler, texcoord + offset) * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0.0)
    {
        finalColor /= totalWeight;
    }
    else
    {
        finalColor = g_InputTexture.Sample(g_LinearSampler, texcoord);
    }
    
    finalColor.a = cocSample.a;
    
    return finalColor;
}