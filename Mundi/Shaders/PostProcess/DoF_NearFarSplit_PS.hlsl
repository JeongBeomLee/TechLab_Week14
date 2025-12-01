#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_SceneColorSource : register(t0); // 원본 씬 컬러
Texture2D<float4> g_CoCTexture : register(t1);      // CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

struct PSOutput
{
    float4 NearField : SV_Target0;  // DofNearTarget
    float4 FarField : SV_Target1;   // DofFarTarget
};

PSOutput mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0)
{
    PSOutput output;

    // 원본 씬 컬러와 CoC 값 읽기
    float4 sceneColor = g_SceneColorSource.Sample(g_LinearSampler, texcoord);
    float4 cocSample = g_CoCTexture.Sample(g_LinearSampler, texcoord);

    float farCoC = cocSample.r;   // Far 필드 CoC
    float nearCoC = cocSample.g;  // Near 필드 CoC
    float depth = cocSample.a;    // 깊이 값

    // Near 필드: nearCoC가 있는 영역만 색상을 가지고, 나머지는 투명/검은색
    // Alpha에 nearCoC를 저장하여 블러 강도 정보 유지
    // 임계점 상향 (0.01 -> 0.05) - 약한 블러 영역 제외
    if (nearCoC > 0.05)
    {
        output.NearField = float4(sceneColor.rgb, nearCoC);
    }
    else
    {
        output.NearField = float4(0.0, 0.0, 0.0, 0.0);
    }

    // Far 필드: farCoC가 있는 영역만 색상을 가지고, 나머지는 투명/검은색
    // Alpha에 farCoC를 저장하여 블러 강도 정보 유지
    // 임계점 상향 (0.01 -> 0.05) - 약한 블러 영역 제외
    if (farCoC > 0.05)
    {
        output.FarField = float4(sceneColor.rgb, farCoC);
    }
    else
    {
        output.FarField = float4(0.0, 0.0, 0.0, 0.0);
    }

    return output;
}
