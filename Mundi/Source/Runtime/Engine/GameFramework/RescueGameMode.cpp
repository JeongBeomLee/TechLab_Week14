// ----------------------------------------------------------------------------
// RscueGameMode.cpp
// 구조 미션 게임 모드 구현
// ----------------------------------------------------------------------------
#include "pch.h"
#include "RescueGameMode.h"
#include "FirefighterCharacter.h"
#include "CharacterMovementComponent.h"
#include "InputManager.h"
#include "World.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "GameInstance.h"
#include "BoxComponent.h"
#include "FAudioDevice.h"
#include "Sound.h"
#include "ResourceManager.h"
#include "PlayerCameraManager.h"

#include "GameUI/SGameHUD.h"
#include "GameUI/STextBlock.h"
#include "GameUI/SProgressBar.h"
#include "GameUI/SButton.h"
#include <cmath>

// ----------------------------------------------------------------------------
// 생성자
// ----------------------------------------------------------------------------

ARescueGameMode::ARescueGameMode()
    : PlayerSpawnScale(1.5f)
    , SpawnActorName("PlayerStartPos")
    , SpawnActorTag("None")
    // 산소 시스템
    , BaseOxygen(100.0f)
    , OxygenPerTank(100.0f)
    , OxygenDecreaseRate(2.0f)
    , RunningOxygenMultiplier(2.0f)
    , FireSuitOxygenMultiplier(0.5f)
    , RunningSpeedThreshold(1.2f)
    , OxygenWarningThreshold(30.0f)
    , CurrentOxygen(100.0f)
    , MaxOxygen(100.0f)
    // 물 시스템
    , BaseWater(100.0f)
    , WaterDecreaseRate(20.0f)
    , CurrentWater(100.0f)
    , FireExtinguisherCount(0)
    , bIsUsingWaterMagic(false)
    // 더미 아이템
    , InitialOxygenTankCount(1)
    , InitialFireExtinguisherCount(3)
    , bInitialHasFireSuit(true)
    // 구조 시스템
    , SafeZoneActorName("SafeZone")
    , SafeZoneActorTag("None")
    , SafeZoneRadius(3.0f)
    , TotalPersonCount(0)
    , RescuedCount(0)
{
    DefaultPawnClass = AFirefighterCharacter::StaticClass();
    PlayerSpawnLocation = FVector(0.0f, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// 생명주기
// ----------------------------------------------------------------------------

void ARescueGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 사운드 초기화 (BGM + 사이렌)
    InitializeSounds();

    // 플레이어 상태 초기화 (아이템 기반)
    InitializePlayerState();

    // HUD 초기화
    InitializeHUD();
}

void ARescueGameMode::EndPlay()
{
    Super::EndPlay();

    // BGM 정지
    if (BGMVoice)
    {
        FAudioDevice::StopSound(BGMVoice);
        BGMVoice = nullptr;
    }

    // HUD 위젯 정리
    if (SGameHUD::Get().IsInitialized())
    {
        // 좌상단: 산소 시스템
        if (OxygenIconWidget)
        {
            SGameHUD::Get().RemoveWidget(OxygenIconWidget);
            OxygenIconWidget.Reset();
        }
        if (OxygenValueText)
        {
            SGameHUD::Get().RemoveWidget(OxygenValueText);
            OxygenValueText.Reset();
        }
        if (OxygenProgressBar)
        {
            SGameHUD::Get().RemoveWidget(OxygenProgressBar);
            OxygenProgressBar.Reset();
        }

        // 우상단: 소화기 + 물 게이지
        if (FireExtinguisherIconWidget)
        {
            SGameHUD::Get().RemoveWidget(FireExtinguisherIconWidget);
            FireExtinguisherIconWidget.Reset();
        }
        if (FireExtinguisherText)
        {
            SGameHUD::Get().RemoveWidget(FireExtinguisherText);
            FireExtinguisherText.Reset();
        }
        if (WaterProgressBar)
        {
            SGameHUD::Get().RemoveWidget(WaterProgressBar);
            WaterProgressBar.Reset();
        }

        // 가운데 상단: 구조 시스템
        if (RescuedIconWidget)
        {
            SGameHUD::Get().RemoveWidget(RescuedIconWidget);
            RescuedIconWidget.Reset();
        }
        if (RescuedCountText)
        {
            SGameHUD::Get().RemoveWidget(RescuedCountText);
            RescuedCountText.Reset();
        }
        if (RemainingIconWidget)
        {
            SGameHUD::Get().RemoveWidget(RemainingIconWidget);
            RemainingIconWidget.Reset();
        }
        if (RemainingCountText)
        {
            SGameHUD::Get().RemoveWidget(RemainingCountText);
            RemainingCountText.Reset();
        }

        // 공지 위젯
        if (NoticeWidget)
        {
            SGameHUD::Get().RemoveWidget(NoticeWidget);
            NoticeWidget.Reset();
        }
    }
}

void ARescueGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 공지 애니메이션 업데이트 (3초 동안만)
    if (NoticeElapsedTime < NoticeDuration)
    {
        UpdateNoticeAnimation(DeltaSeconds);
    }

    // 엔딩 전환 대기 중이면 타이머 처리
    if (bWaitingForEnding)
    {
        EndingTimer += DeltaSeconds;

        // 플레이어 사망 시: 1.5초 대기 후 와이프 시작
        if (bEndingPlayerDead && !bWipeStarted && EndingTimer >= DeathDelayBeforeWipe)
        {
            // 스트립와이프 효과 시작
            if (GetWorld())
            {
                if (APlayerCameraManager* CameraManager = GetWorld()->FindActor<APlayerCameraManager>())
                {
                    CameraManager->StartStripedWipe(WipeDuration, 8.f, FLinearColor(0, 0, 0, 1), 100);
                }
            }
            bWipeStarted = true;
        }

        // 와이프가 시작된 후, 와이프 완료까지 대기
        float TotalDelay = bEndingPlayerDead ? (DeathDelayBeforeWipe + WipeDuration) : WipeDuration;
        if (EndingTimer >= TotalDelay)
        {
            // GameInstance에 결과 저장
            UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
            if (GI)
            {
                GI->SetRescuedCount(RescuedCount);
                GI->SetPlayerHealth(bEndingPlayerDead ? 0 : 100);
                UE_LOG("[info] RescueGameMode - Saved to GameInstance: Rescued=%d, Health=%d",
                    RescuedCount, bEndingPlayerDead ? 0 : 100);
            }

            // 씬 전환 (GWorld 사용 - Lua와 동일)
            if (GWorld)
            {
                GWorld->TransitionToLevel(EndingScenePath);
            }
            bWaitingForEnding = false;
        }
        return;  // 엔딩 대기 중에는 다른 업데이트 중지
    }

    // 플레이어 사망 체크 (불 데미지 등)
    AFirefighterCharacter* Firefighter = nullptr;
    if (PlayerController)
    {
        APawn* ControlledPawn = PlayerController->GetPawn();
        Firefighter = Cast<AFirefighterCharacter>(ControlledPawn);
    }
    if (Firefighter && Firefighter->bIsDead)
    {
        TransitionToEnding(true);  // 플레이어 사망
        return;
    }

    // 산소 시스템 업데이트
    UpdateOxygenSystem(DeltaSeconds);

    // 물 시스템 업데이트
    UpdateWaterSystem(DeltaSeconds);

    // HUD 업데이트
    UpdateHUD();
}

