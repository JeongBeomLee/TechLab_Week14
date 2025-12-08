// ────────────────────────────────────────────────────────────────────────────
// ItemCollectGameMode.cpp
// 아이템 수집 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "ItemCollectGameMode.h"
#include "FirefighterCharacter.h"
#include "World.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "GameUI/SGameHUD.h"
#include "GameUI/STextBlock.h"
#include "GameUI/SButton.h"
#include "LevelTransitionManager.h"
#include "GameInstance.h"
#include "InputManager.h"
#include "FAudioDevice.h"
#include "Sound.h"
#include "ResourceManager.h"
#include <cmath>

IMPLEMENT_CLASS(AItemCollectGameMode)

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AItemCollectGameMode::AItemCollectGameMode()
	: PlayerSpawnScale(1.5f)
	, SpawnActorName("PlayerStartPos")
	, SpawnActorTag("None")
	, TimeLimit(40.0f)
	, NextScenePath(L"Data/Scenes/FireDispatch.scene")
	, TimerWidgetSize(200.f)
	, TimerTextOffsetRatio(0.12f)
	, RemainingTime(0.0f)
	, LastSecond(-1.0f)
	, ShakeAnimationTime(0.0f)
	, NoticeElapsedTime(0.0f)
	, bShowingLimitScreen(false)
	, LimitScreenTimer(0.0f)
{
	DefaultPawnClass = AFirefighterCharacter::StaticClass();
	PlayerSpawnLocation = FVector(0.0f, 0.0f, 1.0f);

	// 아이템 카운트 초기화
	for (int i = 0; i < 3; ++i)
	{
		ItemCounts[i] = 0;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 입력 모드를 GameAndUI로 설정 (플레이어 조작 가능)
	UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);

	// 타이머 초기화
	RemainingTime = TimeLimit;
	LastSecond = -1.0f;
	ShakeAnimationTime = 0.0f;

	// LevelTransitionManager에 NextLevel 설정
	if (GetWorld())
	{
		for (AActor* Actor : GetWorld()->GetActors())
		{
			if (ALevelTransitionManager* Manager = Cast<ALevelTransitionManager>(Actor))
			{
				Manager->SetNextLevel(NextScenePath);
				break;
			}
		}
	}

	// 사운드 초기화 (BGM + 사이렌)
	InitializeSounds();

	// UI 초기화
	InitializeUI();
	InitializeItemUI();

	// 물 마법 사용 불가 설정 (ItemCollect 씬에서는 물 발사 비활성화)
	if (PlayerController)
	{
		if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(PlayerController->GetPawn()))
		{
			Firefighter->SetCanUseWaterMagic(false);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 플레이어 초기화
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::InitPlayer()
{
	// 1. PlayerController 생성
	PlayerController = SpawnPlayerController();
	if (!PlayerController) { return; }

	// 2. 스폰 위치 결정 (Actor 기준 또는 기본 위치)
	FVector SpawnLocation = PlayerSpawnLocation;
	FQuat SpawnRotation = FQuat::Identity();

	// SpawnActor를 찾아서 위치 가져오기
	if (AActor* SpawnActor = FindSpawnActor())
	{
		SpawnLocation = SpawnActor->GetActorLocation();
		SpawnRotation = SpawnActor->GetActorRotation();
	}

	// 3. 스폰 Transform 설정 (스케일은 SetCharacterScale에서 처리)
	FTransform SpawnTransform;
	SpawnTransform.Translation = SpawnLocation;
	SpawnTransform.Rotation = SpawnRotation;
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);  // 기본 스케일로 스폰

	// 4. Pawn 스폰 및 빙의
	if (APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform))
	{
		// 5. FirefighterCharacter의 스케일 설정 (캡슐, 메쉬, 스프링암 등 모두 조절)
		if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(SpawnedPawn))
		{
			Firefighter->SetCharacterScale(PlayerSpawnScale);
		}
	}
}

