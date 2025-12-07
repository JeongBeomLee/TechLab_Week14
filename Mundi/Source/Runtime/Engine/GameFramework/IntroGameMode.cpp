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

IMPLEMENT_CLASS(AIntroGameMode)

AIntroGameMode::AIntroGameMode()
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
    UInputManager::GetInstance().SetInputMode(EInputMode::UIOnly);

    // 사운드 초기화
    InitializeSounds();

    // UI 초기화
    InitializeUI();
}

void AIntroGameMode::EndPlay()
{
    Super::EndPlay();

    // 다른 씬으로 전환 시 입력 모드를 GameAndUI로 복원
    UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);

    ClearUI();
}

void AIntroGameMode::InitializeSounds()
{
    ButtonSound = UResourceManager::GetInstance().Load<USound>(ButtonSoundPath);
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
        .OnClicked(&AIntroGameMode::OnStartButtonClicked);

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
        .OnClicked(&AIntroGameMode::OnQuitButtonClicked);

    SGameHUD::Get().AddWidget(QuitButton)
        .SetAnchor(0.5f, 0.75f)       // 화면 중하단
        .SetPivot(0.5f, 0.5f)
        .SetOffset(230.f, 0.f)        // 오른쪽으로 230px (버튼 너비 400 + 여백 60 / 2)
        .SetSize(400.f, 220.f);
}

void AIntroGameMode::OnStartButtonClicked()
{
    // World를 통해 안전하게 LevelTransitionManager 접근
    if (!GEngine.IsPIEActive()) { return; }

    for (const auto& Context : GEngine.GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game && Context.World)
        {
            // IntroGameMode 인스턴스 찾아서 버튼 사운드 재생
            AIntroGameMode* IntroGameMode = Cast<AIntroGameMode>(Context.World->GetGameMode());
            if (IntroGameMode && IntroGameMode->ButtonSound)
            {
                FAudioDevice::PlaySound3D(IntroGameMode->ButtonSound, FVector::Zero(), 1.0f, false);
            }

            for (AActor* Actor : Context.World->GetActors())
            {
                if (ALevelTransitionManager* Manager = Cast<ALevelTransitionManager>(Actor))
                {
                    if (Manager->IsTransitioning()) { return; }

                    Manager->TransitionToNextLevel();
                    return;
                }
            }
            break;
        }
    }
}

void AIntroGameMode::OnQuitButtonClicked()
{
    // World를 통해 안전하게 IntroGameMode 접근하여 버튼 사운드 재생
    if (GEngine.IsPIEActive())
    {
        for (const auto& Context : GEngine.GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game && Context.World)
            {
                AIntroGameMode* IntroGameMode = Cast<AIntroGameMode>(Context.World->GetGameMode());
                if (IntroGameMode && IntroGameMode->ButtonSound)
                {
                    FAudioDevice::PlaySound3D(IntroGameMode->ButtonSound, FVector::Zero(), 1.0f, false);
                }
                break;
            }
        }
    }

    PostQuitMessage(0);
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
}