// ----------------------------------------------------------------------------
// 플레이어 초기화
// ----------------------------------------------------------------------------

void ARescueGameMode::InitPlayer()
{
    // 1. PlayerController 생성
    PlayerController = SpawnPlayerController();

    if (!PlayerController)
    {
        UE_LOG("[error] RescueGameMode::InitPlayer - Failed to spawn PlayerController");
        return;
    }

    // 2. 스폰 위치 결정 (Actor 기준 또는 기본 위치)
    FVector SpawnLocation = PlayerSpawnLocation;
    FQuat SpawnRotation = FQuat::Identity();

    // SpawnActor를 찾아서 위치 가져오기
    AActor* SpawnActor = FindSpawnActor();
    if (SpawnActor)
    {
        SpawnLocation = SpawnActor->GetActorLocation();
        SpawnRotation = SpawnActor->GetActorRotation();
        UE_LOG("[info] RescueGameMode::InitPlayer - Using SpawnActor location: (%.2f, %.2f, %.2f)",
            SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
    }

    // 3. 스폰 Transform 설정
    FTransform SpawnTransform;
    SpawnTransform.Translation = SpawnLocation;
    SpawnTransform.Rotation = SpawnRotation;
    SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

    // 4. Pawn 스폰 및 빙의
    UE_LOG("[info] RescueGameMode::InitPlayer - DefaultPawnClass: %s",
        DefaultPawnClass ? DefaultPawnClass->Name : "nullptr");

    APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform);

    if (!SpawnedPawn)
    {
        UE_LOG("[error] RescueGameMode::InitPlayer - Failed to spawn DefaultPawn");
    }
    else
    {
        // 5. FirefighterCharacter의 스케일 설정
        if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(SpawnedPawn))
        {
            Firefighter->SetCharacterScale(PlayerSpawnScale);
        }

        UE_LOG("[info] RescueGameMode::InitPlayer - Spawned Pawn: %s with scale %.2f",
            SpawnedPawn->GetClass()->Name, PlayerSpawnScale);
    }
}

