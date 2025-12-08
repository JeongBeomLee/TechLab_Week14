#include "pch.h"
#include "EndGameMode.h"
#include "GameInstance.h"
#include "LevelTransitionManager.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "World.h"
#include "CameraActor.h"
#include "GameUI/SGameHUD.h"
#include "GameUI/SButton.h"
#include "GameUI/STextBlock.h"
#include "FAudioDevice.h"
#include "Sound.h"
#include "ResourceManager.h"

IMPLEMENT_CLASS(AEndGameMode)

AEndGameMode::AEndGameMode()
{
    ObjectName = "EndGameMode";
}

void AEndGameMode::BeginPlay()
{
    Super::BeginPlay();

    // LevelTransitionManager에 IntroScene 설정
    if (GetWorld())
    {
        for (AActor* Actor : GetWorld()->GetActors())
        {
            ALevelTransitionManager* Manager = dynamic_cast<ALevelTransitionManager*>(Actor);
            if (Manager)
            {
                Manager->SetNextLevel(IntroScenePath);
                break;
            }
        }
    }

    // 입력 모드를 UIOnly로 설정
    UInputManager::GetInstance().SetInputMode(EInputMode::UIOnly);

    // 데이터 초기화
    InitializeData();

    // 사운드 초기화
    InitializeSounds();

    // UI 초기화
    InitializeUI();
}

void AEndGameMode::EndPlay()
{
    Super::EndPlay();

    // 입력 모드 복원
    UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);

    ClearUI();
}

void AEndGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateSequence(DeltaTime);
}

void AEndGameMode::InitializeData()
{
    // GameInstance 가져오기
    GameInstance = GEngine.GetGameInstance();
    if (!GameInstance) { return; }

    // 게임 결과 데이터 가져오기
    RescuedCount = GameInstance->GetRescuedCount();
    PlayerScore = GameInstance->GetPlayerScore();
    PlayerHealth = GameInstance->GetPlayerHealth();

    // PlayerCameraManager 찾기
    if (GetWorld())
    {
        for (AActor* Actor : GetWorld()->GetActors())
        {
            if (APlayerCameraManager* Manager = dynamic_cast<APlayerCameraManager*>(Actor))
            {
                CameraManager = Manager;
                break;
            }
        }
    }
}

void AEndGameMode::InitializeSounds()
{
    // 사운드 리소스 로드
    CheckSound = UResourceManager::GetInstance().Load<USound>(CheckSoundPath);
    StampSound = UResourceManager::GetInstance().Load<USound>(StampSoundPath);
    TypingSound = UResourceManager::GetInstance().Load<USound>(TypingSoundPath);
    ButtonSound = UResourceManager::GetInstance().Load<USound>(ButtonSoundPath);
}

