#include "pch.h"
#include "FireDispatchGameMode.h"
#include "LevelTransitionManager.h"
#include "FAudioDevice.h"
#include "Sound.h"
#include "ResourceManager.h"
#include "PlayerCameraManager.h"
#include "Camera/CamMod_LetterBox.h"
#include "Camera/CamMod_Fade.h"
#include "Camera/CamMod_Shake.h"
#include "GameInstance.h"
#include "CameraActor.h"
#include "World.h"
#include "PlayerController.h"
#include "InputManager.h"

IMPLEMENT_CLASS(AFireDispatchGameMode)

AFireDispatchGameMode::AFireDispatchGameMode()
{
    ObjectName = "FireDispatchGameMode";
}

void AFireDispatchGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (GetWorld())
    {
        if (ALevelTransitionManager* Manager = GetWorld()->FindActor<ALevelTransitionManager>())
        {
            Manager->SetNextLevel(NextScenePath);
        }
    }

    // 입력 모드를 UIOnly로 설정 (PlayerController를 통해 설정)
    if (PlayerController)
    {
        PlayerController->SetInputMode(EInputMode::UIOnly);
    }
    else
    {
        UInputManager::GetInstance().SetInputMode(EInputMode::UIOnly);
    }

    InitializeAssets();
    InitializeCameraEffects();
    FindMainCamera();

    // 첫 페이즈 시작
    TransitionToPhase(EFireDispatchPhase::Run);
}

void AFireDispatchGameMode::EndPlay()
{
    Super::EndPlay();

    // 입력 모드 복원 (PlayerController를 통해 설정)
    if (PlayerController)
    {
        PlayerController->SetInputMode(EInputMode::GameAndUI);
    }
    else
    {
        UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);
    }
}

void AFireDispatchGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdatePhases(DeltaTime);
    UpdateCameraTransition(DeltaTime);
    UpdateCameraShakeDelay(DeltaTime);
    UpdateDoorSoundDelay(DeltaTime);
    UpdateOuchSoundDelay(DeltaTime);
    UpdateVibration(DeltaTime);
}

void AFireDispatchGameMode::InitializeAssets()
{
    CarSound = UResourceManager::GetInstance().Load<USound>(CarSoundPath);
    OpenSound = UResourceManager::GetInstance().Load<USound>(CarOpenPath);
    PassSound = UResourceManager::GetInstance().Load<USound>(CarPassPath);
    ScreamSound = UResourceManager::GetInstance().Load<USound>(ScreamSoundPath);
    OuchSound = UResourceManager::GetInstance().Load<USound>(OuchSoundPath);
    CarCrashSound = UResourceManager::GetInstance().Load<USound>(CarCrashSoundPath);
    UE_LOG("[FireDispatchGameMode] OuchSound loaded: %p (path: %s)", OuchSound, OuchSoundPath.c_str());
}

void AFireDispatchGameMode::InitializeCameraEffects()
{
    if (!GetWorld()) { return; }
    if (CameraManager = GetWorld()->FindActor<APlayerCameraManager>(); !CameraManager) { return; }
    CameraManager->StartLetterBox(-1.0f, 1.85f, 0.0f, FLinearColor(0,0,0,1), 100);
    CameraManager->StartFade(1.5f, 1.0f, 0.0f, FLinearColor(0, 0, 0));
}

void AFireDispatchGameMode::FindMainCamera()
{
    if (!GetWorld()) { return; }

    // 씬에서 카메라 액터 찾기
    MainCamera = GetWorld()->FindActor<ACameraActor>();
}

void AFireDispatchGameMode::UpdatePhases(float DeltaTime)
{
    SequenceTimer += DeltaTime;

    float phaseDuration = GetPhaseDuration(CurrentPhase);

    if (SequenceTimer >= phaseDuration)
    {
        // 다음 페이즈로 전환
        switch (CurrentPhase)
        {
        case EFireDispatchPhase::Run:
            TransitionToPhase(EFireDispatchPhase::Fall);
            break;
        case EFireDispatchPhase::Fall:
            TransitionToPhase(EFireDispatchPhase::GetUp);
            break;
        case EFireDispatchPhase::GetUp:
            TransitionToPhase(EFireDispatchPhase::Walk);
            break;
        case EFireDispatchPhase::Walk:
            TransitionToPhase(EFireDispatchPhase::Board);
            break;
        case EFireDispatchPhase::Board:
            TransitionToPhase(EFireDispatchPhase::Drive);
            break;
        case EFireDispatchPhase::Drive:
            // 페이드아웃 시작
            if (CameraManager)
            {
                CameraManager->FadeOut(FadeoutDuration, FLinearColor(0, 0, 0, 1), 200);
            }
            TransitionToPhase(EFireDispatchPhase::Complete);
            break;
        case EFireDispatchPhase::Complete:
            // 레벨 전환
            if (GetWorld())
            {
                if (ALevelTransitionManager* Manager = GetWorld()->FindActor<ALevelTransitionManager>())
                {
                    Manager->TransitionToNextLevel();
                }
            }
            break;
        }
    }
}

