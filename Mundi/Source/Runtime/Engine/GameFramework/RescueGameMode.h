// ----------------------------------------------------------------------------
// RescueGameMode.h
// 구조 미션 게임 모드
// ----------------------------------------------------------------------------
#pragma once

#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "ARescueGameMode.generated.h"

class STextBlock;
class SProgressBar;
class SPanel;

/**
 * ARescueGameMode
 *
 * 구조 미션을 위한 게임 모드입니다.
 * - 산소 게이지: 시간에 따라 감소, 산소통으로 최대치 증가
 * - 물 게이지: 물 마법 사용 시 감소, 소화기로 충전
 * - 플레이어 초기 아이템 설정 가능
 */
UCLASS(DisplayName="구조 게임 모드", Description="구조 미션을 위한 게임 모드입니다.")
class ARescueGameMode : public AGameModeBase
{
public:
    GENERATED_REFLECTION_BODY()

    ARescueGameMode();

protected:
    ~ARescueGameMode() override = default;

public:
    // ────────────────────────────────────────────────
    // 생명주기
    // ────────────────────────────────────────────────

    void BeginPlay() override;
    void EndPlay() override;
    void Tick(float DeltaSeconds) override;

    // ────────────────────────────────────────────────
    // 스폰 설정
    // ────────────────────────────────────────────────

    /** 플레이어 스케일 (기본값 1.5) */
    UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="플레이어의 스케일입니다.")
    float PlayerSpawnScale;

    /** 스폰 위치로 사용할 Actor의 이름 */
    UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="스폰 위치로 사용할 Actor의 이름입니다.")
    FName SpawnActorName;

    /** 스폰 위치로 사용할 Actor의 태그 */
    UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="스폰 위치로 사용할 Actor의 태그입니다.")
    FName SpawnActorTag;

    // ────────────────────────────────────────────────
    // 산소 시스템
    // ────────────────────────────────────────────────

    /** 기본 산소량 */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="기본 산소량입니다.")
    float BaseOxygen;

    /** 산소통당 추가 산소량 */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="산소통 하나당 추가되는 산소량입니다.")
    float OxygenPerTank;

    /** 초당 산소 감소율 */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="초당 산소 감소량입니다.")
    float OxygenDecreaseRate;

    /** 달리기 시 산소 감소 배율 */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="달리기 시 산소 감소 배율입니다.")
    float RunningOxygenMultiplier;

    /** 소방복 착용 시 산소 감소 배율 */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="소방복 착용 시 산소 감소 배율입니다. (기본값 0.5 = 절반)")
    float FireSuitOxygenMultiplier;

    /** 달리기 판정 속도 비율 (MaxWalkSpeed 기준) */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="이 비율 이상의 속도면 달리기로 판정합니다.")
    float RunningSpeedThreshold;

    /** 산소 경고 임계값 (이 값 미만이면 빨간색 표시) */
    UPROPERTY(EditAnywhere, Category="Oxygen", Tooltip="이 값 미만이면 산소 게이지가 빨간색으로 표시됩니다.")
    float OxygenWarningThreshold;

    /** 현재 산소량 */
    float CurrentOxygen;

    /** 최대 산소량 (BaseOxygen + OxygenPerTank * 산소통 개수) */
    float MaxOxygen;

    // ────────────────────────────────────────────────
    // 물 시스템
    // ────────────────────────────────────────────────

    /** 기본 물 게이지량 */
    UPROPERTY(EditAnywhere, Category="Water", Tooltip="기본 물 게이지량입니다.")
    float BaseWater;

    /** 초당 물 감소율 (물 마법 사용 시) */
    UPROPERTY(EditAnywhere, Category="Water", Tooltip="물 마법 사용 시 초당 물 감소량입니다.")
    float WaterDecreaseRate;

    /** 현재 물 게이지량 */
    float CurrentWater;

    /** 남은 소화기 개수 */
    int32 FireExtinguisherCount;

    /** 물 마법 사용 중 여부 */
    bool bIsUsingWaterMagic;

    // ────────────────────────────────────────────────
    // 더미 아이템 설정 (테스트용)
    // ────────────────────────────────────────────────

    /** 시작 시 산소통 개수 */
    UPROPERTY(EditAnywhere, Category="DummyItems", Tooltip="시작 시 보유할 산소통 개수입니다.")
    int32 InitialOxygenTankCount;

    /** 시작 시 소화기 개수 */
    UPROPERTY(EditAnywhere, Category="DummyItems", Tooltip="시작 시 보유할 소화기 개수입니다.")
    int32 InitialFireExtinguisherCount;

    /** 시작 시 소방복 보유 여부 */
    UPROPERTY(EditAnywhere, Category="DummyItems", Tooltip="시작 시 소방복 보유 여부입니다.")
    bool bInitialHasFireSuit;

    // ────────────────────────────────────────────────
    // 구조 시스템
    // ────────────────────────────────────────────────

    /** 구조 구역으로 사용할 Actor의 이름 */
    UPROPERTY(EditAnywhere, Category="Rescue", Tooltip="구조 구역으로 사용할 Actor의 이름입니다.")
    FName SafeZoneActorName;

    /** 구조 구역으로 사용할 Actor의 태그 */
    UPROPERTY(EditAnywhere, Category="Rescue", Tooltip="구조 구역으로 사용할 Actor의 태그입니다.")
    FName SafeZoneActorTag;

    /** 구조 구역 반경 (미터) */
    UPROPERTY(EditAnywhere, Category="Rescue", Tooltip="구조 구역의 반경(미터)입니다.")
    float SafeZoneRadius;

    /** 전체 구조 대상 수 (Lua에서 설정) */
    UPROPERTY(EditAnywhere, Category="Rescue", Tooltip="전체 구조 대상 수입니다. Lua에서 설정합니다.")
    int32 TotalPersonCount;

    /** 구조된 사람 수 */
    int32 RescuedCount;

    // ────────────────────────────────────────────────
    // 외부 접근용 함수
    // ────────────────────────────────────────────────

    /** 산소량 반환 */
    float GetCurrentOxygen() const { return CurrentOxygen; }
    float GetMaxOxygen() const { return MaxOxygen; }
    float GetOxygenPercent() const { return MaxOxygen > 0.0f ? CurrentOxygen / MaxOxygen : 0.0f; }

    /** 물 게이지 반환 */
    float GetCurrentWater() const { return CurrentWater; }
    float GetMaxWater() const { return BaseWater; }
    float GetWaterPercent() const { return BaseWater > 0.0f ? CurrentWater / BaseWater : 0.0f; }

    /** 소화기 개수 반환 */
    int32 GetFireExtinguisherCount() const { return FireExtinguisherCount; }

    /** 물 마법 사용 상태 설정 */
    void SetUsingWaterMagic(bool bUsing) { bIsUsingWaterMagic = bUsing; }

    /** 구조 시스템 접근 */
    int32 GetRescuedCount() const { return RescuedCount; }
    int32 GetTotalPersonCount() const { return TotalPersonCount; }
    void SetTotalPersonCount(int32 Count) { TotalPersonCount = Count; }

    /** 위치가 구조 구역 안에 있는지 확인 */
    bool IsInSafeZone(const FVector& Position) const;

    /** 사람 구조 처리 (구조 카운트 증가) */
    void OnPersonRescued();

    // ────────────────────────────────────────────────
    // 엔딩 전환
    // ────────────────────────────────────────────────

    /** 엔딩 씬 경로 */
    UPROPERTY(EditAnywhere, Category="Ending", Tooltip="엔딩 씬 경로입니다.")
    FWideString EndingScenePath = L"Data/Scenes/Ending.scene";

    /** 플레이어 사망 후 와이프 시작까지 대기 시간 (초) */
    UPROPERTY(EditAnywhere, Category="Ending", Tooltip="플레이어 사망 후 와이프 효과 시작까지 대기 시간입니다.")
    float DeathDelayBeforeWipe = 1.5f;

    /** 와이프 애니메이션 지속 시간 (초) */
    UPROPERTY(EditAnywhere, Category="Ending", Tooltip="스트립와이프 애니메이션 지속 시간입니다.")
    float WipeDuration = 1.5f;

    /** 엔딩으로 전환 (GameInstance에 결과 저장 후 씬 전환) */
    void TransitionToEnding(bool bPlayerDead);