AActor* AItemCollectGameMode::FindSpawnActor() const
{
	if (!World) { return nullptr; }

	// 1. SpawnActorName으로 먼저 찾기
	if (SpawnActorName != "None")
	{
		if (AActor* FoundActor = World->FindActorByName(SpawnActorName)) { return FoundActor; }
	}

	// 2. SpawnActorTag로 찾기 (모든 Actor 순회)
	if (SpawnActorTag != "None")
	{
		FString TagToFind = SpawnActorTag.ToString();
		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->GetTag() == TagToFind)
			{
				return Actor;
			}
		}
	}

	return nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// Tick
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 공지 애니메이션 업데이트 (3초 동안만)
	if (NoticeElapsedTime < NoticeDuration)
	{
		UpdateNoticeAnimation(DeltaTime);
	}

	// Limit 화면 표시 중이면 타이머 업데이트 후 씬 전환
	if (bShowingLimitScreen)
	{
		LimitScreenTimer += DeltaTime;

		// 떨림 애니메이션 업데이트
		UpdateLimitAnimation(DeltaTime);

		if (LimitScreenTimer >= LimitScreenDuration)
		{
			// 3초 경과 후 씬 전환
			if (GetWorld())
			{
				for (AActor* Actor : GetWorld()->GetActors())
				{
					if (ALevelTransitionManager* Manager = Cast<ALevelTransitionManager>(Actor))
					{
						if (!Manager->IsTransitioning())
						{
							Manager->TransitionToNextLevel();
						}
						break;
					}
				}
			}
			bShowingLimitScreen = false;  // 전환 후 플래그 리셋
		}
		return;  // Limit 화면 표시 중에는 다른 업데이트 중지
	}

	// 타이머 업데이트
	if (RemainingTime > 0.0f)
	{
		RemainingTime -= DeltaTime;

		// 0 이하로 떨어지면 Limit 화면 표시
		if (RemainingTime <= 0.0f)
		{
			RemainingTime = 0.0f;

			// BGM 정지 (limit 화면에서는 알람만)
			if (BGMVoice)
			{
				FAudioDevice::StopSound(BGMVoice);
				BGMVoice = nullptr;
			}

			// 시계 알람 재생 (limit 화면용)
			if (ClockAlarmSound)
			{
				FAudioDevice::PlaySound3D(ClockAlarmSound, FVector::Zero(), 1.0f, false);
			}

			// Limit 위젯 생성 및 표시
			if (!LimitWidget && SGameHUD::Get().IsInitialized())
			{
				LimitWidget = MakeShared<STextBlock>();
				LimitWidget->SetText(L"")
					.SetBackgroundImage("Data/Textures/Collect/limit.png");

				SGameHUD::Get().AddWidget(LimitWidget)
					.SetAnchor(0.5f, 0.5f)      // 화면 중앙
					.SetPivot(0.5f, 0.5f)       // 중앙 기준
					.SetSize(1100.f, 400.f)     // notice와 동일한 크기
					.SetZOrder(200);            // 최상위에 표시
			}

			// Limit 화면 표시 시작
			bShowingLimitScreen = true;
			LimitScreenTimer = 0.0f;
		}

		// 애니메이션 시간 감소
		if (ShakeAnimationTime > 0.0f)
		{
			ShakeAnimationTime -= DeltaTime;
		}

		// UI 업데이트ww
		UpdateTimerUI();
	}

	// 아이템 카운트 UI 업데이트 (GameInstance에서 읽어오기)
	UpdateItemCountUI();
}

// ────────────────────────────────────────────────────────────────────────────
// EndPlay
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::EndPlay()
{
	Super::EndPlay();

	// BGM 정지
	if (BGMVoice)
	{
		FAudioDevice::StopSound(BGMVoice);
		BGMVoice = nullptr;
	}

	// 입력 모드 복원 (혹시 모를 상태 변경 대비)
	UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);

	ClearUI();
	ClearItemUI();
}