void AEndGameMode::InitializeUI()
{
    if (!SGameHUD::Get().IsInitialized()) { return; }

    // 서류 배경 이미지 (화면 중앙)
    DocumentBackground = MakeShared<STextBlock>();
    DocumentBackground->SetText(L"")
        .SetBackgroundImage(DocumentImagePath);

    SGameHUD::Get().AddWidget(DocumentBackground)
        .SetAnchor(0.5f, 0.5f)
        .SetPivot(0.5f, 0.5f)
        .SetSize(800.f, 1000.f);

    // 구조 인원 텍스트 (서류 우측 상단 첫 번째 줄)
    RescuedText = MakeShared<STextBlock>();
    RescuedText->SetText(L"0")
        .SetFontSize(40.f)
        .SetColor(FSlateColor::Black())
        .SetHAlign(ETextHAlign::Right);

    SGameHUD::Get().AddWidget(RescuedText)
        .SetAnchor(0.65f, 0.12f) 
        .SetPivot(1.0f, 0.5f)     // 오른쪽 정렬
        .SetSize(200.f, 50.f);
    RescuedText->SetVisibility(ESlateVisibility::Hidden);

    // 점수 텍스트 (서류 우측 상단 두 번째 줄)
    ScoreText = MakeShared<STextBlock>();
    ScoreText->SetText(L"0")
        .SetFontSize(40.f)
        .SetColor(FSlateColor::Black())
        .SetHAlign(ETextHAlign::Right);

    SGameHUD::Get().AddWidget(ScoreText)
        .SetAnchor(0.65f, 0.22f)
        .SetPivot(1.0f, 0.5f)     // 오른쪽 정렬
        .SetSize(200.f, 50.f);
    ScoreText->SetVisibility(ESlateVisibility::Hidden);

    // 엔딩 멘트 텍스트 (서류 중앙, 타이핑 효과)
    EndingText = MakeShared<STextBlock>();
    EndingText->SetText(L"")
        .SetFontSize(48.f)
        .SetFontPath(EndingTextFontPath)
        .SetColor(FSlateColor(0.15f, 0.10f, 0.07f, 1.0f))  // 더 어두운 다크브라운 (#261A12)
        .SetHAlign(ETextHAlign::Center);

    SGameHUD::Get().AddWidget(EndingText)
        .SetAnchor(0.5f, 0.45f)  // 0.52f에서 0.45f로 변경 (위로 이동)
        .SetPivot(0.5f, 0.5f)
        .SetSize(600.f, 200.f);
    EndingText->SetVisibility(ESlateVisibility::Hidden);

    // 도장 이미지 (서류 중앙보다 위, 처음에는 숨김)
    StampImage = MakeShared<STextBlock>();
    StampImage->SetText(L"")
        .SetBackgroundImage(GetStampImagePath());

    SGameHUD::Get().AddWidget(StampImage)
        .SetAnchor(0.5f, 0.45f) 
        .SetPivot(0.5f, 0.5f)
        .SetSize(0.f, 0.f);  // 처음에는 크기 0
    StampImage->SetVisibility(ESlateVisibility::Hidden);

    // 크레딧 이미지 (서류 하단, 처음에는 숨김)
    CreditImage = MakeShared<STextBlock>();
    CreditImage->SetText(L"")
        .SetBackgroundImage(CreditImagePath);

    SGameHUD::Get().AddWidget(CreditImage)
        .SetAnchor(0.5f, 0.67f)  
        .SetPivot(0.5f, 0.5f)
        .SetSize(700.f, 200.f);  // 크기 증가
    CreditImage->SetVisibility(ESlateVisibility::Hidden);
}

void AEndGameMode::ClearUI()
{
    if (!SGameHUD::Get().IsInitialized()) { return; }

    if (DocumentBackground)
    {
        SGameHUD::Get().RemoveWidget(DocumentBackground);
        DocumentBackground.Reset();
    }

    if (RescuedText)
    {
        SGameHUD::Get().RemoveWidget(RescuedText);
        RescuedText.Reset();
    }

    if (ScoreText)
    {
        SGameHUD::Get().RemoveWidget(ScoreText);
        ScoreText.Reset();
    }

    if (EndingText)
    {
        SGameHUD::Get().RemoveWidget(EndingText);
        EndingText.Reset();
    }

    if (StampImage)
    {
        SGameHUD::Get().RemoveWidget(StampImage);
        StampImage.Reset();
    }

    if (CreditImage)
    {
        SGameHUD::Get().RemoveWidget(CreditImage);
        CreditImage.Reset();
    }

    if (MainMenuButton)
    {
        SGameHUD::Get().RemoveWidget(MainMenuButton);
        MainMenuButton.Reset();
    }
}