AActor* ARescueGameMode::FindSpawnActor() const
{
    if (!World)
    {
        return nullptr;
    }

    // 1. SpawnActorName으로 먼저 찾기
    if (SpawnActorName != "None")
    {
        AActor* FoundActor = World->FindActorByName(SpawnActorName);
        if (FoundActor)
        {
            UE_LOG("[info] RescueGameMode::FindSpawnActor - Found actor by name: %s",
                SpawnActorName.ToString().c_str());
            return FoundActor;
        }
        else
        {
            UE_LOG("[warning] RescueGameMode::FindSpawnActor - Actor not found by name: %s",
                SpawnActorName.ToString().c_str());
        }
    }

    // 2. SpawnActorTag로 찾기
    if (SpawnActorTag != "None")
    {
        FString TagToFind = SpawnActorTag.ToString();
        for (AActor* Actor : World->GetActors())
        {
            if (Actor && Actor->GetTag() == TagToFind)
            {
                UE_LOG("[info] RescueGameMode::FindSpawnActor - Found actor by tag: %s",
                    TagToFind.c_str());
                return Actor;
            }
        }
        UE_LOG("[warning] RescueGameMode::FindSpawnActor - Actor not found by tag: %s",
            TagToFind.c_str());
    }

    return nullptr;
}

// ----------------------------------------------------------------------------
// 아이템 기반 상태 초기화
// ----------------------------------------------------------------------------

void ARescueGameMode::InitializePlayerState()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

    // GameInstance에서 실제 아이템 개수 가져오기
    int32 OxygenTankCount = GI ? GI->GetItemCount("Oxygen") : 0;
    int32 FireExCount = GI ? GI->GetItemCount("FireEx") : 0;
    bool bHasFireSuit = GI ? GI->HasItem("FireSuit") : false;

    // 산소 시스템 초기화
    MaxOxygen = BaseOxygen + (OxygenPerTank * OxygenTankCount);
    CurrentOxygen = MaxOxygen;

    // 물 시스템 초기화 (HUD 표시용, 실제 로직은 Lua에서 처리)
    FireExtinguisherCount = FireExCount;
    CurrentWater = BaseWater;

    UE_LOG("[info] RescueGameMode::InitializePlayerState - OxygenTanks: %d, MaxOxygen: %.0f, FireExtinguishers: %d, FireSuit: %s",
        OxygenTankCount, MaxOxygen, FireExtinguisherCount, bHasFireSuit ? "Yes" : "No");
}

