// ────────────────────────────────────────────────────────────────────────────
// ItemCollectGameMode.h
// 아이템 수집 게임 모드
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "GameModeBase.h"
#include "AItemCollectGameMode.generated.h"

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
};