void AEndGameMode::UpdateSequence(float DeltaTime)
{
    SequenceTimer += DeltaTime;

    switch (CurrentState)
    {
    case ESequenceState::ShowDocument:
        if (SequenceTimer >= 1.0f)
        {
            StartRescuedCountAnimation();
            CurrentState = ESequenceState::CountRescued;
            SequenceTimer = 0.f;
        }
        break;

    case ESequenceState::CountRescued:
        // 카운팅 애니메이션 (1초 동안)
        if (SequenceTimer < 1.0f)
        {
            RescuedCountCurrent = FMath::Lerp(0.f, (float)RescuedCount, SequenceTimer);
            if (RescuedText)
            {
                FWideString Text = std::to_wstring((int)RescuedCountCurrent);
                RescuedText->SetText(Text);
            }
        }
        else
        {
            // 카운팅 완료
            if (RescuedText)
            {
                FWideString Text = std::to_wstring(RescuedCount);
                RescuedText->SetText(Text);
            }
            StartScoreCountAnimation();
            CurrentState = ESequenceState::CountScore;
            SequenceTimer = 0.f;
        }
        break;

    case ESequenceState::CountScore:
        // 카운팅 애니메이션 (1초 동안)
        if (SequenceTimer < 1.0f)
        {
            ScoreCountCurrent = FMath::Lerp(0.f, (float)PlayerScore, SequenceTimer);
            if (ScoreText)
            {
                FWideString Text = std::to_wstring((int)ScoreCountCurrent);
                ScoreText->SetText(Text);
            }
        }
        else
        {
            // 카운팅 완료 후 엔딩 멘트 타이핑 시작
            if (ScoreText)
            {
                FWideString Text = std::to_wstring(PlayerScore);
                ScoreText->SetText(Text);
            }
            StartEndingTextTyping();
            CurrentState = ESequenceState::ShowEndingText;
            SequenceTimer = 0.f;
        }
        break;

    case ESequenceState::ShowEndingText:
        // 타이핑 효과
        if (SequenceTimer < EndingTextStartDelay)
        {
            // 딜레이 대기
        }
        else
        {
            UpdateTypingEffect(DeltaTime);

            // 타이핑 완료 체크 (타이핑 시작 딜레이 + 타이핑 지속 시간 + 완료 후 1초 대기)
            float totalEndingTextTime = EndingTextStartDelay + TypingDuration + 1.0f;
            if (SequenceTimer >= totalEndingTextTime)
            {
                // 크레딧 표시
                if (CreditImage)
                {
                    CreditImage->SetVisibility(ESlateVisibility::Visible);
                }

                // 크레딧 사운드 재생
                if (CheckSound)
                {
                    FAudioDevice::PlaySound3D(CheckSound, FVector::Zero(), 1.0f, false);
                }

                CurrentState = ESequenceState::ShowCredit;
                SequenceTimer = 0.f;
            }
        }
        break;

    case ESequenceState::ShowCredit:
        // 크레딧 표시 후 1초 대기
        if (SequenceTimer >= 1.0f)
        {
            StartStampAnimation();
            CurrentState = ESequenceState::StampAnimation;
            SequenceTimer = 0.f;
        }
        break;

    case ESequenceState::StampAnimation:
        if (SequenceTimer < StampAnimDuration)
        {
            // 도장 애니메이션 (EaseOutBack 느낌)
            float t = SequenceTimer / StampAnimDuration;
            float scale = 1.5f + (1.0f - 1.5f) * (t * t * t);  // 1.5 → 1.0

            if (StampImage && SGameHUD::Get().IsInitialized())
            {
                // 크기 조정 (신용카드 비율 1.586:1) - 기존보다 20% 크게
                float width = 720.f * scale;   // 가로 (600 → 720)
                float height = 454.f * scale;  // 세로 (378 → 454, 720 / 1.586 ≈ 454)

                // RootCanvas에서 위젯 찾아서 크기 변경
                if (TSharedPtr<SCanvas> Canvas = SGameHUD::Get().GetRootCanvas())
                {
                    TArray<FCanvasSlot>& Slots = Canvas->GetCanvasSlots();
                    for (FCanvasSlot& Slot : Slots)
                    {
                        if (Slot.Widget == StampImage)
                        {
                            Slot.SetSize(FVector2D(width, height));
                            break;
                        }
                    }
                }
            }

            // UI 흔들림 효과 (도장 찍힐 때만)
            if (ShakeTimer < ShakeDuration)
            {
                ShakeTimer += DeltaTime;
                float shakeT = ShakeTimer / ShakeDuration;
                float shakeDecay = 1.0f - shakeT;  // 시간이 지날수록 감소

                // 랜덤 오프셋 생성 (-1.0 ~ 1.0)
                float randX = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
                float randY = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
                float offsetX = randX * ShakeIntensity * shakeDecay;
                float offsetY = randY * ShakeIntensity * shakeDecay;

                // 모든 UI 위젯에 오프셋 적용
                TSharedPtr<SCanvas> Canvas = SGameHUD::Get().GetRootCanvas();
                if (Canvas)
                {
                    TArray<FCanvasSlot>& Slots = Canvas->GetCanvasSlots();
                    for (FCanvasSlot& Slot : Slots)
                    {
                        // 각 위젯의 원본 오프셋에 흔들림 추가
                        if (Slot.Widget == DocumentBackground)
                            Slot.SetOffset(FVector2D(OriginalDocumentOffset.X + offsetX, OriginalDocumentOffset.Y + offsetY));
                        else if (Slot.Widget == RescuedText)
                            Slot.SetOffset(FVector2D(OriginalRescuedOffset.X + offsetX, OriginalRescuedOffset.Y + offsetY));
                        else if (Slot.Widget == ScoreText)
                            Slot.SetOffset(FVector2D(OriginalScoreOffset.X + offsetX, OriginalScoreOffset.Y + offsetY));
                        else if (Slot.Widget == EndingText)
                            Slot.SetOffset(FVector2D(OriginalEndingTextOffset.X + offsetX, OriginalEndingTextOffset.Y + offsetY));
                        else if (Slot.Widget == StampImage)
                            Slot.SetOffset(FVector2D(OriginalStampOffset.X + offsetX, OriginalStampOffset.Y + offsetY));
                        else if (Slot.Widget == CreditImage)
                            Slot.SetOffset(FVector2D(OriginalCreditOffset.X + offsetX, OriginalCreditOffset.Y + offsetY));
                        else if (Slot.Widget == MainMenuButton)
                            Slot.SetOffset(FVector2D(OriginalButtonOffset.X + offsetX, OriginalButtonOffset.Y + offsetY));
                    }
                }
            }
            else if (ShakeTimer >= ShakeDuration)
            {
                // 흔들림 종료 후 원위치 복원
                TSharedPtr<SCanvas> Canvas = SGameHUD::Get().GetRootCanvas();
                if (Canvas)
                {
                    TArray<FCanvasSlot>& Slots = Canvas->GetCanvasSlots();
                    for (FCanvasSlot& Slot : Slots)
                    {
                        if (Slot.Widget == DocumentBackground) Slot.SetOffset(OriginalDocumentOffset);
                        else if (Slot.Widget == RescuedText) Slot.SetOffset(OriginalRescuedOffset);
                        else if (Slot.Widget == ScoreText) Slot.SetOffset(OriginalScoreOffset);
                        else if (Slot.Widget == EndingText) Slot.SetOffset(OriginalEndingTextOffset);
                        else if (Slot.Widget == StampImage) Slot.SetOffset(OriginalStampOffset);
                        else if (Slot.Widget == CreditImage) Slot.SetOffset(OriginalCreditOffset);
                        else if (Slot.Widget == MainMenuButton) Slot.SetOffset(OriginalButtonOffset);
                    }
                }
            }
        }
        else if (SequenceTimer >= 1.0f)
        {
            ShowMainMenuButton();
            CurrentState = ESequenceState::ShowButton;
            SequenceTimer = 0.f;
        }
        break;

    case ESequenceState::ShowButton:
        // 버튼 표시 후 대기
        CurrentState = ESequenceState::WaitForInput;
        break;

    case ESequenceState::WaitForInput:
        // 사용자 입력 대기 (버튼 클릭 대기)
        break;
    }
}

