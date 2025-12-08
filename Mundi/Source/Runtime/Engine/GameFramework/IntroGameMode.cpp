#include "pch.h"
#include "IntroGameMode.h"
#include "LevelTransitionManager.h"
#include "InputManager.h"
#include "World.h"
#include "GameUI/SGameHUD.h"
#include "GameUI/SButton.h"
#include "GameUI/STextBlock.h"
#include "FAudioDevice.h"
#include "Sound.h"
#include "ResourceManager.h"
#include "PlayerController.h"
#include "PlayerCameraManager.h"

IMPLEMENT_CLASS(AIntroGameMode)

AIntroGameMode::AIntroGameMode()
    : bIsTransitioning(false)
    , TransitionTimer(0.0f)
    , bIsQuitting(false)
{
    ObjectName = "IntroGameMode";
}

void AIntroGameMode::BeginPlay()
{
    Super::BeginPlay();

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

    // 입력 모드를 UIOnly로 설정 (마우스 클릭만 허용, 키보드/카메라 입력 차단)
    if (PlayerController)
    {
        PlayerController->SetInputMode(EInputMode::UIOnly);
    }
    else
    {
        UInputManager::GetInstance().SetInputMode(EInputMode::UIOnly);
    }

    // 사운드 초기화
    InitializeSounds();

    // UI 초기화
    InitializeUI();
}

void AIntroGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 와이프 전환 타이머 처리 (게임 시작 버튼만 해당)
    if (bIsTransitioning)
    {
        TransitionTimer += DeltaTime;

        if (TransitionTimer >= WipeDuration)
        {
            // 다음 씬으로 전환
            if (GetWorld())
            {
                if (ALevelTransitionManager* Manager = GetWorld()->FindActor<ALevelTransitionManager>())
                {
                    if (!Manager->IsTransitioning())
                    {
                        Manager->TransitionToNextLevel();
                    }
                }
            }
            bIsTransitioning = false;
        }
    }
}

void AIntroGameMode::EndPlay()
{
    Super::EndPlay();

    // BGM 정지
    if (BGMVoice)
    {
        FAudioDevice::StopSound(BGMVoice);
        BGMVoice = nullptr;
    }

    // 다른 씬으로 전환 시 입력 모드를 GameAndUI로 복원
    if (PlayerController)
    {
        PlayerController->SetInputMode(EInputMode::GameAndUI);
    }
    else
    {
        UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);
    }

    ClearUI();
}

void AIntroGameMode::InitializeSounds()
{
    ButtonSound = UResourceManager::GetInstance().Load<USound>(ButtonSoundPath);

    // BGM 로드 및 재생 (루프)
    BGMSound = UResourceManager::GetInstance().Load<USound>(BGMSoundPath);
    if (BGMSound)
    {
        BGMVoice = FAudioDevice::PlaySound3D(BGMSound, FVector::Zero(), 0.5f, true);
    }
}

