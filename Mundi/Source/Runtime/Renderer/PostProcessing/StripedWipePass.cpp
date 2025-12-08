#include "pch.h"
#include "StripedWipePass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FStripedWipePass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // 1) 스왑 + SRV 언바인드 관리 (SceneColorSource 한 장만 읽음)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/1);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false); // 전화면 덮어쓰기

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* StripedWipePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/StripedWipe_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !StripedWipePS || !StripedWipePS->GetPixelShader())
    {
        UE_LOG("StripedWipe용 셰이더 없음!\n");
        return;
    }
    RHIDevice->PrepareShader(FullScreenTriangleVS, StripedWipePS);

    // 4) SRV / Sampler (현재 SceneColorSource)
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

    if (!SceneSRV || !LinearClampSamplerState)
    {
        UE_LOG("StripedWipe: Scene SRV / LinearClamp Sampler is null!\n");
        return;
    }

    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &LinearClampSamplerState);

    // 5) 상수 버퍼 업데이트 (Progress/StripeCount/Color/Weight)
    FStripedWipeBufferType StripedWipeConstant;
    StripedWipeConstant.WipeColor = M.Payload.Color;
    StripedWipeConstant.Progress = M.Payload.Params0.X;
    StripedWipeConstant.StripeCount = M.Payload.Params0.Y;
    StripedWipeConstant.Weight = M.Weight;

    RHIDevice->SetAndUpdateConstantBuffer(StripedWipeConstant);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
