Texture2D    g_SceneColorTex   : register(t0);

SamplerState g_LinearClampSample  : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer StripedWipeCB : register(b2)
{
    float4 WipeColor;      // rgb: 색, a: 사용 안함
    float  Progress;       // 0~1 (시간에 따라 변화)
    float  StripeCount;    // 줄무늬 개수
    float  Weight;         // 0~1 (모디파이어 가중치)
    float  _Pad;           // 정렬용
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 sceneColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);

    // 수평 줄무늬: Y 좌표를 기준으로 줄무늬 생성
    float stripeIndex = floor(input.texCoord.y * StripeCount);

    // 홀수/짝수 줄무늬에 따라 방향 결정 (0: 왼->오, 1: 오->왼)
    float isOddStripe = fmod(stripeIndex, 2.0);

    // X 좌표 (0~1)
    float xCoord = input.texCoord.x;

    // 홀수 줄무늬는 오른쪽에서 왼쪽으로 진행 (1-x)
    float stripeProgress = isOddStripe > 0.5 ? (1.0 - xCoord) : xCoord;

    // 전체 진행도와 비교
    // Progress가 0일 때 아무것도 안 보임, 1일 때 전체가 덮임
    float wipeAmount = saturate((Progress - stripeProgress) * 2.0 + 1.0);

    // 최종 색상 블렌딩
    float3 outColor = lerp(sceneColor.rgb, WipeColor.rgb, wipeAmount * Weight);

    return float4(outColor, sceneColor.a);
}