void AEndGameMode::StartRescuedCountAnimation()
{
    if (RescuedText)
    {
        RescuedText->SetVisibility(ESlateVisibility::Visible);
    }

    // 체크 사운드 재생
    if (CheckSound)
    {
        FAudioDevice::PlaySound3D(CheckSound, FVector::Zero(), 1.0f, false);
    }
}

void AEndGameMode::StartScoreCountAnimation()
{
    if (ScoreText)
    {
        ScoreText->SetVisibility(ESlateVisibility::Visible);
    }

    // 체크 사운드 재생
    if (CheckSound)
    {
        FAudioDevice::PlaySound3D(CheckSound, FVector::Zero(), 1.0f, false);
    }
}

void AEndGameMode::StartStampAnimation()
{
    // 도장 사운드 재생
    if (StampSound)
    {
        FAudioDevice::PlaySound3D(StampSound, FVector::Zero(), 1.0f, false);
    }

    // UI 흔들림 타이머 초기화 및 원본 오프셋 저장
    ShakeTimer = 0.f;

    if (TSharedPtr<SCanvas> Canvas = SGameHUD::Get().GetRootCanvas())
    {
        TArray<FCanvasSlot>& Slots = Canvas->GetCanvasSlots();
        for (FCanvasSlot& Slot : Slots)
        {
            if (Slot.Widget == DocumentBackground) OriginalDocumentOffset = Slot.Offset;
            else if (Slot.Widget == RescuedText) OriginalRescuedOffset = Slot.Offset;
            else if (Slot.Widget == ScoreText) OriginalScoreOffset = Slot.Offset;
            else if (Slot.Widget == EndingText) OriginalEndingTextOffset = Slot.Offset;
            else if (Slot.Widget == StampImage) OriginalStampOffset = Slot.Offset;
            else if (Slot.Widget == CreditImage) OriginalCreditOffset = Slot.Offset;
            else if (Slot.Widget == MainMenuButton) OriginalButtonOffset = Slot.Offset;
        }
    }

    // 도장 이미지 표시
    if (StampImage) { StampImage->SetVisibility(ESlateVisibility::Visible); }
    StampAnimTimer = 0.f;
    bStampAnimStarted = true;
}