// ────────────────────────────────────────────────────────────────────────────
// UI 초기화
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::InitializeUI()
{
	if (!SGameHUD::Get().IsInitialized()) { return; }

	// 타이머 배경 이미지 위젯
	TimerBackgroundWidget = MakeShared<STextBlock>();
	TimerBackgroundWidget->SetText(L"")
		.SetBackgroundImage("Data/Textures/Collect/timer.png");

	SGameHUD::Get().AddWidget(TimerBackgroundWidget)
		.SetAnchor(0.90f, 0.10f)    // 우측 상단
		.SetPivot(0.5f, 0.5f)       // 중앙 기준
		.SetSize(TimerWidgetSize, TimerWidgetSize)
		.SetZOrder(10);

	// 타이머 텍스트 위젯 (배경과 같은 위치, Offset으로 조정)
	TimerTextWidget = MakeShared<STextBlock>();

	// 초기 텍스트 설정
	int CurrentSecond = static_cast<int>(std::ceil(RemainingTime));
	wchar_t TimeText[32];
	swprintf_s(TimeText, L"%d", CurrentSecond);

	// 텍스트 Y 오프셋 계산 (위젯 크기 * 비율)
	float TextOffsetY = TimerWidgetSize * TimerTextOffsetRatio;

	TimerTextWidget->SetText(TimeText)
		.SetFontSize(52.f)
		.SetFontPath("Data/UI/fonts/ChosunKm.TTF")
		.SetColor(FSlateColor(0.1f, 0.1f, 0.1f, 1.0f))  // 거의 검은색
		.SetHAlign(ETextHAlign::Center)    // 중앙 정렬
		.SetVAlign(ETextVAlign::Center)    // 중앙 정렬
		.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(1.0f, 1.0f, 1.0f, 0.8f));  // 흰색 그림자

	SGameHUD::Get().AddWidget(TimerTextWidget)
		.SetAnchor(0.90f, 0.10f)    // 배경과 동일한 Anchor
		.SetPivot(0.5f, 0.5f)       // 중앙 기준
		.SetOffset(0.f, TextOffsetY)  // 비율 기반 오프셋
		.SetSize(TimerWidgetSize, TimerWidgetSize)
		.SetZOrder(11);             // 텍스트가 위에

	// 공지 이미지 위젯 (화면 중앙, 3초간 크기 변화 애니메이션)
	NoticeWidget = MakeShared<STextBlock>();
	NoticeWidget->SetText(L"")
		.SetBackgroundImage("Data/Textures/Collect/notice.png");

	SGameHUD::Get().AddWidget(NoticeWidget)
		.SetAnchor(0.5f, 0.5f)      // 화면 중앙
		.SetPivot(0.5f, 0.5f)       // 중앙 기준
		.SetSize(1100.f, 400.f)      // 초기 크기
		.SetZOrder(100);            // 가장 위에 표시
}

