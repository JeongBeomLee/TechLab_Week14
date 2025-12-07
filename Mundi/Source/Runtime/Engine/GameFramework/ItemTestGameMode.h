// ────────────────────────────────────────────────────────────────────────────
// ItemTestGameMode.h
// 아이템 수집 HUD 테스트용 게임 모드
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AItemTestGameMode.generated.h"

class STextBlock;

/**
 * AItemTestGameMode
 *
 * 우측 상단에 수집한 아이템 목록을 표시하는 테스트용 게임 모드입니다.
 */
UCLASS(DisplayName="아이템 테스트 게임 모드", Description="아이템 수집 HUD 테스트용 게임 모드입니다.")
class AItemTestGameMode : public AGameModeBase
{
public:
	GENERATED_REFLECTION_BODY()

	AItemTestGameMode();

protected:
	~AItemTestGameMode() override = default;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float DeltaSeconds) override;

private:
	// ────────────────────────────────────────────────
	// HUD 업데이트
	// ────────────────────────────────────────────────

	/** 아이템 HUD 텍스트 갱신 */
	void UpdateItemHUD();

	// ────────────────────────────────────────────────
	// UI 위젯
	// ────────────────────────────────────────────────

	TSharedPtr<STextBlock> ItemHUDText;
};