// ----------------------------------------------------------------------------
// HUD 초기화
// ----------------------------------------------------------------------------

void ARescueGameMode::InitializeHUD()
{
    if (!SGameHUD::Get().IsInitialized())
        return;

    const float IconSize = 100.f;
    const float FontSize = 48.f;
    const float NumberWidth = 100.f;  // 3자리 숫자 지원
    const float BarWidth = 220.f;     // 게이지 길이
    const FString FontPath = "Data/UI/fonts/ChosunKm.TTF";

    // ========================
    // 좌상단: 산소 아이콘 + 숫자 + 게이지 (대칭 레이아웃)
    // ========================
    OxygenIconWidget = MakeShared<SButton>();
    FSlateRect OxygenAtlas(1200.f, 0.f, 1800.f, 600.f);  // items.png 인덱스 2 (산소통)
    OxygenIconWidget->SetBackgroundImageAtlas(
        "Data/Textures/Collect/items.png", OxygenAtlas, OxygenAtlas, OxygenAtlas);

    SGameHUD::Get().AddWidget(OxygenIconWidget)
        .SetAnchor(0.f, 0.f)
        .SetPivot(0.f, 0.f)
        .SetOffset(30.f, 30.f)
        .SetSize(IconSize, IconSize)
        .SetZOrder(10);

    OxygenValueText = MakeShared<STextBlock>();
    OxygenValueText->SetText(std::to_wstring(static_cast<int32>(CurrentOxygen)))
        .SetFontSize(FontSize)
        .SetFontPath(FontPath)
        .SetColor(FSlateColor::White())
        .SetHAlign(ETextHAlign::Left)
        .SetVAlign(ETextVAlign::Center)
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(0.0f, 0.0f, 0.0f, 0.8f));

    SGameHUD::Get().AddWidget(OxygenValueText)
        .SetAnchor(0.f, 0.f)
        .SetPivot(0.f, 0.5f)
        .SetOffset(30.f + IconSize + 10.f, 30.f + IconSize * 0.5f)
        .SetSize(NumberWidth, 60.f)
        .SetZOrder(11);

    OxygenProgressBar = MakeShared<SProgressBar>();
    float OxygenBarPercent = FMath::Min(CurrentOxygen / 100.f, 1.f);  // 100 기준 클램프
    OxygenProgressBar->SetPercent(OxygenBarPercent)
        .SetFillColor(FSlateColor(0.3f, 0.8f, 0.3f, 1.0f))  // 녹색
        .SetBackgroundColor(FSlateColor(0.15f, 0.15f, 0.15f, 0.9f))
        .SetBorderColor(FSlateColor::White())
        .SetBorderThickness(2.0f)
        .SetCornerRadius(4.0f);

    SGameHUD::Get().AddWidget(OxygenProgressBar)
        .SetAnchor(0.f, 0.f)
        .SetPivot(0.f, 0.f)
        .SetOffset(30.f, 30.f + IconSize + 10.f)
        .SetSize(BarWidth, 20.f)
        .SetZOrder(10);

    // ========================
    // 우상단: 소화기 아이콘 + 숫자 + 물 게이지 (대칭 레이아웃)
    // ========================
    FireExtinguisherIconWidget = MakeShared<SButton>();
    FSlateRect FireExAtlas(600.f, 0.f, 1200.f, 600.f);  // items.png 인덱스 1 (소화기)
    FireExtinguisherIconWidget->SetBackgroundImageAtlas(
        "Data/Textures/Collect/items.png", FireExAtlas, FireExAtlas, FireExAtlas);

    SGameHUD::Get().AddWidget(FireExtinguisherIconWidget)
        .SetAnchor(1.f, 0.f)
        .SetPivot(1.f, 0.f)
        .SetOffset(-30.f, 30.f)
        .SetSize(IconSize, IconSize)
        .SetZOrder(10);

    FireExtinguisherText = MakeShared<STextBlock>();
    FireExtinguisherText->SetText(std::to_wstring(FireExtinguisherCount))
        .SetFontSize(FontSize)
        .SetFontPath(FontPath)
        .SetColor(FSlateColor::White())
        .SetHAlign(ETextHAlign::Right)
        .SetVAlign(ETextVAlign::Center)
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(0.0f, 0.0f, 0.0f, 0.8f));

    SGameHUD::Get().AddWidget(FireExtinguisherText)
        .SetAnchor(1.f, 0.f)
        .SetPivot(1.f, 0.5f)
        .SetOffset(-30.f - IconSize - 10.f, 30.f + IconSize * 0.5f)
        .SetSize(NumberWidth, 60.f)
        .SetZOrder(11);

    WaterProgressBar = MakeShared<SProgressBar>();
    WaterProgressBar->SetPercent(GetWaterPercent())
        .SetFillColor(FSlateColor(0.2f, 0.5f, 1.0f, 1.0f))  // 파란색
        .SetBackgroundColor(FSlateColor(0.15f, 0.15f, 0.15f, 0.9f))
        .SetBorderColor(FSlateColor::White())
        .SetBorderThickness(2.0f)
        .SetCornerRadius(4.0f);

    SGameHUD::Get().AddWidget(WaterProgressBar)
        .SetAnchor(1.f, 0.f)
        .SetPivot(1.f, 0.f)
        .SetOffset(-30.f, 30.f + IconSize + 10.f)
        .SetSize(BarWidth, 20.f)
        .SetZOrder(10);

    // ========================
    // 가운데 상단: 구조된 사람 + 남은 사람 (대칭 배치)
    // 레이아웃: [숫자] [구조 아이콘] | [남은 아이콘] [숫자]
    // ========================
    const float CenterGap = 40.f;  // 중앙 간격
    const float TextWidth = 60.f;

    // 구조된 사람 숫자 (왼쪽 그룹의 왼쪽)
    RescuedCountText = MakeShared<STextBlock>();
    RescuedCountText->SetText(std::to_wstring(RescuedCount))
        .SetFontSize(FontSize)
        .SetFontPath(FontPath)
        .SetColor(FSlateColor::White())
        .SetHAlign(ETextHAlign::Right)
        .SetVAlign(ETextVAlign::Center)
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(0.0f, 0.0f, 0.0f, 0.8f));

    SGameHUD::Get().AddWidget(RescuedCountText)
        .SetAnchor(0.5f, 0.f)
        .SetPivot(1.f, 0.5f)
        .SetOffset(-CenterGap * 0.5f - IconSize - 10.f, 20.f + IconSize * 0.5f)
        .SetSize(TextWidth, 60.f)
        .SetZOrder(11);

    // 구조된 사람 아이콘 (왼쪽 그룹의 오른쪽)
    RescuedIconWidget = MakeShared<SButton>();
    FSlateRect RescuedAtlas(0.f, 0.f, 600.f, 600.f);  // icons.png 왼쪽
    RescuedIconWidget->SetBackgroundImageAtlas(
        "Data/Textures/Rescue/icons.png", RescuedAtlas, RescuedAtlas, RescuedAtlas);

    SGameHUD::Get().AddWidget(RescuedIconWidget)
        .SetAnchor(0.5f, 0.f)
        .SetPivot(1.f, 0.f)
        .SetOffset(-CenterGap * 0.5f, 20.f)
        .SetSize(IconSize, IconSize)
        .SetZOrder(10);

    // 남은 사람 아이콘 (오른쪽 그룹의 왼쪽)
    RemainingIconWidget = MakeShared<SButton>();
    FSlateRect RemainingAtlas(600.f, 0.f, 1200.f, 600.f);  // icons.png 오른쪽
    RemainingIconWidget->SetBackgroundImageAtlas(
        "Data/Textures/Rescue/icons.png", RemainingAtlas, RemainingAtlas, RemainingAtlas);

    SGameHUD::Get().AddWidget(RemainingIconWidget)
        .SetAnchor(0.5f, 0.f)
        .SetPivot(0.f, 0.f)
        .SetOffset(CenterGap * 0.5f, 20.f)
        .SetSize(IconSize, IconSize)
        .SetZOrder(10);

    // 남은 사람 숫자 (오른쪽 그룹의 오른쪽)
    RemainingCountText = MakeShared<STextBlock>();
    int32 RemainingCount = TotalPersonCount - RescuedCount;
    RemainingCountText->SetText(std::to_wstring(RemainingCount))
        .SetFontSize(FontSize)
        .SetFontPath(FontPath)
        .SetColor(FSlateColor::White())
        .SetHAlign(ETextHAlign::Left)
        .SetVAlign(ETextVAlign::Center)
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor(0.0f, 0.0f, 0.0f, 0.8f));

    SGameHUD::Get().AddWidget(RemainingCountText)
        .SetAnchor(0.5f, 0.f)
        .SetPivot(0.f, 0.5f)
        .SetOffset(CenterGap * 0.5f + IconSize + 10.f, 20.f + IconSize * 0.5f)
        .SetSize(TextWidth, 60.f)
        .SetZOrder(11);

    // ========================
    // 공지 이미지 위젯 (화면 중앙, 3초간 크기 변화 애니메이션)
    // ========================
    NoticeWidget = MakeShared<STextBlock>();
    NoticeWidget->SetText(L"")
        .SetBackgroundImage("Data/Textures/Rescue/start.png");

    SGameHUD::Get().AddWidget(NoticeWidget)
        .SetAnchor(0.5f, 0.5f)
        .SetPivot(0.5f, 0.5f)
        .SetSize(1100.f, 400.f)
        .SetZOrder(100);
}