void AIntroGameMode::InitializeUI()
{
    if (!SGameHUD::Get().IsInitialized()) { return; }

    // 타이틀 이미지 (2816 x 1536 이미지, 비율 유지하며 적절한 크기로 표시)
    // 화면 중상단 (Y: 0.25)
    TitleWidget = MakeShared<STextBlock>();
    TitleWidget->SetText(L"")  // 텍스트 없음, 배경 이미지만 사용
        .SetBackgroundImage("Data/Textures/Intro/Fire_Scene1_logo.png");

    SGameHUD::Get().AddWidget(TitleWidget)
        .SetAnchor(0.5f, 0.35f)      // 화면 중상단
        .SetPivot(0.5f, 0.5f)         // 중앙 기준
        .SetSize(1600.f, 874.f);       // 2816:1536 비율 유지 (800 x 437)

    // 게임 시작 버튼 (화면 중하단 왼쪽)
    // Y: 0.75, 왼쪽으로 오프셋
    StartButton = MakeShared<SButton>();
    StartButton->SetText(StartButtonText)
        .SetBackgroundImageAtlas(
            "Data/Textures/Intro/Fire_Scene1_Btn.png",
            FSlateRect(0.f, 0.f, 1408.f, 768.f),      // Normal: 왼쪽 위
            FSlateRect(0.f, 768.f, 1408.f, 1536.f),   // Hovered: 왼쪽 아래
            FSlateRect(0.f, 768.f, 1408.f, 1536.f)    // Pressed: Hovered와 동일
        )
        .SetFontSize(28.f)
        .OnClicked([this]() { OnStartButtonClicked(); });

    SGameHUD::Get().AddWidget(StartButton)
        .SetAnchor(0.5f, 0.75f)       // 화면 중하단
        .SetPivot(0.5f, 0.5f)
        .SetOffset(-230.f, 0.f)       // 왼쪽으로 230px (버튼 너비 400 + 여백 60 / 2)
        .SetSize(400.f, 220.f);

    // 나가기 버튼 (화면 중하단 오른쪽)
    // Y: 0.75, 오른쪽으로 오프셋
    QuitButton = MakeShared<SButton>();
    QuitButton->SetText(QuitButtonText)
        .SetBackgroundImageAtlas(
            "Data/Textures/Intro/Fire_Scene1_Btn.png",
            FSlateRect(1408.f, 0.f, 2816.f, 768.f),     // Normal: 오른쪽 위
            FSlateRect(1408.f, 768.f, 2816.f, 1536.f),  // Hovered: 오른쪽 아래
            FSlateRect(1408.f, 768.f, 2816.f, 1536.f)   // Pressed: Hovered와 동일
        )
        .SetFontSize(28.f)
        .OnClicked([this]() { OnQuitButtonClicked(); });

    SGameHUD::Get().AddWidget(QuitButton)
        .SetAnchor(0.5f, 0.75f)       // 화면 중하단
        .SetPivot(0.5f, 0.5f)
        .SetOffset(230.f, 0.f)        // 오른쪽으로 230px (버튼 너비 400 + 여백 60 / 2)
        .SetSize(400.f, 220.f);

    // 도움말 버튼 (우측 상단)
    HelpButton = MakeShared<SButton>();
    HelpButton->SetText(L"")
        .SetBackgroundImageAtlas(
            "Data/Textures/Intro/Fire_Scene1_Btn_Help.png",
            FSlateRect(0.f, 0.f, 768.f, 768.f),      // Normal: 왼쪽
            FSlateRect(768.f, 0.f, 1536.f, 768.f),   // Hovered: 오른쪽
            FSlateRect(768.f, 0.f, 1536.f, 768.f)    // Pressed: Hovered와 동일
        )
        .OnHover([this](bool bIsHovered) {
            if (bIsHovered)
                OnHelpButtonHovered();
            else
                OnHelpButtonUnhovered();
        });

    SGameHUD::Get().AddWidget(HelpButton)
        .SetAnchor(1.0f, 0.0f)        // 우측 상단
        .SetPivot(1.0f, 0.0f)         // 우측 상단 기준
        .SetOffset(-20.f, 20.f)       // 우측 상단에서 약간의 여백
        .SetSize(200.f, 200.f);

    // 가이드 이미지 (화면 중앙, 초기에는 숨김)
    GuideWidget = MakeShared<STextBlock>();
    GuideWidget->SetText(L"")
        .SetBackgroundImage("Data/Textures/Intro/Fire_Scene1_Guide.png")
        .SetVisibility(ESlateVisibility::Hidden);

    SGameHUD::Get().AddWidget(GuideWidget)
        .SetAnchor(0.5f, 0.5f)        // 화면 중앙
        .SetPivot(0.5f, 0.5f)         // 중앙 기준
        .SetSize(1350.f, 800.f);       // 적절한 크기로 조정
}

void AIntroGameMode::OnStartButtonClicked()
{
    if (bIsTransitioning) { return; }

    // 버튼 사운드 재생
    if (ButtonSound)
    {
        FAudioDevice::PlaySound3D(ButtonSound, FVector::Zero(), 1.0f, false);
    }

    // 모든 UI 즉시 숨김
    if (TitleWidget) TitleWidget->SetVisibility(ESlateVisibility::Hidden);
    if (StartButton) StartButton->SetVisibility(ESlateVisibility::Hidden);
    if (QuitButton) QuitButton->SetVisibility(ESlateVisibility::Hidden);
    if (HelpButton) HelpButton->SetVisibility(ESlateVisibility::Hidden);
    if (GuideWidget) GuideWidget->SetVisibility(ESlateVisibility::Hidden);

    // 수직 줄무늬 와이프 시작
    if (GetWorld())
    {
        if (APlayerCameraManager* CameraManager = GetWorld()->FindActor<APlayerCameraManager>())
        {
            CameraManager->StartStripedWipe(WipeDuration, 8.f, FLinearColor(0,0,0,1), 100);
        }
    }

    bIsTransitioning = true;
    bIsQuitting = false;
    TransitionTimer = 0.0f;
}

void AIntroGameMode::OnQuitButtonClicked()
{
    if (bIsTransitioning) { return; }

    // 즉시 게임 종료 (UI 정리는 자동으로 수행됨)
    PostQuitMessage(0);
}

void AIntroGameMode::OnHelpButtonHovered()
{
    if (GuideWidget)
    {
        GuideWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void AIntroGameMode::OnHelpButtonUnhovered()
{
    if (GuideWidget)
    {
        GuideWidget->SetVisibility(ESlateVisibility::Hidden);
    }
}

void AIntroGameMode::ClearUI()
{
    if (!SGameHUD::Get().IsInitialized()) { return; }

    if (TitleWidget)
    {
        SGameHUD::Get().RemoveWidget(TitleWidget);
        TitleWidget.Reset();
    }

    if (StartButton)
    {
        SGameHUD::Get().RemoveWidget(StartButton);
        StartButton.Reset();
    }

    if (QuitButton)
    {
        SGameHUD::Get().RemoveWidget(QuitButton);
        QuitButton.Reset();
    }

    if (HelpButton)
    {
        SGameHUD::Get().RemoveWidget(HelpButton);
        HelpButton.Reset();
    }

    if (GuideWidget)
    {
        SGameHUD::Get().RemoveWidget(GuideWidget);
        GuideWidget.Reset();
    }
}
