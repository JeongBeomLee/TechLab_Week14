#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

/**
 * 사선 줄무늬로 화면을 덮는 와이프 전환 효과
 * 좌상단에서 우하단으로 검은색 줄무늬가 진행
 */
class UCamMod_StripedWipe : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_StripedWipe, UCameraModifierBase)

    UCamMod_StripedWipe() = default;
    virtual ~UCamMod_StripedWipe() = default;

    FLinearColor WipeColor = FLinearColor(0, 0, 0, 1); // 와이프 색상 (기본: 검은색)
    float StartProgress = 0.f;   // 시작 진행도 [0..1]
    float EndProgress = 1.f;     // 종료 진행도 [0..1]
    float Elapsed = 0.f;         // 진행 시간(초)
    float StripeCount = 10.f;    // 줄무늬 개수

    float CurrentProgress = 0.f; // 이번 프레임 출력 진행도

    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override
    {
        if (!bEnabled) return;

        Elapsed += DeltaTime;
        const float T = (Duration <= 0.f) ? 1.f : FMath::Clamp(Elapsed / Duration, 0.f, 1.f);

        CurrentProgress = FMath::Lerp(StartProgress, EndProgress, T);

        // 완료 처리
        if (T >= 1.f) bEnabled = false;
    }

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override
    {
        if (!bEnabled && CurrentProgress <= 0.f) return;
        if (CurrentProgress <= 0.f) return;

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::StripedWipe;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        M.Payload.Color = WipeColor;
        M.Payload.Params0 = FVector4(CurrentProgress, StripeCount, 0, 0);

        Out.Add(M);
    }
};
