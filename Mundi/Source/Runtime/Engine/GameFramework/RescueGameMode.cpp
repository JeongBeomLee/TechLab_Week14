// ----------------------------------------------------------------------------
// RescueGameMode.cpp
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
#include "PlayerCameraManager.h"

#include "GameUI/SGameHUD.h"
#include "GameUI/STextBlock.h"
#include "GameUI/SProgressBar.h"
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

    // 플레이어 상태 초기화 (아이템 기반)
    InitializePlayerState();

    // HUD 초기화
    InitializeHUD();
}

void ARescueGameMode::EndPlay()
{
    Super::EndPlay();

    // HUD 위젯 정리
    if (SGameHUD::Get().IsInitialized())
    {
        if (OxygenIconText)
        {
            SGameHUD::Get().RemoveWidget(OxygenIconText);
            OxygenIconText.Reset();
        }
        if (OxygenValueText)
        {
            SGameHUD::Get().RemoveWidget(OxygenValueText);
            OxygenValueText.Reset();
        }
        if (WaterProgressBar)
        {
            SGameHUD::Get().RemoveWidget(WaterProgressBar);
            WaterProgressBar.Reset();
        }
        if (FireExtinguisherText)
        {
            SGameHUD::Get().RemoveWidget(FireExtinguisherText);
            FireExtinguisherText.Reset();
        }
        if (RescueCountText)
        {
            SGameHUD::Get().RemoveWidget(RescueCountText);
            RescueCountText.Reset();
        }
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
    int32 FireExCount = GI ? GI->GetItemCount("FireEX") : 0;
    bool bHasFireSuit = GI ? GI->HasItem("FireSuit") : false;

    // 산소 시스템 초기화
    MaxOxygen = BaseOxygen + (OxygenPerTank * OxygenTankCount);
    CurrentOxygen = MaxOxygen;

    // 물 시스템 초기화
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

    // ========================
    // 산소 아이콘 (좌하단)
    // ========================
    OxygenIconText = MakeShared<STextBlock>();
    OxygenIconText->SetText(L"O2")
        .SetFontSize(28.f)
        .SetColor(FSlateColor::White())
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

    SGameHUD::Get().AddWidget(OxygenIconText)
        .SetAnchor(0.f, 1.f)      // 좌하단 앵커
        .SetPivot(0.f, 1.f)       // 좌하단 피벗
        .SetOffset(30.f, -100.f)  // 화면 가장자리에서 오프셋
        .SetSize(60.f, 40.f);

    // ========================
    // 산소 수치 (아이콘 오른쪽)
    // ========================
    OxygenValueText = MakeShared<STextBlock>();
    OxygenValueText->SetText(std::to_wstring(static_cast<int32>(CurrentOxygen)))
        .SetFontSize(32.f)
        .SetColor(FSlateColor::White())
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

    SGameHUD::Get().AddWidget(OxygenValueText)
        .SetAnchor(0.f, 1.f)
        .SetPivot(0.f, 1.f)
        .SetOffset(90.f, -100.f)
        .SetSize(100.f, 40.f);

    // ========================
    // 물 게이지 프로그레스 바 (우하단)
    // ========================
    WaterProgressBar = MakeShared<SProgressBar>();
    WaterProgressBar->SetPercent(1.0f)
        .SetFillColor(FSlateColor(0.2f, 0.5f, 1.0f, 1.0f))  // 파란색
        .SetBackgroundColor(FSlateColor(0.15f, 0.15f, 0.15f, 0.9f))
        .SetBorderColor(FSlateColor::White())
        .SetBorderThickness(2.0f)
        .SetCornerRadius(4.0f);

    SGameHUD::Get().AddWidget(WaterProgressBar)
        .SetAnchor(1.f, 1.f)      // 우하단 앵커
        .SetPivot(1.f, 1.f)       // 우하단 피벗
        .SetOffset(-30.f, -100.f)
        .SetSize(200.f, 20.f);

    // ========================
    // 소화기 개수 텍스트 (프로그레스 바 왼쪽)
    // ========================
    FireExtinguisherText = MakeShared<STextBlock>();
    FireExtinguisherText->SetText(L"x" + std::to_wstring(FireExtinguisherCount))
        .SetFontSize(24.f)
        .SetColor(FSlateColor::White())
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

    SGameHUD::Get().AddWidget(FireExtinguisherText)
        .SetAnchor(1.f, 1.f)
        .SetPivot(1.f, 1.f)
        .SetOffset(-240.f, -95.f)
        .SetSize(60.f, 30.f);

    // ========================
    // 구조 카운트 텍스트 (우측 상단)
    // ========================
    RescueCountText = MakeShared<STextBlock>();
    std::wstring RescueStr = L"구조: " + std::to_wstring(RescuedCount) + L"/" + std::to_wstring(TotalPersonCount);
    RescueCountText->SetText(RescueStr)
        .SetFontSize(28.f)
        .SetColor(FSlateColor::White())
        .SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

    SGameHUD::Get().AddWidget(RescueCountText)
        .SetAnchor(1.f, 0.f)      // 우상단 앵커
        .SetPivot(1.f, 0.f)       // 우상단 피벗
        .SetOffset(-30.f, 30.f)   // 화면 가장자리에서 오프셋
        .SetSize(200.f, 40.f);

    // ========================
    // 공지 이미지 위젯 (화면 중앙, 3초간 크기 변화 애니메이션)
    // ========================
    NoticeWidget = MakeShared<STextBlock>();
    NoticeWidget->SetText(L"")
        .SetBackgroundImage("Data/Textures/Rescue/start.png");

    SGameHUD::Get().AddWidget(NoticeWidget)
        .SetAnchor(0.5f, 0.5f)      // 화면 중앙
        .SetPivot(0.5f, 0.5f)       // 중앙 기준
        .SetSize(1100.f, 400.f)      // 초기 크기
        .SetZOrder(100);            // 가장 위에 표시
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

    // 산소 수치 업데이트
    if (OxygenValueText)
    {
        // 사망 시 0 표시
        int32 DisplayOxygen = bPlayerDead ? 0 : static_cast<int32>(CurrentOxygen);
        OxygenValueText->SetText(std::to_wstring(DisplayOxygen));

        // 경고 상태 또는 사망 시 빨간색 표시
        if (bPlayerDead || CurrentOxygen < OxygenWarningThreshold)
        {
            OxygenValueText->SetColor(FSlateColor::Red());
            if (OxygenIconText)
            {
                OxygenIconText->SetColor(FSlateColor::Red());
            }
        }
        else
        {
            OxygenValueText->SetColor(FSlateColor::White());
            if (OxygenIconText)
            {
                OxygenIconText->SetColor(FSlateColor::White());
            }
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
        FireExtinguisherText->SetText(L"x" + std::to_wstring(FireExtinguisherCount));
    }

    // 구조 카운트 업데이트
    if (RescueCountText)
    {
        std::wstring RescueStr = L"구조: " + std::to_wstring(RescuedCount) + L"/" + std::to_wstring(TotalPersonCount);
        RescueCountText->SetText(RescueStr);
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
    // 물 마법 사용 중 확인
    if (PlayerController)
    {
        APawn* ControlledPawn = PlayerController->GetPawn();
        if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(ControlledPawn))
        {
            bIsUsingWaterMagic = Firefighter->IsUsingWaterMagic();
        }
    }

    // 물 마법 사용 시 물 게이지 감소
    if (bIsUsingWaterMagic)
    {
        CurrentWater -= WaterDecreaseRate * DeltaSeconds;

        // 물이 다 떨어지면 자동 충전
        if (CurrentWater <= 0.0f)
        {
            if (FireExtinguisherCount > 0)
            {
                // 소화기 사용하여 충전
                FireExtinguisherCount--;
                CurrentWater = BaseWater;

                // GameInstance에서도 아이템 제거
                UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
                if (GI)
                {
                    GI->RemoveItem("FireEX", 1);
                }

                UE_LOG("[info] RescueGameMode - Fire extinguisher used. Remaining: %d", FireExtinguisherCount);
            }
            else
            {
                // 소화기가 없으면 물 마법 사용 불가
                CurrentWater = 0.0f;
            }
        }
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
