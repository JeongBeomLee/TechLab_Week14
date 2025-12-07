// ────────────────────────────────────────────────────────────────────────────
// ItemTestGameMode.cpp
// 아이템 수집 HUD 테스트용 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "ItemTestGameMode.h"
#include "GameInstance.h"
#include "World.h"
#include "FirefighterCharacter.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"

#include "GameUI/SGameHUD.h"
#include "GameUI/STextBlock.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AItemTestGameMode::AItemTestGameMode()
{
	DefaultPawnClass = AFirefighterCharacter::StaticClass();
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AItemTestGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!SGameHUD::Get().IsInitialized())
		return;

	// 이미 위젯이 생성되어 있으면 중복 생성 방지
	if (ItemHUDText)
		return;

	// 아이템 HUD 텍스트 (우상단)
	ItemHUDText = MakeShared<STextBlock>();
	ItemHUDText->SetText(L"[수집 아이템]")
		.SetFontSize(24.f)
		.SetColor(FSlateColor::Yellow())
		.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

	SGameHUD::Get().AddWidget(ItemHUDText)
		.SetAnchor(1.f, 0.f)      // 우상단 앵커
		.SetPivot(1.f, 0.f)       // 우상단 피벗
		.SetOffset(-20.f, 20.f)   // 화면 가장자리에서 20픽셀 안쪽
		.SetSize(300.f, 400.f);
}

void AItemTestGameMode::EndPlay()
{
	Super::EndPlay();

	// HUD 위젯 정리
	if (SGameHUD::Get().IsInitialized())
	{
		if (ItemHUDText)
		{
			SGameHUD::Get().RemoveWidget(ItemHUDText);
			ItemHUDText.Reset();
		}
	}
}

void AItemTestGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 매 프레임 아이템 HUD 갱신
	UpdateItemHUD();
}

// ────────────────────────────────────────────────────────────────────────────
// HUD 업데이트
// ────────────────────────────────────────────────────────────────────────────

void AItemTestGameMode::UpdateItemHUD()
{
	if (!ItemHUDText)
		return;

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		ItemHUDText->SetText(L"[수집 아이템]\n(GameInstance 없음)");
		return;
	}

	const auto& Items = GI->GetAllItems();

	std::wstring Text = L"[수집 아이템]\n";

	if (Items.empty())
	{
		Text += L"(없음)";
	}
	else
	{
		for (const auto& Pair : Items)
		{
			// FString (UTF-8) -> wstring 변환 (한글 지원)
			FWideString ItemName = UTF8ToWide(Pair.first);
			Text += ItemName + L": " + std::to_wstring(Pair.second) + L"\n";
		}
	}

	ItemHUDText->SetText(Text);
}
