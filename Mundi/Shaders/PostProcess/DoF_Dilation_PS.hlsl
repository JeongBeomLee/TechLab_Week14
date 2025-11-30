#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_CoCTexture : register(t0);  // CoC Map (R=Far, G=Near, B=?, A=Depth)
SamplerState g_PointSampler : register(s0);

// Dilation: 주변 픽셀의 최대 CoC를 찾아서 블러 영역 확장
// Near CoC는 최대값, Far CoC는 최대값을 사용하여 경계를 부드럽게 처리
float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // 픽셀 오프셋 (텍스처 공간)
    float2 pixelSize = ScreenSize.zw; // (1/width, 1/height)

    // 중심 픽셀의 CoC 값
    float4 centerCoC = g_CoCTexture.Sample(g_PointSampler, texcoord);

    // 초기값: 중심 픽셀 값
    float maxFarCoC = centerCoC.r;
    float maxNearCoC = centerCoC.g;

    // 5x5 영역에서 최대 CoC 값 찾기 (블러 영역 확장)
    // 더 넓은 커널로 경계를 부드럽게 처리
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            // 중심 픽셀은 이미 처리했으므로 스킵
            if (x == 0 && y == 0)
                continue;

            float2 offset = float2(x, y) * pixelSize;
            float4 sampleCoC = g_CoCTexture.Sample(g_PointSampler, texcoord + offset);

            // 최대값 찾기 (블러 영역이 주변으로 확장됨)
            maxFarCoC = max(maxFarCoC, sampleCoC.r);
            maxNearCoC = max(maxNearCoC, sampleCoC.g);
        }
    }

    // Dilated CoC 반환 (depth는 원본 유지)
    return float4(maxFarCoC, maxNearCoC, centerCoC.b, centerCoC.a);
}