// ────────────────────────────────────────────────────────────────────────────
// UI 정리
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::ClearUI()
{
	if (!SGameHUD::Get().IsInitialized()) { return; }

	if (TimerBackgroundWidget)
	{
		SGameHUD::Get().RemoveWidget(TimerBackgroundWidget);
		TimerBackgroundWidget.Reset();
	}

	if (TimerTextWidget)
	{
		SGameHUD::Get().RemoveWidget(TimerTextWidget);
		TimerTextWidget.Reset();
	}

	if (NoticeWidget)
	{
		SGameHUD::Get().RemoveWidget(NoticeWidget);
		NoticeWidget.Reset();
	}

	if (LimitWidget)
	{
		SGameHUD::Get().RemoveWidget(LimitWidget);
		LimitWidget.Reset();
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 타이머 UI 업데이트
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::UpdateTimerUI()
{
	if (!TimerBackgroundWidget || !TimerTextWidget || !SGameHUD::Get().IsInitialized()) { return; }

	int CurrentSecond = static_cast<int>(std::ceil(RemainingTime));

	// 1초가 감소할 때마다 흔들림 애니메이션 시작
	if (CurrentSecond != static_cast<int>(LastSecond))
	{
		LastSecond = static_cast<float>(CurrentSecond);
		ShakeAnimationTime = 0.3f; // 0.3초 동안 애니메이션
	}

	// 텍스트 업데이트
	wchar_t TimeText[32];
	swprintf_s(TimeText, L"%d", CurrentSecond);
	TimerTextWidget->SetText(TimeText);

	// 시간에 따라 점진적으로 빨간색으로 변경 (Lerp)
	float TimeRatio = RemainingTime / TimeLimit;  // 1.0 (시작) ~ 0.0 (종료)
	float Red = 0.1f + (0.9f - 0.1f) * (1.0f - TimeRatio);  // 0.1 -> 0.9
	TimerTextWidget->SetColor(FSlateColor(Red, 0.1f, 0.1f, 1.0f));

	// 슬롯을 직접 찾아서 애니메이션 적용
	auto& Slots = SGameHUD::Get().GetRootCanvas()->GetCanvasSlots();

	// 애니메이션 값 계산
	float Scale = 1.0f;
	float OffsetX = 0.0f;

	if (ShakeAnimationTime > 0.0f)
	{
		float AnimProgress = 1.0f - (ShakeAnimationTime / 0.3f); // 0~1
		Scale = 1.0f + 0.2f * (1.0f - AnimProgress);
		OffsetX = std::sin(AnimProgress * 3.14159f * 4.0f) * 10.0f * (1.0f - AnimProgress);
	}

	// 텍스트 Y 오프셋 계산
	float TextOffsetY = TimerWidgetSize * TimerTextOffsetRatio;

	// 배경과 텍스트 둘 다 애니메이션 적용
	for (auto& Slot : Slots)
	{
		if (Slot.Widget == TimerBackgroundWidget)
		{
			Slot.SetOffset(OffsetX, 0.f)  // 배경은 흔들림만
				.SetSize(TimerWidgetSize * Scale, TimerWidgetSize * Scale);
		}
		else if (Slot.Widget == TimerTextWidget)
		{
			Slot.SetOffset(OffsetX, TextOffsetY)  // 텍스트는 비율 기반 오프셋 + 흔들림
				.SetSize(TimerWidgetSize * Scale, TimerWidgetSize * Scale);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 아이템 UI 초기화
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::InitializeItemUI()
{
	if (!SGameHUD::Get().IsInitialized()) { return; }

	// 아이템 아이콘 크기 및 간격 설정
	const float ItemImageSize = 150.f;   // 아이템 이미지 크기
	const float ItemSpacing = 220.f;     // 아이템 간 간격
	const float StartX = 0.5f;           // 화면 중앙 (X Anchor)
	const float StartY = 0.08f;          // 상단 (Y Anchor)
	const float StartOffsetX = -ItemSpacing;  // 중앙 기준 왼쪽으로 이동하여 3개 배치

	for (int i = 0; i < 3; ++i)
	{
		// 아이템 이미지 위젯 (items.png 아틀라스, 600x600씩 슬라이싱)
		ItemImageWidgets[i] = MakeShared<SButton>();

		// 아틀라스 영역 설정 (Left, Top, Right, Bottom)
		FSlateRect AtlasRect(
			i * 600.f,           // Left
			0.f,                 // Top
			(i + 1) * 600.f,     // Right
			600.f                // Bottom
		);

		// 소방복(인덱스 0)은 처음에 흑백 이미지(itemsNone.png)로 시작
		FString ImagePath = (i == 0) ? "Data/Textures/Collect/itemsNone.png" : "Data/Textures/Collect/items.png";

		ItemImageWidgets[i]->SetBackgroundImageAtlas(
			ImagePath,
			AtlasRect,  // Normal
			AtlasRect,  // Hovered
			AtlasRect   // Pressed
		);

		// 화면 중앙 상단에 배치 (3개를 일렬로)
		SGameHUD::Get().AddWidget(ItemImageWidgets[i])
			.SetAnchor(StartX, StartY)
			.SetPivot(0.5f, 0.5f)
			.SetOffset(StartOffsetX + i * ItemSpacing, 0.f)
			.SetSize(ItemImageSize, ItemImageSize)
			.SetZOrder(10);

		// 아이템 카운트 텍스트 위젯 (이미지 오른쪽 옆)
		// 소방복(인덱스 0)은 개수를 표시하지 않음
		if (i != 0)
		{
			ItemCountWidgets[i] = MakeShared<STextBlock>();

			wchar_t CountText[32];
			swprintf_s(CountText, L"%d", ItemCounts[i]);

			ItemCountWidgets[i]->SetText(CountText)
				.SetFontSize(48.f)
				.SetFontPath("Data/UI/fonts/ChosunKm.TTF")
				.SetColor(FSlateColor(1.0f, 1.0f, 1.0f, 1.0f))  // 흰색
				.SetHAlign(ETextHAlign::Left)
				.SetVAlign(ETextVAlign::Center)
				.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(0.0f, 0.0f, 0.0f, 0.8f));  // 검은색 그림자

			SGameHUD::Get().AddWidget(ItemCountWidgets[i])
				.SetAnchor(StartX, StartY)
				.SetPivot(0.0f, 0.5f)  // 왼쪽 중앙 기준
				.SetOffset(StartOffsetX + i * ItemSpacing + ItemImageSize * 0.6f, 0.f)  // 이미지 오른쪽
				.SetSize(100.f, 60.f)
				.SetZOrder(11);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 아이템 UI 정리
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::ClearItemUI()
{
	if (!SGameHUD::Get().IsInitialized()) { return; }

	for (int i = 0; i < 3; ++i)
	{
		if (ItemImageWidgets[i])
		{
			SGameHUD::Get().RemoveWidget(ItemImageWidgets[i]);
			ItemImageWidgets[i].Reset();
		}

		if (ItemCountWidgets[i])
		{
			SGameHUD::Get().RemoveWidget(ItemCountWidgets[i]);
			ItemCountWidgets[i].Reset();
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 아이템 수집 업데이트
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::UpdateItemCount(const FString& ItemTag)
{
	// 태그에 따라 인덱스 결정
	int ItemIndex = -1;

	if (ItemTag.find("firesuit") != FString::npos || ItemTag.find("소방복") != FString::npos)
	{
		ItemIndex = 0;
	}
	else if (ItemTag.find("extinguisher") != FString::npos || ItemTag.find("소화기") != FString::npos)
	{
		ItemIndex = 1;
	}
	else if (ItemTag.find("oxygen") != FString::npos || ItemTag.find("산소통") != FString::npos)
	{
		ItemIndex = 2;
	}

	// 유효한 인덱스라면 카운트 증가 및 UI 업데이트
	if (ItemIndex >= 0 && ItemIndex < 3)
	{
		ItemCounts[ItemIndex]++;

		// 소방복(인덱스 0)은 이미지만 컬러로 변경 (개수 표시 안 함)
		if (ItemIndex == 0)
		{
			if (ItemImageWidgets[0] && SGameHUD::Get().IsInitialized())
			{
				FSlateRect AtlasRect(0.f, 0.f, 600.f, 600.f);
				ItemImageWidgets[0]->SetBackgroundImageAtlas(
					"Data/Textures/Collect/items.png",
					AtlasRect,
					AtlasRect,
					AtlasRect
				);
			}
		}
		else
		{
			// 소화기, 산소통은 개수 텍스트 업데이트
			if (ItemCountWidgets[ItemIndex] && SGameHUD::Get().IsInitialized())
			{
				wchar_t CountText[32];
				swprintf_s(CountText, L"%d", ItemCounts[ItemIndex]);
				ItemCountWidgets[ItemIndex]->SetText(CountText);
			}
		}
	}
}

void AItemCollectGameMode::OnItemCollected(const FString& ItemTag)
{
	UpdateItemCount(ItemTag);
}

// ────────────────────────────────────────────────────────────────────────────
// 아이템 카운트 UI 업데이트 (GameInstance에서 읽어오기)
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::UpdateItemCountUI()
{
	if (!SGameHUD::Get().IsInitialized()) { return; }

	// GameInstance 가져오기
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance) { return; }

	// 각 아이템의 개수를 GameInstance에서 조회
	const FString ItemNames[] = { "FireSuit", "FireEx", "Oxygen" };

	for (int i = 0; i < 3; ++i)
	{
		int32 ItemCount = GameInstance->GetItemCount(ItemNames[i]);

		// 카운트가 변경되었을 때만 UI 업데이트
		if (ItemCounts[i] != ItemCount)
		{
			ItemCounts[i] = ItemCount;

			// 소방복(인덱스 0)은 이미지만 컬러로 변경
			if (i == 0)
			{
				if (ItemImageWidgets[0] && ItemCounts[0] > 0)
				{
					FSlateRect AtlasRect(0.f, 0.f, 600.f, 600.f);
					ItemImageWidgets[0]->SetBackgroundImageAtlas(
						"Data/Textures/Collect/items.png",
						AtlasRect,
						AtlasRect,
						AtlasRect
					);
				}
			}
			else
			{
				// 소화기, 산소통은 개수 텍스트 업데이트
				if (ItemCountWidgets[i])
				{
					wchar_t CountText[32];
					swprintf_s(CountText, L"%d", ItemCounts[i]);
					ItemCountWidgets[i]->SetText(CountText);
				}
			}
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 공지 애니메이션 업데이트
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::UpdateNoticeAnimation(float DeltaTime)
{
	if (!NoticeWidget || !SGameHUD::Get().IsInitialized()) { return; }

	NoticeElapsedTime += DeltaTime;

	// 3초가 지나면 완전히 숨김
	if (NoticeElapsedTime >= NoticeDuration)
	{
		NoticeWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// 애니메이션 계산
	// 0초~3초: 펄스 애니메이션 (크기 0.9 ~ 1.1 반복)
	float PulseProgress = (NoticeElapsedTime / 3.0f) * 3.14159f * 4.0f;  // 4회 반복
	float Scale = 1.0f + std::sin(PulseProgress) * 0.1f;  // 0.9 ~ 1.1

	// 위젯 슬롯 찾아서 크기 적용
	auto& Slots = SGameHUD::Get().GetRootCanvas()->GetCanvasSlots();
	for (auto& Slot : Slots)
	{
		if (Slot.Widget == NoticeWidget)
		{
			// 크기 적용
			Slot.SetSize(1100.f * Scale, 400.f * Scale);
			break;
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 사운드 초기화
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::InitializeSounds()
{
	// BGM 로드 및 재생 (루프)
	BGMSound = UResourceManager::GetInstance().Load<USound>(BGMSoundPath);
	if (BGMSound)
	{
		BGMVoice = FAudioDevice::PlaySound3D(BGMSound, FVector::Zero(), 0.5f, true);
	}

	// 사이렌 로드 및 재생 (1회)
	SirenSound = UResourceManager::GetInstance().Load<USound>(SirenSoundPath);
	if (SirenSound)
	{
		FAudioDevice::PlaySound3D(SirenSound, FVector::Zero(), 0.7f, false);
	}

	// 시간 종료 알람 사운드 로드 (재생은 limit 화면에서)
	ClockAlarmSound = UResourceManager::GetInstance().Load<USound>(ClockAlarmSoundPath);
}

// ────────────────────────────────────────────────────────────────────────────
// Limit 애니메이션 업데이트 (알람시계처럼 떨림)
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::UpdateLimitAnimation(float DeltaTime)
{
	if (!LimitWidget || !SGameHUD::Get().IsInitialized()) { return; }

	// 알람시계처럼 좌우로 빠르게 흔들림
	// 주파수를 높여서 빠른 떨림 효과
	float ShakeFrequency = 7.0f;  // 초당 15번 흔들림

	// 좌우 및 상하로 빠르게 흔들림
	float OffsetX = std::sin(LimitScreenTimer * 3.14159f * 2.0f * ShakeFrequency) * 15.0f;  // ±15px
	float OffsetY = std::cos(LimitScreenTimer * 3.14159f * 2.0f * ShakeFrequency * 1.3f) * 8.0f;  // ±8px (약간 다른 주기)

	// 위젯 슬롯 찾아서 오프셋 적용
	auto& Slots = SGameHUD::Get().GetRootCanvas()->GetCanvasSlots();
	for (auto& Slot : Slots)
	{
		if (Slot.Widget == LimitWidget)
		{
			// 오프셋 적용 (떨림)
			Slot.SetOffset(OffsetX, OffsetY);
			break;
		}
	}
}
