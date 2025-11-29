#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_SceneColorSource : register(t0); // 원본 씬 컬러
Texture2D<float4> g_DofBlurMap : register(t1);     // 최종 블러 결과
Texture2D<float4> g_DofCocMap : register(t2);      // CoC 맵 (R=Far, G=Near)

SamplerState g_LinearSampler : register(s0);

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float4 originalColor = g_SceneColorSource.Sample(g_LinearSampler, texcoord);
    float4 blurredColor = g_DofBlurMap.Sample(g_LinearSampler, texcoord);
    float4 cocSample = g_DofCocMap.Sample(g_LinearSampler, texcoord);

    float farCoC = cocSample.r;
    float nearCoC = cocSample.g;

    // Near/Far CoC 중 더 큰 값을 최종 블러 강도(혼합 비율)로 사용
    float blendFactor = max(nearCoC, farCoC);
    
    // FDepthOfFieldBuffer.Weight를 사용하여 전체 DoF 효과의 강도를 조절
    // Weight가 1.0이면 계산된 CoC 값을 그대로 사용
    // Weight가 0.0이면 DoF 효과 없음
    blendFactor *= FDepthOfFieldBuffer.Weight;
    
    // 블러 정도에 따라 원본 이미지와 블러 이미지를 혼합
    // saturate를 통해 blendFactor가 0~1 범위를 벗어나지 않도록 함
    float4 finalColor = lerp(originalColor, blurredColor, saturate(blendFactor));

    // 알파 채널은 원본의 값을 유지하거나, 필요에 따라 1.0으로 설정
    finalColor.a = originalColor.a;

    return finalColor;
}
