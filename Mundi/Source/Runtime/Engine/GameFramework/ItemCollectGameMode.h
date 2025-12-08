// ────────────────────────────────────────────────────────────────────────────
// ItemCollectGameMode.h
// 아이템 수집 게임 모드
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AItemCollectGameMode.generated.h"

class STextBlock;
class SButton;

/**
 * AItemCollectGameMode
 *
 * 아이템 수집을 위한 게임 모드입니다.
 * - 특정 Actor를 기준으로 플레이어 스폰 위치 지정 가능
 * - 플레이어 스케일 조정 가능 (기본값 1.5)
 */
UCLASS(DisplayName="아이템 수집 게임 모드", Description="아이템 수집을 위한 게임 모드입니다.")
class AItemCollectGameMode : public AGameModeBase
{
public:
	GENERATED_REFLECTION_BODY()

	AItemCollectGameMode();

protected:
	~AItemCollectGameMode() override = default;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;

	// ────────────────────────────────────────────────
	// 스폰 설정
	// ────────────────────────────────────────────────

	/** 플레이어 스케일 (기본값 1.5) */
	UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="플레이어의 스케일입니다.")
	float PlayerSpawnScale;

	/** 스폰 위치로 사용할 Actor의 이름 (비어있으면 PlayerSpawnLocation 사용) */
	UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="스폰 위치로 사용할 Actor의 이름입니다. 비어있으면 PlayerSpawnLocation을 사용합니다.")
	FName SpawnActorName;

	/** 스폰 위치로 사용할 Actor의 태그 (SpawnActorName보다 우선순위 낮음) */
	UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="스폰 위치로 사용할 Actor의 태그입니다.")
	FName SpawnActorTag;

	// ────────────────────────────────────────────────
	// 타이머 설정
	// ────────────────────────────────────────────────

	/** 제한 시간 (초) */
	UPROPERTY(EditAnywhere, Category="Timer", Tooltip="제한 시간(초)입니다.")
	float TimeLimit;

	/** 다음 씬 경로 */
	UPROPERTY(EditAnywhere, Category="Timer", Tooltip="시간이 다 되면 전환될 씬 경로입니다.")
	FWideString NextScenePath;

	/** 타이머 위젯 크기 */
	UPROPERTY(EditAnywhere, Category="Timer", Tooltip="타이머 위젯 크기입니다.")
	float TimerWidgetSize;

	/** 타이머 텍스트 Y 오프셋 비율 (0.0~1.0) */
	UPROPERTY(EditAnywhere, Category="Timer", Tooltip="타이머 텍스트의 Y 오프셋 비율입니다. 0.1 = 위젯 크기의 10%")
	float TimerTextOffsetRatio;

	// ────────────────────────────────────────────────
	// Getter/Setter
	// ────────────────────────────────────────────────

	float GetPlayerSpawnScale() const { return PlayerSpawnScale; }
	void SetPlayerSpawnScale(float InScale) { PlayerSpawnScale = InScale; }

	const FName& GetSpawnActorName() const { return SpawnActorName; }
	void SetSpawnActorName(const FName& InName) { SpawnActorName = InName; }

protected:
	// ────────────────────────────────────────────────
	// 내부 함수
	// ────────────────────────────────────────────────

	/** 플레이어 초기화 (스케일 및 Actor 기반 위치 적용) */
	void InitPlayer() override;

	/** 스폰 위치로 사용할 Actor 찾기 */
	AActor* FindSpawnActor() const;

	/** UI 초기화 */
	void InitializeUI();

	/** UI 정리 */
	void ClearUI();

	/** 타이머 UI 업데이트 */
	void UpdateTimerUI();

	/** 아이템 UI 초기화 */
	void InitializeItemUI();

	/** 아이템 카운트 UI 업데이트 (GameInstance에서 읽어오기) */
	void UpdateItemCountUI();

	/** 아이템 UI 정리 */
	void ClearItemUI();

	/** 아이템 수집 콜백 */
	void OnItemCollected(const FString& ItemTag);

	/** 공지 애니메이션 업데이트 */
	void UpdateNoticeAnimation(float DeltaTime);

	/** Limit 화면 애니메이션 업데이트 (떨림 효과) */
	void UpdateLimitAnimation(float DeltaTime);

public:
	/** 아이템 수집 카운트 업데이트 (외부에서 호출 가능) */
	void UpdateItemCount(const FString& ItemTag);

private:
	// 타이머 UI
	TSharedPtr<STextBlock> TimerBackgroundWidget;  // 배경 이미지
	TSharedPtr<STextBlock> TimerTextWidget;        // 텍스트만

	// 타이머 상태
	float RemainingTime;
	float LastSecond;
	float ShakeAnimationTime;

	// 공지 UI
	TSharedPtr<STextBlock> NoticeWidget;  // 공지 이미지

	// 공지 애니메이션 상태
	float NoticeElapsedTime;  // 경과 시간 (0 ~ 3초)
	static constexpr float NoticeDuration = 3.0f;  // 전체 애니메이션 시간

	// 아이템 UI (3개 아이템: 소방복, 소화기, 산소통)
	TSharedPtr<SButton> ItemImageWidgets[3];    // 아이템 이미지 (아틀라스)
	TSharedPtr<STextBlock> ItemCountWidgets[3]; // 아이템 카운트 텍스트

	// 아이템 카운트
	int ItemCounts[3]; // 각 아이템 수집 개수

	// 제한시간 종료 UI
	TSharedPtr<STextBlock> LimitWidget;  // limit.png 이미지
	bool bShowingLimitScreen;            // limit 화면 표시 중 여부
	float LimitScreenTimer;              // limit 화면 표시 타이머
	static constexpr float LimitScreenDuration = 3.0f;  // limit 화면 표시 시간
};
