#pragma once
#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AFireDispatchGameMode.generated.h"

class USound;

enum class EFireDispatchPhase : uint8
{
    Run,        // 달리기
    Fall,       // 넘어짐
    GetUp,      // 일어남
    Walk,       // 차량까지 걷기
    Board,      // 차량 탑승
    Drive,      // 차량 출발
    Complete    // 완료
};

UCLASS(DisplayName="소방관 출동 게임 모드", Description="소방관 출동 컷씬 제공")
class AFireDispatchGameMode : public AGameModeBase
{
public:
    GENERATED_REFLECTION_BODY()

    AFireDispatchGameMode();
    virtual ~AFireDispatchGameMode() override = default;

    /**
     * 다음 씬 경로 (컷씬 완료 후 이동)
     */
    UPROPERTY(EditAnywhere)
    FWideString NextScenePath = L"Data/Scenes/Ending.scene";

    /**
     * 효과음 경로
     */
    UPROPERTY(EditAnywhere)
    FString CarSoundPath = "Data/Audio/CutScene/car.wav";

    UPROPERTY(EditAnywhere)
    FString CarOpenPath = "Data/Audio/CutScene/open.wav";

    UPROPERTY(EditAnywhere)
    FString CarPassPath = "Data/Audio/CutScene/pass.wav";

    // ════════════════════════════════════════════════════════════════════════
    // 타이밍 설정

    UPROPERTY(EditAnywhere)
    float RunDuration = 2.0f;

    UPROPERTY(EditAnywhere)
    float FallDuration = 2.0f;

    UPROPERTY(EditAnywhere)
    float GetUpDuration = 2.0f;

    UPROPERTY(EditAnywhere)
    float WalkDuration = 3.0f;

    UPROPERTY(EditAnywhere)
    float BoardDuration = 1.0f;

    UPROPERTY(EditAnywhere)
    float DriveDuration = 2.0f;

    UPROPERTY(EditAnywhere)
    float FadeoutDuration = 2.0f;

    // ════════════════════════════════════════════════════════════════════════
    // 물리 설정

    UPROPERTY(EditAnywhere)
    float RunSpeed = 5.0f;

    UPROPERTY(EditAnywhere)
    float WalkSpeed = 3.0f;

    UPROPERTY(EditAnywhere)
    float FallImpulseX = 0.0f;

    UPROPERTY(EditAnywhere)
    float FallImpulseZ = 25.0f;

    // ════════════════════════════════════════════════════════════════════════
    // 카메라 전환 설정

    UPROPERTY(EditAnywhere)
    float CameraBlendTime = 0.5f;

    UPROPERTY(EditAnywhere)
    FVector CameraTargetPosition = FVector(-48.5f, 31.0f, -18.5f);

    UPROPERTY(EditAnywhere)
    FVector CameraTargetRotation = FVector(0.0f, -24.0f, 90.0f);

    // ════════════════════════════════════════════════════════════════════════
    // 카메라 쉐이크 설정 (차량 시동 효과)

    UPROPERTY(EditAnywhere)
    float ShakeDelay = 0.6f;  // 트랜지션 후 쉐이크 시작까지 대기 시간

    UPROPERTY(EditAnywhere)
    float ShakeDuration = 1.5f;  // 쉐이크 지속 시간

    UPROPERTY(EditAnywhere)
    float ShakeIntensity = 0.2f;  // 강렬한 시동 효과

    UPROPERTY(EditAnywhere)
    float ShakeFrequency = 2.0f;  // 빠른 진동

    void BeginPlay() override;
    void EndPlay() override;
    void Tick(float DeltaTime) override;

private:
    // 시퀀스 타이머
    float SequenceTimer = 0.f;

    EFireDispatchPhase CurrentPhase = EFireDispatchPhase::Run;
    USound* CarSound = nullptr;
    USound* OpenSound = nullptr;
    USound* PassSound = nullptr;
    class APlayerCameraManager* CameraManager = nullptr;
    class ACameraActor* MainCamera = nullptr;

    // 카메라 전환 상태
    bool bCameraTransitioning = false;
    float CameraTransitionTimer = 0.f;
    FVector CameraStartPosition;
    FQuat CameraStartRotation;

    // 카메라 쉐이크 지연 상태
    bool bWaitingForShake = false;
    float ShakeDelayTimer = 0.f;

    // 문 여는 소리 지연 상태
    bool bWaitingForDoorSound = false;
    float DoorSoundDelayTimer = 0.f;
    float DoorSoundDelay = 0.5f;

    // 내부 함수
    void InitializeAssets();
    void InitializeCameraEffects();
    void FindMainCamera();

    void UpdatePhases(float DeltaTime);
    void TransitionToPhase(EFireDispatchPhase NewPhase);
    void UpdateCameraTransition(float DeltaTime);
    void StartCameraTransition();
    void UpdateCameraShakeDelay(float DeltaTime);
    void UpdateDoorSoundDelay(float DeltaTime);

    float GetPhaseDuration(EFireDispatchPhase Phase) const;
};
