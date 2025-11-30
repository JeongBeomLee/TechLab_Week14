#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_InputTexture : register(t0);    // Near 또는 Far 필드 (RGB=Color, A=CoC)
Texture2D<float4> g_CoCTexture : register(t1);      // 원본 CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

// 3차 패스: 120도 방향 블러
#define BLUR_DIRECTION float2(-0.5, 0.8660254)

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // 입력 텍스처에서 현재 픽셀의 색상과 CoC 읽기
    float4 centerSample = g_InputTexture.Sample(g_LinearSampler, texcoord);
    float centerCoC = centerSample.a;  // 알파 채널에 CoC가 저장되어 있음

    // CoC가 거의 0이면 (이 필드에 속하지 않는 영역) 그대로 반환
    if (centerCoC < 0.01)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    // Hexagonal blur 적용
    float4 colorSum = float4(0.0, 0.0, 0.0, 0.0);
    float weightSum = 0.0;

    // cbuffer에서 샘플 개수와 블러 스케일 가져오기
    int sampleCount = max(1, FDepthOfFieldBuffer.BlurSampleCount);
    int kernelRadius = max(1, sampleCount / 2);

    // 텍스처 해상도 (ScreenSize.xy에서 가져오기)
    float2 pixelSize = 1.0 / ScreenSize.xy;

    // CoC에 블러 스케일 적용
    // NearBlurScale은 0~2 범위, 여기에 추가로 10배 스케일 적용
    float blurScale = centerCoC * FDepthOfFieldBuffer.NearBlurScale;
    float invKernelRadius = 1.0 / max(1.0, float(kernelRadius));  // 0 나눗셈 방지

    // 방향성 블러 적용 (동적 루프)
    [loop]
    for (int i = -kernelRadius; i <= kernelRadius; i++)
    {
        // 오프셋 계산: 방향 * (인덱스 / 반경) * 픽셀크기 * 블러스케일
        float t = float(i) * invKernelRadius;  // -1 ~ 1
        float2 offset = BLUR_DIRECTION * t * pixelSize.x * blurScale;
        float2 sampleUV = texcoord + offset;

        float4 sampleColor = g_InputTexture.Sample(g_LinearSampler, sampleUV);
        float sampleCoC = sampleColor.a;

        // Gaussian-like 가중치
        float weight = exp(-2.0 * t * t);  // 중심에서 멀어질수록 감소
        weight *= (sampleCoC > 0.01) ? 1.0 : 0.0;  // 유효한 CoC만 포함

        colorSum += sampleColor * weight;
        weightSum += weight;
    }

    // 가중 평균 계산
    if (weightSum > 0.0)
    {
        colorSum /= weightSum;
        colorSum.a = centerCoC;  // CoC 값 유지
        return colorSum;
    }

    return centerSample;
}