// ----------------------------------------------------------------------------
// HUD 업데이트
// ----------------------------------------------------------------------------

void ARescueGameMode::UpdateHUD()
{
    // 플레이어 사망 여부 확인
    bool bPlayerDead = false;
    if (PlayerController)
    {
        APawn* ControlledPawn = PlayerController->GetPawn();
        if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(ControlledPawn))
        {
            bPlayerDead = Firefighter->bIsDead;
        }
    }

    // 산소 수치 텍스트 업데이트
    if (OxygenValueText)
    {
        int32 DisplayOxygen = bPlayerDead ? 0 : static_cast<int32>(CurrentOxygen);
        OxygenValueText->SetText(std::to_wstring(DisplayOxygen));

        // 경고 상태 또는 사망 시 빨간색 표시
        if (bPlayerDead || CurrentOxygen < OxygenWarningThreshold)
        {
            OxygenValueText->SetColor(FSlateColor::Red());
        }
        else
        {
            OxygenValueText->SetColor(FSlateColor::White());
        }
    }

    // 산소 게이지 업데이트 (100 기준 클램프)
    if (OxygenProgressBar)
    {
        float ClampedOxygen = bPlayerDead ? 0.f : FMath::Min(CurrentOxygen, 100.f);
        OxygenProgressBar->SetPercent(ClampedOxygen / 100.f);

        // 경고 상태 또는 사망 시 빨간색 표시
        if (bPlayerDead || CurrentOxygen < OxygenWarningThreshold)
        {
            OxygenProgressBar->SetFillColor(FSlateColor(1.0f, 0.2f, 0.2f, 1.0f));  // 빨간색
        }
        else
        {
            OxygenProgressBar->SetFillColor(FSlateColor(0.3f, 0.8f, 0.3f, 1.0f));  // 녹색
        }
    }

    // 물 게이지 업데이트
    if (WaterProgressBar)
    {
        WaterProgressBar->SetPercent(GetWaterPercent());
    }

    // 소화기 개수 업데이트
    if (FireExtinguisherText)
    {
        FireExtinguisherText->SetText(std::to_wstring(FireExtinguisherCount));
    }

    // 구조된 사람 수 업데이트
    if (RescuedCountText)
    {
        RescuedCountText->SetText(std::to_wstring(RescuedCount));
    }

    // 남은 사람 수 업데이트
    if (RemainingCountText)
    {
        int32 RemainingCount = TotalPersonCount - RescuedCount;
        RemainingCountText->SetText(std::to_wstring(RemainingCount));
    }
}

