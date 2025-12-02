Texture2D g_SceneColorTex : register(t0);
Texture2D g_COCTex : register(t1);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

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

cbuffer DepthOfFieldCB : register(b2)
{
    float FocusDistance;
    float FocusRange;
    float COCSize;
    float padding;
}
cbuffer DOFBokehCB : register(b5)
{
    uint BokehRange;
    uint BokehbNear;
    float2 Bokehpadding;
}

cbuffer ViewportConstants : register(b10)
{
    // x: Viewport TopLeftX, y: Viewport TopLeftY
    // z: Viewport Width,   w: Viewport Height
    float4 ViewportRect;
    
    // x: Screen Width      (전체 렌더 타겟 너비)
    // y: Screen Height     (전체 렌더 타겟 높이)
    // z: 1.0f / Screen W,  w: 1.0f / Screen H
    float4 ScreenSize;
}
static const int kernelSampleCount = 22;
static const float2 kernel[kernelSampleCount] =
{
    float2(0, 0),
						float2(0.53333336, 0),
						float2(0.3325279, 0.4169768),
						float2(-0.11867785, 0.5199616),
						float2(-0.48051673, 0.2314047),
						float2(-0.48051673, -0.23140468),
						float2(-0.11867763, -0.51996166),
						float2(0.33252785, -0.4169769),
						float2(1, 0),
						float2(0.90096885, 0.43388376),
						float2(0.6234898, 0.7818315),
						float2(0.22252098, 0.9749279),
						float2(-0.22252095, 0.9749279),
						float2(-0.62349, 0.7818314),
						float2(-0.90096885, 0.43388382),
						float2(-1, 0),
						float2(-0.90096885, -0.43388376),
						float2(-0.6234896, -0.7818316),
						float2(-0.22252055, -0.974928),
						float2(0.2225215, -0.9749278),
						float2(0.6234897, -0.7818316),
						float2(0.90096885, -0.43388376),
};
float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_SceneColorTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
    float2 COC = g_COCTex.Sample(g_PointClampSample, uv).rg;

    float3 FinalColor = float3(0, 0, 0);
    float TotalWeight = 0;
    
    for (int i = 0; i < kernelSampleCount; i++)
    {
        float2 CurUV = uv + kernel[i] * InvTexSize * COCSize * BokehRange;
        float2 CurCOC = g_COCTex.Sample(g_LinearClampSample, CurUV).rg;
        if (BokehbNear)
        {
            if (true)
            {
                TotalWeight++;
                FinalColor += g_SceneColorTex.Sample(g_LinearClampSample, CurUV).rgb;
            }
        }
        else
        {
            if (true)
            {
                TotalWeight++;
                FinalColor += g_SceneColorTex.Sample(g_LinearClampSample, CurUV).rgb;
            }
        }
    }
    
    FinalColor = FinalColor / TotalWeight;
    return float4(FinalColor, 1);
}