protected:
    // ────────────────────────────────────────────────
    // 내부 함수
    // ────────────────────────────────────────────────

    /** 플레이어 초기화 (스케일 및 Actor 기반 위치 적용) */
    void InitPlayer() override;

    /** 스폰 위치로 사용할 Actor 찾기 */
    AActor* FindSpawnActor() const;

    /** 아이템 기반 상태 초기화 */
    void InitializePlayerState();

    /** HUD 초기화 */
    void InitializeHUD();

    /** HUD 업데이트 */
    void UpdateHUD();

    /** 산소 시스템 업데이트 */
    void UpdateOxygenSystem(float DeltaSeconds);

    /** 물 시스템 업데이트 */
    void UpdateWaterSystem(float DeltaSeconds);

    /** 구조 구역 Actor 찾기 */
    AActor* FindSafeZoneActor() const;

    /** 공지 애니메이션 업데이트 */
    void UpdateNoticeAnimation(float DeltaTime);

private:
    // ────────────────────────────────────────────────
    // UI 위젯
    // ────────────────────────────────────────────────

    /** 산소 아이콘 텍스트 */
    TSharedPtr<STextBlock> OxygenIconText;

    /** 산소 수치 텍스트 */
    TSharedPtr<STextBlock> OxygenValueText;

    /** 물 게이지 프로그레스 바 */
    TSharedPtr<SProgressBar> WaterProgressBar;

    /** 소화기 개수 텍스트 */
    TSharedPtr<STextBlock> FireExtinguisherText;

    /** 구조 카운트 텍스트 (우측 상단) */
    TSharedPtr<STextBlock> RescueCountText;

    /** 공지 이미지 위젯 (start.png) */
    TSharedPtr<STextBlock> NoticeWidget;

    /** 공지 애니메이션 상태 */
    float NoticeElapsedTime = 0.0f;
    static constexpr float NoticeDuration = 3.0f;  // 전체 애니메이션 시간

    /** 캐시된 구조 구역 Actor */
    mutable AActor* CachedSafeZoneActor = nullptr;

    // ────────────────────────────────────────────────
    // 엔딩 전환 상태
    // ────────────────────────────────────────────────

    /** 엔딩 전환 대기 중 여부 */
    bool bWaitingForEnding = false;

    /** 엔딩 전환 타이머 */
    float EndingTimer = 0.0f;

    /** 플레이어 사망 여부 (엔딩 결과용) */
    bool bEndingPlayerDead = false;

    /** 와이프 효과 시작 여부 */
    bool bWipeStarted = false;
};