// ----------------------------------------------------------------------------
// 산소 시스템 업데이트
// ----------------------------------------------------------------------------

void ARescueGameMode::UpdateOxygenSystem(float DeltaSeconds)
{
    // 플레이어가 죽었으면 산소 감소 중지
    AFirefighterCharacter* Firefighter = nullptr;
    if (PlayerController)
    {
        APawn* ControlledPawn = PlayerController->GetPawn();
        Firefighter = Cast<AFirefighterCharacter>(ControlledPawn);
    }

    if (Firefighter && Firefighter->bIsDead)
    {
        return;  // 사망 시 산소 감소 중지
    }

    // 달리기 상태 확인 (Shift 키 입력 + 이동 중)
    float OxygenMultiplier = 1.0f;
    if (Firefighter)
    {
        UCharacterMovementComponent* Movement = Firefighter->GetCharacterMovement();
        if (Movement)
        {
            // Shift 키가 눌려있고 이동 중인지 확인
            bool bShiftPressed = UInputManager::GetInstance().IsKeyDown(VK_SHIFT);
            FVector Velocity = Movement->GetVelocity();
            float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

            // Shift 키를 누르고 이동 중이면 달리기로 판정
            if (bShiftPressed && HorizontalSpeed > 0.1f)
            {
                OxygenMultiplier = RunningOxygenMultiplier;
            }
        }
    }

    // 소방복 착용 시 산소 감소율 감소
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (GI && GI->HasItem("FireSuit"))
    {
        OxygenMultiplier *= FireSuitOxygenMultiplier;
    }

    // 산소 감소 (달리기 시 배율 적용, 소방복 착용 시 절반)
    CurrentOxygen -= OxygenDecreaseRate * OxygenMultiplier * DeltaSeconds;

    // 산소가 0 이하가 되면 플레이어 사망
    if (CurrentOxygen <= 0.0f)
    {
        CurrentOxygen = 0.0f;

        // 플레이어 즉시 사망
        if (Firefighter && !Firefighter->bIsDead)
        {
            Firefighter->Kill();
            TransitionToEnding(true);  // 플레이어 사망
        }
    }
}

