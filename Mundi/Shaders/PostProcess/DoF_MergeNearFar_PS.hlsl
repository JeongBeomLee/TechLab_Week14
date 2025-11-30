#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_NearBlurTexture : register(t0);  // Near 필드 블러 결과 (RGB=Color, A=CoC)
Texture2D<float4> g_FarBlurTexture : register(t1);   // Far 필드 블러 결과 (RGB=Color, A=CoC)
Texture2D<float4> g_CoCTexture : register(t2);       // 원본 CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // Near와 Far 블러 결과 읽기 (알파 채널에 CoC 포함)
    float4 nearBlur = g_NearBlurTexture.Sample(g_LinearSampler, texcoord);
    float4 farBlur = g_FarBlurTexture.Sample(g_LinearSampler, texcoord);
    float4 cocSample = g_CoCTexture.Sample(g_LinearSampler, texcoord);

    float nearCoC = cocSample.g;  // 원본 CoC 맵에서 Near CoC
    float farCoC = cocSample.r;   // 원본 CoC 맵에서 Far CoC

    // Near가 Far보다 우선순위가 높음 (전경이 배경을 가림)
    float4 finalBlur;
    if (nearCoC > 0.01)
    {
        // Near 영역: nearBlur 사용
        finalBlur = nearBlur;
        finalBlur.a = nearCoC;  // CoC 값 설정
    }
    else if (farCoC > 0.01)
    {
        // Far 영역: farBlur 사용
        finalBlur = farBlur;
        finalBlur.a = farCoC;  // CoC 값 설정
    }
    else
    {
        // 초점 영역: 블러 없음
        finalBlur = float4(0, 0, 0, 0);
    }

    return finalBlur;
}