void AEndGameMode::ShowMainMenuButton()
{
    if (!SGameHUD::Get().IsInitialized()) { return; }

    // 메인 메뉴 버튼 (2258x850 이미지, 가로로 반 나눔)
    // Left, Top, Right, Bottom 형식
    MainMenuButton = MakeShared<SButton>();
    MainMenuButton->SetText(L"")  // 이미지만 사용
        .SetBackgroundImageAtlas(
            MainMenuButtonImagePath,
            FSlateRect(0.f, 0.f, 1129.f, 850.f),       // Normal: 왼쪽 (0~1129, 0~850)
            FSlateRect(1129.f, 0.f, 2258.f, 850.f),    // Hovered: 오른쪽 (1129~2258, 0~850)
            FSlateRect(1129.f, 0.f, 2258.f, 850.f)     // Pressed: Hovered와 동일
        )
        .OnClicked(&AEndGameMode::OnMainMenuButtonClicked);

    SGameHUD::Get().AddWidget(MainMenuButton)
        .SetAnchor(0.5f, 0.85f)
        .SetPivot(0.5f, 0.5f)
        .SetSize(300.f, 226.f);  // 1129:850 비율 유지 (약 1.33 비율)
}

FString AEndGameMode::GetStampImagePath() const
{
    // HP가 0이면 순직
    if (PlayerHealth <= 0)
    {
        return MartyrdomStampImagePath;
    }

    // HP가 0이 아니면 구조 인원 수로 판정
    if (RescuedCount >= SuccessThreshold)
    {
        return SuccessStampImagePath;
    }
    else
    {
        return FailureStampImagePath;
    }
}