// ----------------------------------------------------------------------------
// 물 시스템 업데이트
// ----------------------------------------------------------------------------

void ARescueGameMode::UpdateWaterSystem(float DeltaSeconds)
{
    // HUD 표시를 위해 캐릭터의 ExtinguishGauge와 GameInstance 소화기 개수를 읽기만 함
    // 실제 로직(게이지 감소, 소화기 소진, 충전)은 Lua에서 처리
    if (PlayerController)
    {
        APawn* ControlledPawn = PlayerController->GetPawn();
        if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(ControlledPawn))
        {
            bIsUsingWaterMagic = Firefighter->IsUsingWaterMagic();
            CurrentWater = Firefighter->ExtinguishGauge;
        }
    }

    // GameInstance에서 소화기 개수 읽기 (HUD용)
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (GI)
    {
        FireExtinguisherCount = GI->GetItemCount("FireEx");
    }
}

// ----------------------------------------------------------------------------
// 구조 시스템
// ----------------------------------------------------------------------------

AActor* ARescueGameMode::FindSafeZoneActor() const
{
    // 캐시된 Actor가 있으면 반환
    if (CachedSafeZoneActor)
    {
        return CachedSafeZoneActor;
    }

    if (!World)
    {
        return nullptr;
    }

    // 1. SafeZoneActorName으로 먼저 찾기
    if (SafeZoneActorName != "None")
    {
        AActor* FoundActor = World->FindActorByName(SafeZoneActorName);
        if (FoundActor)
        {
            UE_LOG("[info] RescueGameMode::FindSafeZoneActor - Found actor by name: %s",
                SafeZoneActorName.ToString().c_str());
            CachedSafeZoneActor = FoundActor;
            return FoundActor;
        }
    }

    // 2. SafeZoneActorTag로 찾기
    if (SafeZoneActorTag != "None")
    {
        FString TagToFind = SafeZoneActorTag.ToString();
        for (AActor* Actor : World->GetActors())
        {
            if (Actor && Actor->GetTag() == TagToFind)
            {
                UE_LOG("[info] RescueGameMode::FindSafeZoneActor - Found actor by tag: %s",
                    TagToFind.c_str());
                CachedSafeZoneActor = Actor;
                return Actor;
            }
        }
    }

    return nullptr;
}