void AFireDispatchGameMode::TransitionToPhase(EFireDispatchPhase NewPhase)
{
    CurrentPhase = NewPhase;
    SequenceTimer = 0.0f;

    UGameInstance* GI = GetWorld()->GetGameInstance();
    if (!GI) { return; }

    // GameInstance에 플래그 설정 (Lua 스크립트가 읽음)
    switch (CurrentPhase)
    {
    case EFireDispatchPhase::Run:
        GI->SetBool("FireDispatch_Run", true);
        GI->SetFloat("FireDispatch_RunSpeed", RunSpeed);
        GI->SetBool("Vehicle_StartMove", true);  // 차량 이동 시작
        break;

    case EFireDispatchPhase::Fall:
        GI->SetBool("FireDispatch_Fall", true);
        GI->SetFloat("FireDispatch_ImpulseX", FallImpulseX);
        GI->SetFloat("FireDispatch_ImpulseZ", FallImpulseZ);
        // 사운드 위치 하드 코딩
        FAudioDevice::PlaySound3D(ScreamSound, FVector(-45.f,34.f,-19.f), 1.0f, false);
        FAudioDevice::PlaySound3D(CarCrashSound, FVector(-45.f, 34.f, -19.f), 1.0f, false);
        // 충돌 카메라 셰이크
        if (CameraManager)
        {
            CameraManager->StartCameraShake(
                FallShakeDuration,
                FallShakeIntensity,
                FallShakeIntensity,
                FallShakeFrequency,
                100
            );
        }
        // 충돌 진동
        bVibrating = true;
        VibrationTimer = 0.0f;
        VibrationDuration = FallVibrationDuration;
        UInputManager::GetInstance().SetVibration(FallVibrationIntensity, FallVibrationIntensity);
        break;

    case EFireDispatchPhase::GetUp:
        GI->SetBool("FireDispatch_GetUp", true);
        // Ouch 소리 딜레이 시작
        bWaitingForOuchSound = true;
        OuchSoundDelayTimer = 0.0f;
        break;

    case EFireDispatchPhase::Walk:
        GI->SetBool("FireDispatch_Walk", true);
        GI->SetFloat("FireDispatch_WalkSpeed", WalkSpeed);
        break;

    case EFireDispatchPhase::Board:
        GI->SetBool("FireDispatch_Board", true);

        // 문 여는 소리 지연 시작
        bWaitingForDoorSound = true;
        DoorSoundDelayTimer = 0.0f;
        break;

    case EFireDispatchPhase::Drive:
        GI->SetBool("FireDispatch_Drive", true);

        // 카메라 전환 시작
        StartCameraTransition();

        // 카메라 쉐이크 지연 시작 (트랜지션 완료 후 실행)
        bWaitingForShake = true;
        ShakeDelayTimer = 0.0f;

        // 차량 사운드 재생
        if (CarSound)
        {
            // 위치 하드 코딩
            FAudioDevice::PlaySound3D(CarSound, FVector(-48.5, 36, -19), 1.0f, false);
        }

        // 시동 진동
        bVibrating = true;
        VibrationTimer = 0.0f;
        VibrationDuration = EngineVibrationDuration;
        UInputManager::GetInstance().SetVibration(EngineVibrationIntensity, EngineVibrationIntensity);
        break;

    case EFireDispatchPhase::Complete:
        if (PassSound)
        {
            // 위치 하드 코딩
            FAudioDevice::PlaySound3D(PassSound, FVector(-48.5, 36, -19), 1.0f, false);
        }
        break;
    }
}

float AFireDispatchGameMode::GetPhaseDuration(EFireDispatchPhase Phase) const
{
    switch (Phase)
    {
    case EFireDispatchPhase::Run:    return RunDuration;
    case EFireDispatchPhase::Fall:   return FallDuration;
    case EFireDispatchPhase::GetUp:  return GetUpDuration;
    case EFireDispatchPhase::Board:  return BoardDuration;
    case EFireDispatchPhase::Drive:  return DriveDuration;
    case EFireDispatchPhase::Complete: return FadeoutDuration;
    default: return 1.0f;
    }
}