FWideString AEndGameMode::GetEndingText() const
{
    // 엔딩 타입별 멘트 (각 2종류씩)
    static const FWideString MartyrdomTexts[2] = {
        L"당신의 희생은\n결코 잊혀지지 않을 것입니다.",
        L"영웅은 떠났지만\n그 용기는 영원히 기억됩니다."
    };

    static const FWideString FailureTexts[2] = {
        L"더 많은 생명을 구하지 못해\n안타깝습니다.",
        L"때로는 최선을 다해도\n모두를 구할 수는 없습니다."
    };

    static const FWideString SuccessTexts[2] = {
        L"당신의 용기로\n많은 생명을 구했습니다.",
        L"훌륭한 임무 수행이었습니다.\n모두가 당신에게 감사합니다."
    };

    // 랜덤 인덱스 (0 또는 1)
    int32 randomIndex = rand() % 2;

    // HP가 0이면 순직
    if (PlayerHealth <= 0)
    {
        return MartyrdomTexts[randomIndex];
    }

    // HP가 0이 아니면 구조 인원 수로 판정
    if (RescuedCount >= SuccessThreshold)
    {
        return SuccessTexts[randomIndex];
    }
    else
    {
        return FailureTexts[randomIndex];
    }
}

void AEndGameMode::StartEndingTextTyping()
{
    // 엔딩 멘트 가져오기
    FullEndingText = GetEndingText();
    CurrentCharIndex = 0;
    TypingTimer = 0.f;
    bTypingSoundPlayed = false;  // 타이핑 사운드 재생 플래그 리셋

    if (EndingText)
    {
        EndingText->SetVisibility(ESlateVisibility::Visible);
        EndingText->SetText(L"");
    }
}

void AEndGameMode::UpdateTypingEffect(float DeltaTime)
{
    if (!EndingText || CurrentCharIndex >= (int32)FullEndingText.length()) { return; }

    TypingTimer += DeltaTime;

    // 전체 텍스트 길이를 기준으로 타이핑 속도 계산
    int32 totalChars = (int32)FullEndingText.length();
    if (totalChars == 0) { return; }

    // 진행률 기반으로 현재 표시할 글자 인덱스 계산
    float progress = FMath::Clamp(TypingTimer / TypingDuration, 0.f, 1.f);
    int32 targetIndex = (int32)(progress * totalChars);

    if (targetIndex > CurrentCharIndex)
    {
        CurrentCharIndex = targetIndex;

        // 현재까지의 텍스트 표시
        FWideString displayText = FullEndingText.substr(0, CurrentCharIndex);
        EndingText->SetText(displayText);

        // 타이핑 사운드 재생 (딱 1번만, 타이핑 시작할 때)
        if (TypingSound && !bTypingSoundPlayed)
        {
            FAudioDevice::PlaySound3D(TypingSound, FVector::Zero(), 0.5f, false);
            bTypingSoundPlayed = true;
        }
    }
}

void AEndGameMode::OnMainMenuButtonClicked()
{
    // 버튼 클릭 사운드 재생
    // static 메서드이므로 인스턴스를 찾아서 사운드 재생
    if (!GEngine.IsPIEActive()) { return; }

    for (const auto& Context : GEngine.GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game && Context.World)
        {
            // EndGameMode 인스턴스 찾기
            AEndGameMode* EndGameMode = Cast<AEndGameMode>(Context.World->GetGameMode());
            if (EndGameMode && EndGameMode->ButtonSound)
            {
                UE_LOG("[info] EndGameMode: Playing button click sound");
                FAudioDevice::PlaySound3D(EndGameMode->ButtonSound, FVector::Zero(), 1.0f, false);
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
