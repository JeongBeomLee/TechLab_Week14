#pragma once
#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AEndGameMode.generated.h"

class SButton;
class STextBlock;
class APlayerCameraManager;
class UGameInstance;

/**
 * 엔딩 씬 전용 GameMode
 *
 * 기능:
 * - GameInstance에서 게임 결과 데이터 가져오기
 * - 순차적 연출: 구조 인원 카운팅 → 점수 카운팅 → 도장 찍기 → 메인 메뉴 버튼
 * - 도장 연출: 레터박스 + 카메라 쉐이크 + 도장 이미지 스케일 애니메이션
 * - 카메라 입력 비활성화
 */
UCLASS(DisplayName="엔딩 게임 모드", Description="게임 결과 화면 및 도장 연출 제공")
class AEndGameMode : public AGameModeBase
{
public:
    GENERATED_REFLECTION_BODY()

    AEndGameMode();
    virtual ~AEndGameMode() override = default;

    // ════════════════════════════════════════════════════════════════════════
    // 설정

    /**
     * 인트로 씬 경로 (메인 메뉴로 돌아가기)
     */
    UPROPERTY(EditAnywhere)
    FWideString IntroScenePath = L"Data/Scenes/IntroScene.scene";

    /**
     * 서류 배경 이미지 경로
     */
    UPROPERTY(EditAnywhere)
    FString DocumentImagePath = "Data/Textures/Ending/Fire_End_Paper.png";

    /**
     * 성공 도장 이미지 경로
     */
    UPROPERTY(EditAnywhere)
    FString SuccessStampImagePath = "Data/Textures/Ending/Fire_End_Success.png";

    /**
     * 실패 도장 이미지 경로
     */
    UPROPERTY(EditAnywhere)
    FString FailureStampImagePath = "Data/Textures/Ending/Fire_End_Failed.png";

    /**
     * 순직 도장 이미지 경로
     */
    UPROPERTY(EditAnywhere)
    FString MartyrdomStampImagePath = "Data/Textures/Ending/Fire_End_LODD.png";

    /**
     * 메인 메뉴 버튼 이미지 경로 (2465x1729, 가로로 Normal/Hovered)
     */
    UPROPERTY(EditAnywhere)
    FString MainMenuButtonImagePath = "Data/Textures/Ending/Fire_End_Btn.png";

    /**
     * 크레딧 이미지 경로
     */
    UPROPERTY(EditAnywhere)
    FString CreditImagePath = "Data/Textures/Ending/Fire_End_Credit.png";

    /**
     * 엔딩 멘트용 폰트 경로
     */
    UPROPERTY(EditAnywhere)
    FString EndingTextFontPath = "Data/UI/Fonts/ChosunKm.TTF";

    /**
     * 효과음 경로
     */
    UPROPERTY(EditAnywhere)
    FString CheckSoundPath = "Data/Audio/EndingScene/check.wav";

    UPROPERTY(EditAnywhere)
    FString StampSoundPath = "Data/Audio/EndingScene/stamp.wav";

    UPROPERTY(EditAnywhere)
    FString TypingSoundPath = "Data/Audio/EndingScene/typing.wav";

    UPROPERTY(EditAnywhere)
    FString ButtonSoundPath = "Data/Audio/EndingScene/button.wav";

    /**
     * 성공 판정 기준 구조 인원 수
     */
    UPROPERTY(EditAnywhere)
    int32 SuccessThreshold = 3;

    /**
     * 타이핑 효과 지속 시간 (초)
     */
    UPROPERTY(EditAnywhere)
    float TypingDuration = 3.0f;

    /**
     * 엔딩 멘트 표시 시작 지연 시간 (순서상 점수 카운팅 및 크레딧 표시 이후)
     */
    UPROPERTY(EditAnywhere)
    float EndingTextStartDelay = 0.5f;

    // ════════════════════════════════════════════════════════════════════════
    // 생명주기

    virtual void BeginPlay() override;
    virtual void EndPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // ════════════════════════════════════════════════════════════════════════
    // 시퀀스 타이머

    float SequenceTimer = 0.f;

    enum class ESequenceState : uint8
    {
        ShowDocument,       // 0초: 서류 배경 표시
        CountRescued,       // 1초: 구조 인원 카운팅
        CountScore,         // 2초: 점수 카운팅
        ShowEndingText,     // 3초: 엔딩 멘트 타이핑 효과
        ShowCredit,         // 4초: 크레딧 표시
        StampAnimation,     // 5초: 도장 찍기 연출
        ShowButton,         // 6초: 메인 메뉴 버튼 표시
        WaitForInput        // 대기
    };

    ESequenceState CurrentState = ESequenceState::ShowDocument;

    // ════════════════════════════════════════════════════════════════════════
    // 게임 데이터

    UGameInstance* GameInstance = nullptr;
    APlayerCameraManager* CameraManager = nullptr;

    int32 RescuedCount = 0;
    int32 PlayerScore = 0;
    int32 PlayerHealth = 0;

    // 카운팅 애니메이션용
    float RescuedCountCurrent = 0.f;
    float ScoreCountCurrent = 0.f;

    // ════════════════════════════════════════════════════════════════════════
    // UI 위젯

    TSharedPtr<STextBlock> DocumentBackground;  // 서류 배경
    TSharedPtr<STextBlock> RescuedText;         // 구조 인원 텍스트
    TSharedPtr<STextBlock> ScoreText;           // 점수 텍스트
    TSharedPtr<STextBlock> EndingText;          // 엔딩 멘트 텍스트 (타이핑 효과)
    TSharedPtr<STextBlock> StampImage;          // 도장 이미지
    TSharedPtr<STextBlock> CreditImage;         // 크레딧 이미지
    TSharedPtr<SButton> MainMenuButton;         // 메인 메뉴 버튼

    // 도장 애니메이션
    float StampAnimTimer = 0.f;
    float StampAnimDuration = 0.5f;
    bool bStampAnimStarted = false;

    // UI 흔들림 (화면 쉐이크 효과)
    float ShakeDuration = 0.3f;
    float ShakeTimer = 0.f;
    float ShakeIntensity = 20.f;  // 픽셀 단위

    // 원본 오프셋 저장 (위젯별)
    FVector2D OriginalDocumentOffset;
    FVector2D OriginalRescuedOffset;
    FVector2D OriginalScoreOffset;
    FVector2D OriginalEndingTextOffset;
    FVector2D OriginalStampOffset;
    FVector2D OriginalCreditOffset;
    FVector2D OriginalButtonOffset;

    // 타이핑 효과
    FWideString FullEndingText;      // 전체 엔딩 멘트
    int32 CurrentCharIndex = 0;      // 현재 표시된 글자 인덱스
    float TypingTimer = 0.f;         // 타이핑 타이머
    bool bTypingSoundPlayed = false; // 타이핑 사운드 재생 여부

    // 사운드
    class USound* CheckSound = nullptr;
    class USound* StampSound = nullptr;
    class USound* TypingSound = nullptr;
    class USound* ButtonSound = nullptr;

    // ════════════════════════════════════════════════════════════════════════
    // 내부 함수

    void InitializeData();
    void InitializeUI();
    void InitializeSounds();
    void ClearUI();

    void UpdateSequence(float DeltaTime);
    void StartRescuedCountAnimation();
    void StartEndingTextTyping();
    void UpdateTypingEffect(float DeltaTime);
    void StartScoreCountAnimation();
    void StartStampAnimation();
    void ShowMainMenuButton();

    FString GetStampImagePath() const;
    FWideString GetEndingText() const;

    static void OnMainMenuButtonClicked();
};