void AFireDispatchGameMode::StartCameraTransition()
{
    if (!MainCamera) { return; }

    // 현재 카메라 위치/회전 저장
    CameraStartPosition = MainCamera->GetActorLocation();
    CameraStartRotation = FQuat(MainCamera->GetActorRotation());

    // 전환 시작
    bCameraTransitioning = true;
    CameraTransitionTimer = 0.0f;
}

void AFireDispatchGameMode::UpdateCameraTransition(float DeltaTime)
{
    if (!bCameraTransitioning || !MainCamera) { return; }

    CameraTransitionTimer += DeltaTime;
    float t = CameraTransitionTimer / CameraBlendTime;

    if (t >= 1.0f)
    {
        // 전환 완료
        MainCamera->SetActorLocation(CameraTargetPosition);
        // 회전 적용 (Quaternion 생성: Roll, Pitch, Yaw 순서)
        FQuat TargetQuat = FQuat::MakeFromEulerZYX(CameraTargetRotation);
        MainCamera->SetActorRotation(TargetQuat);
        bCameraTransitioning = false;
    }
    else
    {
        // 선형 보간
        FVector NewPosition = FMath::Lerp(CameraStartPosition, CameraTargetPosition, t);
        FQuat TargetQuat = FQuat::MakeFromEulerZYX(CameraTargetRotation);

        // Quaternion 수동 Lerp
        FQuat NewRotation;
        NewRotation.X = CameraStartRotation.X + (TargetQuat.X - CameraStartRotation.X) * t;
        NewRotation.Y = CameraStartRotation.Y + (TargetQuat.Y - CameraStartRotation.Y) * t;
        NewRotation.Z = CameraStartRotation.Z + (TargetQuat.Z - CameraStartRotation.Z) * t;
        NewRotation.W = CameraStartRotation.W + (TargetQuat.W - CameraStartRotation.W) * t;
        NewRotation.Normalize();

        MainCamera->SetActorLocation(NewPosition);
        MainCamera->SetActorRotation(NewRotation);

        // 진행 상황 로그 (0.5초마다)
        static float LogTimer = 0.0f;
        LogTimer += DeltaTime;
        if (LogTimer >= 0.5f)
        {
            LogTimer = 0.0f;
        }
    }
}

void AFireDispatchGameMode::UpdateCameraShakeDelay(float DeltaTime)
{
    if (!bWaitingForShake) { return; }

    ShakeDelayTimer += DeltaTime;

    if (ShakeDelayTimer >= ShakeDelay && ShakeDelayTimer < ShakeDelay + DeltaTime)
    {
        // 지연 시간 경과 - 쉐이크 시작 (한 번만 실행)
        if (CameraManager)
        {
            CameraManager->StartCameraShake(
                ShakeDuration,      // Duration (1.5초)
                ShakeIntensity,     // AmplitudeLocation (5.0cm - 강렬한 진동)
                ShakeIntensity,     // AmplitudeRotation (5.0도 - 강렬한 회전)
                ShakeFrequency,     // Frequency (25Hz - 빠른 진동)
                100                 // Priority
            );
        }
    }

    if (ShakeDelayTimer >= ShakeDelay + ShakeDuration)
    {
        // 쉐이크 완료 - 시동 완료 플래그 설정 및 대기 종료
        bWaitingForShake = false;

        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            GI->SetBool("FireDispatch_EngineStarted", true);
        }
    }
}

void AFireDispatchGameMode::UpdateDoorSoundDelay(float DeltaTime)
{
    if (!bWaitingForDoorSound) { return; }

    DoorSoundDelayTimer += DeltaTime;

    if (DoorSoundDelayTimer >= DoorSoundDelay)
    {
        // 지연 시간 경과 - 문 여는 소리 재생
        bWaitingForDoorSound = false;

        if (OpenSound)
        {
            FAudioDevice::PlaySound3D(OpenSound, FVector(0, 0, 0), 1.0f, false);
            UE_LOG("[FireDispatchGameMode] Playing door open sound");
        }
    }
}

void AFireDispatchGameMode::UpdateOuchSoundDelay(float DeltaTime)
{
    if (!bWaitingForOuchSound) { return; }

    OuchSoundDelayTimer += DeltaTime;

    if (OuchSoundDelayTimer >= OuchSoundDelay)
    {
        // 지연 시간 경과 - Ouch 소리 재생
        bWaitingForOuchSound = false;

        if (OuchSound)
        {
            FAudioDevice::PlaySound3D(OuchSound, FVector(-45.f, 34.f, -19.f), 1.0f, false);
        }
    }
}

void AFireDispatchGameMode::UpdateVibration(float DeltaTime)
{
    if (!bVibrating) { return; }

    VibrationTimer += DeltaTime;

    if (VibrationTimer >= VibrationDuration)
    {
        bVibrating = false;
        UInputManager::GetInstance().StopVibration();
    }
}