bool ARescueGameMode::IsInSafeZone(const FVector& Position) const
{
    AActor* SafeZoneActor = FindSafeZoneActor();
    if (!SafeZoneActor)
    {
        return false;
    }

    // BoxComponent가 있으면 ContainsPoint로 체크
    UBoxComponent* BoxComp = Cast<UBoxComponent>(
        SafeZoneActor->GetComponent(UBoxComponent::StaticClass()));
    if (BoxComp)
    {
        bool bContains = BoxComp->ContainsPoint(Position);
        UE_LOG("[info] RescueGameMode::IsInSafeZone - BoxComponent check: %s",
            bContains ? "true" : "false");
        return bContains;
    }

    // BoxComponent가 없으면 기존 radius 방식으로 fallback
    FVector SafeZonePos = SafeZoneActor->GetActorLocation();
    float Distance = (Position - SafeZonePos).Size();

    return Distance <= SafeZoneRadius;
}

void ARescueGameMode::OnPersonRescued()
{
    RescuedCount++;
    UE_LOG("[info] RescueGameMode::OnPersonRescued - Rescued: %d / %d", RescuedCount, TotalPersonCount);

    // 모든 사람 구조 시 엔딩으로 전환
    if (TotalPersonCount > 0 && RescuedCount >= TotalPersonCount)
    {
        UE_LOG("[info] RescueGameMode - All persons rescued! Transitioning to ending...");
        TransitionToEnding(false);  // 플레이어 생존
    }
}

void ARescueGameMode::TransitionToEnding(bool bPlayerDead)
{
    if (bWaitingForEnding)
    {
        return;  // 이미 전환 대기 중
    }

    // 마지막 HUD 업데이트
    UpdateHUD();

    UE_LOG("[info] RescueGameMode::TransitionToEnding - PlayerDead: %s, DeathDelay: %.1fs, WipeDuration: %.1fs",
        bPlayerDead ? "true" : "false", DeathDelayBeforeWipe, WipeDuration);

    // 사망 시에는 와이프를 즉시 시작하지 않고 타이머로 처리
    // 구조 완료 시에는 즉시 와이프 시작
    if (!bPlayerDead)
    {
        // 구조 완료: 즉시 와이프 시작
        if (GetWorld())
        {
            if (APlayerCameraManager* CameraManager = GetWorld()->FindActor<APlayerCameraManager>())
            {
                CameraManager->StartStripedWipe(WipeDuration, 8.f, FLinearColor(0, 0, 0, 1), 100);
            }
        }
        bWipeStarted = true;
    }

    bWaitingForEnding = true;
    bEndingPlayerDead = bPlayerDead;
    bWipeStarted = !bPlayerDead;  // 구조 완료 시에는 이미 와이프 시작됨
    EndingTimer = 0.0f;
}

// ----------------------------------------------------------------------------
// 사운드 초기화
// ----------------------------------------------------------------------------

void ARescueGameMode::InitializeSounds()
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
}

// ----------------------------------------------------------------------------
// 공지 애니메이션 업데이트
// ----------------------------------------------------------------------------

void ARescueGameMode::UpdateNoticeAnimation(float DeltaTime)
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
