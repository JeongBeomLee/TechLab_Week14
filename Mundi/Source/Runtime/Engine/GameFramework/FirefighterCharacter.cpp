#include "pch.h"
#include "FirefighterCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SpringArmComponent.h"
#include "CameraComponent.h"
#include "CapsuleComponent.h"
#include "SphereComponent.h"
#include "AudioComponent.h"
#include "FAudioDevice.h"
#include "PlayerController.h"
#include "Controller.h"
#include "InputComponent.h"
#include "InputManager.h"
#include "LuaScriptComponent.h"
#include "CharacterMovementComponent.h"
#include "ParticleSystemComponent.h"
#include "ParticleSystem.h"
#include "ResourceManager.h"
#include "World.h"
#include "PhysScene.h"
#include "FireActor.h"
#include "EPhysicsMode.h"
#include "PhysicsAsset.h"
#include "BoneSocketComponent.h"
#include "SkeletalMesh.h"
#include "GameObject.h"
#include "../Audio/Sound.h"
#include "BodyInstance.h"
#include "PhysXPublic.h"
#include "ItemComponent.h"
#include "RescueGameMode.h"
#include "../Audio/Sound.h"
#include "StaticMeshComponent.h"

AFirefighterCharacter::AFirefighterCharacter()
	: bOrientRotationToMovement(true)
	, RotationRate(540.0f)
	, CurrentMovementDirection(FVector::Zero())
	, ItemDetectionSphere(nullptr)
	, ItemPickupScript(nullptr)
	, WaterMagicParticle(nullptr)
{
	// 캡슐 크기 설정 (기본 스케일 1.0 기준)
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCapsuleSize(0.33f, 1.0f);
	}

	// SkeletalMeshComponent 생성 (애니메이션)
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("MeshComponent");
	if (MeshComponent && CapsuleComponent)
	{
		MeshComponent->SetupAttachment(CapsuleComponent);
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -1.05f));
		MeshComponent->SetSkeletalMesh(GDataDir + "/firefighter/Firefighter_Without_Cloth.fbx");

		// PhysicsAsset 설정 (랙돌용)
		UPhysicsAsset* PhysAsset = UResourceManager::GetInstance().Load<UPhysicsAsset>("Data/Physics/firefighter_nocloth.physicsasset");
		if (PhysAsset)
		{
			MeshComponent->SetPhysicsAsset(PhysAsset);
		}
	}

	// LuaScriptComponent 생성 (애니메이션 제어용)
	LuaScript = CreateDefaultSubobject<ULuaScriptComponent>("LuaScript");
	if (LuaScript)
	{
		LuaScript->ScriptFilePath = "Data/Scripts/FirefighterController.lua";
	}

	// SpringArmComponent 생성 (3인칭 카메라용)
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	if (SpringArmComponent && CapsuleComponent)
	{
		SpringArmComponent->SetupAttachment(CapsuleComponent);
		SpringArmComponent->SetTargetArmLength(10.0f);
		SpringArmComponent->SetSocketOffset(FVector(0.0f, 0.0f, 0.0f));
		SpringArmComponent->SetEnableCameraLag(true);
		SpringArmComponent->SetCameraLagSpeed(10.0f);
		SpringArmComponent->SetUsePawnControlRotation(true);  // Controller 회전 사용
	}

	// CameraComponent 생성 (SpringArm 끝에 부착)
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	if (CameraComponent && SpringArmComponent)
	{
		CameraComponent->SetupAttachment(SpringArmComponent);
	}

	// ItemDetectionSphere 생성 (아이템 감지용)
	ItemDetectionSphere = CreateDefaultSubobject<USphereComponent>("ItemDetectionSphere");
	if (ItemDetectionSphere && CapsuleComponent)
	{
		ItemDetectionSphere->SetupAttachment(CapsuleComponent);
		ItemDetectionSphere->SetSphereRadius(2.0f);  // 감지 반경 2m
		ItemDetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // 오버랩만 감지, 물리 충돌 X
		ItemDetectionSphere->SetSimulatePhysics(false);
		ItemDetectionSphere->SetGenerateOverlapEvents(true);
		// Pawn 제외, WorldStatic/WorldDynamic만 오버랩 감지 (아이템은 WorldDynamic)
		ItemDetectionSphere->CollisionMask = CollisionMasks::WorldStatic | CollisionMasks::WorldDynamic;
	}

	// ItemPickupScript 생성 (아이템 줍기 로직)
	ItemPickupScript = CreateDefaultSubobject<ULuaScriptComponent>("ItemPickupScript");
	if (ItemPickupScript)
	{
		ItemPickupScript->ScriptFilePath = "Data/Scripts/ItemPickupManager.lua";
	}

	// 소방복 장착 파티클 컴포넌트 생성
	FireSuitEquipParticle = CreateDefaultSubobject<UParticleSystemComponent>("FireSuitEquipParticle");
	if (FireSuitEquipParticle && CapsuleComponent)
	{
		FireSuitEquipParticle->SetupAttachment(CapsuleComponent);
		FireSuitEquipParticle->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));  // 발밑
		FireSuitEquipParticle->bAutoActivate = false;  // 수동으로 활성화

		// 파티클 시스템 로드
		UParticleSystem* EquipEffect = UResourceManager::GetInstance().Load<UParticleSystem>("Data/Particles/FireSuitEquip.particle");
		if (EquipEffect)
		{
			FireSuitEquipParticle->SetTemplate(EquipEffect);
		}
	}

	// 물 마법 파티클 컴포넌트 생성
	WaterMagicParticle = CreateDefaultSubobject<UParticleSystemComponent>("WaterMagicParticle");
	if (WaterMagicParticle && MeshComponent)
	{
		WaterMagicParticle->SetupAttachment(MeshComponent);
		WaterMagicParticle->SetRelativeLocation(FVector(0.9f, 0.2f, 1.2f));  // 캐릭터 앞쪽 손 위치 근처
		WaterMagicParticle->bAutoActivate = false;  // 수동으로 활성화

		// 파티클 시스템 로드
		UParticleSystem* WaterEffect = UResourceManager::GetInstance().Load<UParticleSystem>("Data/Particles/WaterMagic.particle");
		if (WaterEffect)
		{
			WaterMagicParticle->SetTemplate(WaterEffect);
		}
	}

	// 발먼지 파티클 로드 (공용)
	UParticleSystem* FootDustEffect = UResourceManager::GetInstance().Load<UParticleSystem>("Data/Particles/FootDust.particle");

	// 왼발 먼지 파티클 컴포넌트 생성
	LeftFootDustParticle = CreateDefaultSubobject<UParticleSystemComponent>("LeftFootDustParticle");
	if (LeftFootDustParticle && CapsuleComponent)
	{
		LeftFootDustParticle->SetupAttachment(CapsuleComponent);
		LeftFootDustParticle->bAutoActivate = false;
		if (FootDustEffect)
		{
			LeftFootDustParticle->SetTemplate(FootDustEffect);
		}
	}

	// 오른발 먼지 파티클 컴포넌트 생성
	RightFootDustParticle = CreateDefaultSubobject<UParticleSystemComponent>("RightFootDustParticle");
	if (RightFootDustParticle && CapsuleComponent)
	{
		RightFootDustParticle->SetupAttachment(CapsuleComponent);
		RightFootDustParticle->bAutoActivate = false;
		if (FootDustEffect)
		{
			RightFootDustParticle->SetTemplate(FootDustEffect);
		}
	}

	// 발소리 사운드 로드
	FootstepSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/FirefighterStep.wav");

	// 물 마법 사운드 로드
	WaterStartSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/water_start.wav");
	WaterLoopSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/water_ing.wav");
	WaterEndSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/water_end.wav");
	ShoutWaterSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/ShoutWater.wav");
	
	// 아이템 획득 사운드 로드
	ItemPickupSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/AcquisitionSound.wav");

	// 구출 사운드 로드 (남성/여성)
	ManLetsGoSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/ManLetsGo.wav");
	WomanLetsGoSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/WomanLetsGo.wav");

	// 구출 완료 사운드 로드 (남성/여성)
	ManThankYouSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/ManThankYou.wav");
	WomanThankYouSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/WomanThankYou.wav");

	// 피격 사운드 로드
	DamagedSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/Damaged.wav");
	DeathScreamSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/CutScene/scream.wav");

	// 아이템 픽업 파티클 컴포넌트 생성
	ItemPickupParticle = CreateDefaultSubobject<UParticleSystemComponent>("ItemPickupParticle");
	if (ItemPickupParticle)
	{
		// 루트에 붙이지 않고 월드 위치로 직접 설정할 것임
		ItemPickupParticle->bAutoActivate = false;
		UParticleSystem* PickupEffect = UResourceManager::GetInstance().Load<UParticleSystem>("Data/Particles/ItemPickup.particle");
		if (PickupEffect)
		{
			ItemPickupParticle->SetTemplate(PickupEffect);
		}
	}

	// 왼손 본 소켓 컴포넌트 생성 (사람 들기용 - Neck 부착)
	LeftHandSocket = CreateDefaultSubobject<UBoneSocketComponent>("LeftHandSocket");
	if (LeftHandSocket && MeshComponent)
	{
		LeftHandSocket->SetupAttachment(MeshComponent);
		LeftHandSocket->BoneName = "mixamorig:LeftHand";
		// 손에서 앞쪽(X)으로 오프셋, Person이 눕혀지도록 회전 (Pitch -90도)
		LeftHandSocket->SocketOffsetLocation = FVector(0.0f, 0.0f, -0.2f);
	}

	// 오른손 본 소켓 컴포넌트 생성 (사람 들기용 - Hips 부착)
	RightHandSocket = CreateDefaultSubobject<UBoneSocketComponent>("RightHandSocket");
	if (RightHandSocket && MeshComponent)
	{
		RightHandSocket->SetupAttachment(MeshComponent);
		RightHandSocket->BoneName = "mixamorig:RightHand";
		// 손에서 앞쪽(X)으로 오프셋 + 위(Z)로 올려서 손 관통 방지
		RightHandSocket->SocketOffsetLocation = FVector(0.5f, 0.0f, 0.0f);
	}

	// 충돌 파티클 컴포넌트 생성 (1~4)
	const char* CollisionParticlePaths[4] = {
		"Data/Particles/CollisionBoom1.particle",
		"Data/Particles/CollisionBoom2.particle",
		"Data/Particles/CollisionBoom3.particle",
		"Data/Particles/CollisionBoom4.particle"
	};

	const char* CollisionParticleNames[4] = {
		"CollisionBoomParticle1",
		"CollisionBoomParticle2",
		"CollisionBoomParticle3",
		"CollisionBoomParticle4"
	};

	for (int32 i = 0; i < 4; ++i)
	{
		CollisionBoomParticles[i] = CreateDefaultSubobject<UParticleSystemComponent>(CollisionParticleNames[i]);
		if (CollisionBoomParticles[i])
		{
			CollisionBoomParticles[i]->bAutoActivate = false;
			UParticleSystem* Effect = UResourceManager::GetInstance().Load<UParticleSystem>(CollisionParticlePaths[i]);
			if (Effect)
			{
				CollisionBoomParticles[i]->SetTemplate(Effect);
			}
		}
	}

	// 충돌 사운드 로드
	GlassBreakSounds[0] = UResourceManager::GetInstance().Load<USound>("Data/Audio/GlassBreak1.wav");
	GlassBreakSounds[1] = UResourceManager::GetInstance().Load<USound>("Data/Audio/GlassBreak2.wav");
}

AFirefighterCharacter::~AFirefighterCharacter()
{
	if (WaterLoopVoice)
	{
		FAudioDevice::StopSound(WaterLoopVoice);
		WaterLoopVoice = nullptr;
	}
}

void AFirefighterCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 애니메이션은 LuaScriptComponent(FirefighterController.lua)에서 처리

	// BoneSocketComponent에 명시적으로 TargetMesh 설정 (BeginPlay 후 본 검색)
	if (LeftHandSocket && MeshComponent)
	{
		LeftHandSocket->SetTargetByName(MeshComponent, "mixamorig:LeftHand");
		UE_LOG("[FirefighterCharacter] LeftHandSocket TargetMesh set, BoneIndex=%d", LeftHandSocket->BoneIndex);
	}
	if (RightHandSocket && MeshComponent)
	{
		RightHandSocket->SetTargetByName(MeshComponent, "mixamorig:RightHand");
		UE_LOG("[FirefighterCharacter] RightHandSocket TargetMesh set, BoneIndex=%d", RightHandSocket->BoneIndex);
	}

	// CapsuleComponent 충돌 이벤트 바인딩
	if (CapsuleComponent)
	{
		CapsuleComponent->OnComponentHit.AddDynamic(this, &AFirefighterCharacter::HandleCapsuleHit);
	}
}

void AFirefighterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 게임 전용 입력 모드 설정 (커서 숨김, 마우스 잠금, 항상 카메라 회전)
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		PC->SetInputMode(EInputMode::GameOnly);
	}
}

void AFirefighterCharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
	Super::HandleAnimNotify(NotifyEvent);

	FString NotifyName = NotifyEvent.NotifyName.ToString();

	// 발 착지 노티파이 처리 - 발먼지 파티클 재생
	if (NotifyName == "LeftFootLanding")
	{
		PlayFootDustEffect(true);
	}
	else if (NotifyName == "RightFootLanding")
	{
		PlayFootDustEffect(false);
	}

	// Lua 스크립트에 AnimNotify 전달 (애니메이션 제어용)
	if (LuaScript)
	{
		LuaScript->OnAnimNotify(NotifyName);
	}

	// ItemPickupScript에도 AnimNotify 전달 (아이템 줍기용)
	if (ItemPickupScript)
	{
		ItemPickupScript->OnAnimNotify(NotifyName);
	}
}

void AFirefighterCharacter::Tick(float DeltaSeconds)
{
	// 게임패드 입력 처리 (좌 스틱 이동, A 버튼 점프)
	ProcessGamepadInput();

	// 이동 방향으로 캐릭터 회전 (Super::Tick 전에 처리해야 SpringArm이 올바른 회전을 사용)
	// MaxWalkSpeed가 0이면 (PickUp 등) 회전도 비활성화, 단 물 마법 사용 중에는 회전 허용
	const bool bCanRotate = CharacterMovement && (CharacterMovement->MaxWalkSpeed > 0.01f || bIsUsingWaterMagic);
	if (bOrientRotationToMovement && bCanRotate && CurrentMovementDirection.SizeSquared() > 0.01f)
	{
		// 이동 방향에서 목표 회전 계산 (Yaw만 사용)
		FVector Direction = CurrentMovementDirection.GetNormalized();
		float TargetYaw = std::atan2(Direction.Y, Direction.X);  // 라디안

		// 현재 Actor 회전에서 Yaw 추출
		FVector CurrentEuler = GetActorRotation().ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees
		float CurrentYaw = CurrentEuler.Z * (PI / 180.0f);  // 라디안으로 변환

		// 각도 차이 계산 (최단 경로)
		float DeltaYaw = TargetYaw - CurrentYaw;

		// -PI ~ PI 범위로 정규화
		while (DeltaYaw > PI) DeltaYaw -= 2.0f * PI;
		while (DeltaYaw < -PI) DeltaYaw += 2.0f * PI;

		// 회전 속도 제한 적용
		float MaxDelta = RotationRate * (PI / 180.0f) * DeltaSeconds;  // 라디안/초
		DeltaYaw = FMath::Clamp(DeltaYaw, -MaxDelta, MaxDelta);

		// 새로운 Yaw 계산
		float NewYaw = CurrentYaw + DeltaYaw;

		// Actor 회전 설정 (Yaw만 변경)
		FQuat NewRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, NewYaw * (180.0f / PI)));
		SetActorRotation(NewRotation);
	}

	// 이동 방향 초기화 (다음 프레임을 위해)
	CurrentMovementDirection = FVector::Zero();

	// Super::Tick에서 Component들의 Tick이 호출됨 (SpringArm 포함)
	Super::Tick(DeltaSeconds);

	// 물 루프 사운드 위치 업데이트
	if (WaterLoopVoice)
	{
		FAudioDevice::UpdateSoundPosition(WaterLoopVoice, GetWaterEmitterLocation());
	}

	// 데미지 쿨타임 타이머 업데이트
	if (DamageCooldownTimer > 0.0f)
	{
		DamageCooldownTimer -= DeltaSeconds;
	}

	// 충돌 이펙트 쿨타임 타이머 업데이트
	if (CollisionEffectCooldownTimer > 0.0f)
	{
		CollisionEffectCooldownTimer -= DeltaSeconds;
	}

	// 물 발사 중 진동 업데이트
	if (WaterLoopVoice)
	{
		WaterVibrationTimer += DeltaSeconds;
		if (WaterVibrationTimer >= WaterVibrationInterval)
		{
			WaterVibrationTimer = 0.0f;
			UInputManager::GetInstance().SetVibration(WaterVibrationIntensity, WaterVibrationIntensity);
		}
	}

	// 아이템 수집 진동 업데이트
	if (bItemVibrating)
	{
		ItemVibrationTimer += DeltaSeconds;
		if (ItemVibrationTimer >= ItemVibrationDuration)
		{
			bItemVibrating = false;
			ItemVibrationTimer = 0.0f;
			UInputManager::GetInstance().StopVibration();
		}
	}

	// 피격 진동 업데이트
	if (bDamageVibrating)
	{
		DamageVibrationTimer += DeltaSeconds;
		if (DamageVibrationTimer >= DamageVibrationDuration)
		{
			bDamageVibrating = false;
			DamageVibrationTimer = 0.0f;
			UInputManager::GetInstance().StopVibration();
		}
	}

	// 피격 플래시 효과 업데이트
	UpdateHitFlash(DeltaSeconds);

	// 사람을 들고 있을 때 상체 본들 위치 업데이트
	UpdateCarriedPersonPose();
}

void AFirefighterCharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	if (!InInputComponent)
	{
		return;
	}

	// WASD 이동 바인딩 (카메라 방향 기준)
	InInputComponent->BindAxis("MoveForward", 'W', 1.0f, this, &AFirefighterCharacter::MoveForwardCamera);
	InInputComponent->BindAxis("MoveForward", 'S', -1.0f, this, &AFirefighterCharacter::MoveForwardCamera);
	InInputComponent->BindAxis("MoveRight", 'D', 1.0f, this, &AFirefighterCharacter::MoveRightCamera);
	InInputComponent->BindAxis("MoveRight", 'A', -1.0f, this, &AFirefighterCharacter::MoveRightCamera);

	// 점프 바인딩 (부모 클래스 함수 사용)
	InInputComponent->BindAction("Jump", VK_SPACE, Cast<ACharacter>(this), &ACharacter::Jump, &ACharacter::StopJumping);
}

void AFirefighterCharacter::MoveForwardCamera(float Value)
{
	if (Value == 0.0f)
	{
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	// Controller의 Yaw만 사용하여 Forward 방향 계산
	FQuat ControlRot = MyController->GetControlRotation();
	FVector ControlEuler = ControlRot.ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees

	// Yaw만으로 회전 생성
	FQuat YawOnlyRot = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, ControlEuler.Z));
	FVector Forward = YawOnlyRot.GetForwardVector();

	// 이동 방향 누적 (회전 계산용)
	CurrentMovementDirection += Forward * Value;

	// 이동 입력 추가
	AddMovementInput(Forward, Value);
}

void AFirefighterCharacter::MoveRightCamera(float Value)
{
	if (Value == 0.0f)
	{
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	// Controller의 Yaw만 사용하여 Right 방향 계산
	FQuat ControlRot = MyController->GetControlRotation();
	FVector ControlEuler = ControlRot.ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees

	// Yaw만으로 회전 생성
	FQuat YawOnlyRot = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, ControlEuler.Z));
	FVector Right = YawOnlyRot.GetRightVector();

	// 이동 방향 누적 (회전 계산용)
	CurrentMovementDirection += Right * Value;

	// 이동 입력 추가
	AddMovementInput(Right, Value);
}

// ────────────────────────────────────────────────────────────────────────────
// 스케일 설정
// ────────────────────────────────────────────────────────────────────────────

void AFirefighterCharacter::SetCharacterScale(float Scale)
{
	CharacterScale = Scale;

	// 기본 수치 (스케일 1.0 기준)
	constexpr float BaseCapsuleRadius = 0.33f;
	constexpr float BaseCapsuleHalfHeight = 1.0f;
	constexpr float BaseMeshOffsetZ = -1.05f;
	constexpr float BaseSpringArmLength = 10.0f;
	constexpr float BaseItemDetectionRadius = 2.0f;

	// 1. CapsuleComponent 크기 설정 (PhysX CCT용)
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCapsuleSize(
			BaseCapsuleRadius * Scale,
			BaseCapsuleHalfHeight * Scale
		);
	}

	// 2. MeshComponent 스케일 및 오프셋 설정
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale(FVector(Scale, Scale, Scale));
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BaseMeshOffsetZ * Scale));
	}

	// 3. SpringArmComponent 길이 설정
	if (SpringArmComponent)
	{
		SpringArmComponent->SetTargetArmLength(BaseSpringArmLength * Scale);
	}

	// 4. ItemDetectionSphere 반경 설정
	if (ItemDetectionSphere)
	{
		ItemDetectionSphere->SetSphereRadius(BaseItemDetectionRadius * Scale);
	}

	UE_LOG("[info] FirefighterCharacter::SetCharacterScale - Scale set to %.2f", Scale);
}

void AFirefighterCharacter::PlayFireSuitEquipEffect()
{
	if (FireSuitEquipParticle)
	{
		FireSuitEquipParticle->ResetParticles();
		FireSuitEquipParticle->ActivateSystem();
	}
}

void AFirefighterCharacter::PlayWaterMagicEffect()
{
	if (WaterMagicParticle)
	{
		WaterMagicParticle->ResetParticles();
		WaterMagicParticle->ActivateSystem();
	}

	// 시작/루프 사운드 재생
	FVector EmitterLocation = GetWaterEmitterLocation();

	if (WaterStartSound)
	{
		FAudioDevice::PlaySound3D(WaterStartSound, EmitterLocation, 0.35f, false);
	}
	if (ShoutWaterSound)
	{
		FAudioDevice::PlaySound3D(ShoutWaterSound, EmitterLocation, 1.0f, false);
	}

	if (WaterLoopSound && !WaterLoopVoice)
	{
		WaterLoopVoice = FAudioDevice::PlaySound3D(WaterLoopSound, EmitterLocation, 0.35f, true);
	}

	// 진동 시작
	WaterVibrationTimer = WaterVibrationInterval; // 즉시 진동 트리거
}

void AFirefighterCharacter::StopWaterMagicEffect()
{
	if (WaterMagicParticle)
	{
		WaterMagicParticle->DeactivateSystem();
	}

	// 루프 사운드 정지 및 종료 사운드 재생
	if (WaterLoopVoice)
	{
		FAudioDevice::StopSound(WaterLoopVoice);
		WaterLoopVoice = nullptr;
	}

	// 진동 정지
	UInputManager::GetInstance().StopVibration();

	if (WaterEndSound)
	{
		FAudioDevice::PlaySound3D(WaterEndSound, GetWaterEmitterLocation(), 0.35f, false);
	}
}

void AFirefighterCharacter::PlayItemPickupEffect(const FVector& Position)
{
	if (!ItemPickupParticle)
	{
		return;
	}

	// 지정된 위치에 파티클 배치
	ItemPickupParticle->SetWorldLocation(Position);
	ItemPickupParticle->ResetParticles();
	ItemPickupParticle->ActivateSystem();

	// 아이템 획득 사운드 재생
	if (ItemPickupSound)
	{
		FAudioDevice::PlaySound3D(ItemPickupSound, Position, 1.0f, false);
	}

	// 아이템 수집 진동
	bItemVibrating = true;
	ItemVibrationTimer = 0.0f;
	UInputManager::GetInstance().SetVibration(ItemVibrationIntensity, ItemVibrationIntensity);
}

void AFirefighterCharacter::PlayFootDustEffect(bool bLeftFoot)
{
	UParticleSystemComponent* DustParticle = bLeftFoot ? LeftFootDustParticle : RightFootDustParticle;
	if (!DustParticle || !CapsuleComponent)
	{
		return;
	}

	// 캡슐 바닥 위치 계산 (액터 위치 - 캡슐 반높이)
	FVector ActorPos = GetActorLocation();
	float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	FVector BaseFootPos = ActorPos - FVector(0.0f, 0.0f, CapsuleHalfHeight);

	// 캐릭터 방향 기준 좌/우 오프셋 계산
	FQuat ActorRot = GetActorRotation();
	FVector RightDir = ActorRot.RotateVector(FVector(0.0f, 1.0f, 0.0f));  // 캐릭터의 오른쪽 방향

	// 왼발은 왼쪽으로, 오른발은 오른쪽으로 오프셋 (스케일 적용)
	float FootOffset = 0.15f * CharacterScale;  // 발 간격
	FVector FootPosition = BaseFootPos + RightDir * (bLeftFoot ? -FootOffset : FootOffset);

	// 월드 좌표로 직접 설정
	DustParticle->SetWorldLocation(FootPosition);

	// 파티클 재생
	DustParticle->ResetParticles();
	DustParticle->ActivateSystem();

	// 발소리 재생
	PlayFootstepSound(FootPosition);
}

void AFirefighterCharacter::PlayFootstepSound(const FVector& FootPosition)
{
	if (!FootstepSound)
	{
		return;
	}

	// 3D 공간 사운드로 발소리 재생
	FAudioDevice::PlaySound3D(FootstepSound, FootPosition, 0.5f, false);
}

FVector AFirefighterCharacter::GetWaterEmitterLocation() const
{
	if (WaterMagicParticle)
	{
		return WaterMagicParticle->GetWorldLocation();
	}

	return GetActorLocation();
}

void AFirefighterCharacter::DrainExtinguishGauge(float Amount)
{
	ExtinguishGauge = FMath::Max(0.0f, ExtinguishGauge - Amount);
}

void AFirefighterCharacter::ChargeExtinguishGauge(float Amount)
{
	ExtinguishGauge = FMath::Min(MaxExtinguishGauge, ExtinguishGauge + Amount);
}

void AFirefighterCharacter::FireWaterMagic(float DamageAmount)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	// 캐릭터 전방 방향으로 Sphere Sweep (물줄기 굵기 표현)
	FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 0.0f);  // 약간 위에서 발사
	FVector Direction = GetActorForward();
	FVector End = Start + Direction * WaterMagicRange;

	FHitResult HitResult;
	float SweepRadius = 0.75f;  // 물줄기 반경

	// Sphere Sweep 수행
	bool bHit = PhysScene->SweepSingleSphere(Start, End, SweepRadius, HitResult, this);

	if (bHit && HitResult.Actor.IsValid())
	{
		// 불 액터에 맞았는지 확인
		AFireActor* FireActor = Cast<AFireActor>(HitResult.Actor.Get());
		if (FireActor && FireActor->IsFireActive())
		{
			// 물 데미지 적용
			FireActor->ApplyWaterDamage(DamageAmount);
			UE_LOG("FireActor->ApplyWaterDamage");
		}
	}
}

void AFirefighterCharacter::TakeDamage(float DamageAmount)
{
	// 이미 죽었거나 쿨타임 중이면 무시
	if (bIsDead || DamageCooldownTimer > 0.0f)
	{
		return;
	}

	// 사망 여부 미리 확인
	bool bWillDie = (Health - DamageAmount) <= 0.0f;

	// 데미지 적용
	Health -= DamageAmount;
	DamageCooldownTimer = DamageCooldown;

	// 피격 플래시 효과 시작
	StartHitFlash();

	// 피격 사운드 재생 (사망 시 비명, 아니면 일반 피격음)
	if (bWillDie && DeathScreamSound)
	{
		FAudioDevice::PlaySound3D(DeathScreamSound, GetActorLocation(), 1.0f, false);
	}
	else if (DamagedSound)
	{
		FAudioDevice::PlaySound3D(DamagedSound, GetActorLocation(), 1.0f, false);
	}

	// 피격 컨트롤러 진동 (사망 시 더 긴 진동)
	UInputManager::GetInstance().SetVibration(0.6f, 0.6f);
	bDamageVibrating = true;
	DamageVibrationTimer = 0.0f;
	DamageVibrationDuration = bWillDie ? 1.0f : 0.2f;

	UE_LOG("TakeDamage: %.1f damage, Health: %.1f/%.1f", DamageAmount, Health, MaxHealth);

	// 체력이 0 이하면 사망
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		Die();
	}
}

void AFirefighterCharacter::StartHitFlash()
{
	bHitFlashActive = true;
	HitFlashTimer = 0.0f;

	// 스켈레탈 메쉬의 모든 머티리얼에 Emissive 색상 적용
	if (MeshComponent)
	{
		const TArray<UMaterialInterface*>& Materials = MeshComponent->GetMaterialSlots();
		for (uint32 i = 0; i < Materials.Num(); ++i)
		{
			// 머티리얼이 존재하는 슬롯에만 적용
			if (MeshComponent->GetMaterial(i))
			{
				MeshComponent->CreateAndSetMaterialInstanceDynamic(i);
				MeshComponent->SetMaterialColorByUser(i, "EmissiveColor", HitFlashColor);
			}
		}
	}
}

void AFirefighterCharacter::UpdateHitFlash(float DeltaTime)
{
	if (!bHitFlashActive) { return; }

	HitFlashTimer += DeltaTime;

	if (HitFlashTimer >= HitFlashDuration)
	{
		// 플래시 종료 - Emissive 색상 원래대로
		bHitFlashActive = false;
		if (MeshComponent)
		{
			const TArray<UMaterialInterface*>& Materials = MeshComponent->GetMaterialSlots();
			for (uint32 i = 0; i < Materials.Num(); ++i)
			{
				if (MeshComponent->GetMaterial(i))
				{
					MeshComponent->SetMaterialColorByUser(i, "EmissiveColor", FLinearColor(0, 0, 0, 1));
				}
			}
		}
	}
	else
	{
		// 페이드 아웃 효과
		float Alpha = 1.0f - (HitFlashTimer / HitFlashDuration);
		FLinearColor FadedColor = HitFlashColor * Alpha;
		if (MeshComponent)
		{
			const TArray<UMaterialInterface*>& Materials = MeshComponent->GetMaterialSlots();
			for (uint32 i = 0; i < Materials.Num(); ++i)
			{
				if (MeshComponent->GetMaterial(i))
				{
					MeshComponent->SetMaterialColorByUser(i, "EmissiveColor", FadedColor);
				}
			}
		}
	}
}

void AFirefighterCharacter::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	UE_LOG("Firefighter died! Activating ragdoll...");

	// 들고 있던 사람 처리 (크래시 방지를 위해 삭제 대신 숨김)
	if (bIsCarryingPerson && CarriedPerson)
	{
		UE_LOG("[FirefighterCharacter] Die: Releasing carried person");

		// 소켓에서 분리
		if (LeftHandSocket)
		{
			LeftHandSocket->DetachRagdoll();
		}
		if (RightHandSocket)
		{
			RightHandSocket->DetachRagdoll();
		}

		// 메시 숨기기
		USkeletalMeshComponent* PersonMesh = Cast<USkeletalMeshComponent>(
			CarriedPerson->GetComponent(USkeletalMeshComponent::StaticClass()));
		if (PersonMesh)
		{
			PersonMesh->SetVisibility(false);
			// 물리도 비활성화
			PersonMesh->SetPhysicsMode(EPhysicsMode::Animation);
		}

		// SpringArm 무시 목록에서 제거
		if (SpringArmComponent)
		{
			SpringArmComponent->RemoveIgnoredActor(CarriedPerson);
		}

		// 상태 초기화
		CarriedPerson = nullptr;
		bIsCarryingPerson = false;
	}

	// 물 마법 사용 중이면 강제 종료
	ForceStopWaterMagic();

	// 이동 비활성화
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed = 0.0f;
	}

	// 캡슐 충돌 비활성화 (랙돌이 땅에 끼지 않도록)
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 스켈레탈 메쉬 랙돌 활성화
	if (MeshComponent)
	{
		// 랙돌 모드로 전환 (물리가 본을 제어)
		MeshComponent->SetPhysicsMode(EPhysicsMode::Ragdoll);
	}
}

void AFirefighterCharacter::StartCarryingPerson(FGameObject* PersonGameObject)
{
	if (!PersonGameObject)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: PersonGameObject is null");
		return;
	}

	// FGameObject에서 Actor 가져오기
	AActor* PersonActor = PersonGameObject->GetOwner();
	if (!PersonActor)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: PersonActor is null");
		return;
	}

	// 이미 들고 있으면 무시
	if (bIsCarryingPerson && CarriedPerson)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: Already carrying someone");
		return;
	}

	// 액터 이름에 따라 구출 사운드 재생
	FString ActorName = PersonActor->GetName();
	UE_LOG("[FirefighterCharacter] StartCarryingPerson: Actor name = %s", ActorName.c_str());

	// 남성 이름: David, Lewis
	if (ActorName.find("David") != FString::npos || ActorName.find("Lewis") != FString::npos)
	{
		if (ManLetsGoSound)
		{
			FAudioDevice::PlaySound3D(ManLetsGoSound, GetActorLocation(), 1.0f, false);
			UE_LOG("[FirefighterCharacter] Playing ManLetsGo sound");
		}
	}
	// 여성 이름: Elizabeth, Sophie, Suzie
	else if (ActorName.find("Elizabeth") != FString::npos ||
	         ActorName.find("Sophie") != FString::npos ||
	         ActorName.find("Suzie") != FString::npos)
	{
		if (WomanLetsGoSound)
		{
			FAudioDevice::PlaySound3D(WomanLetsGoSound, GetActorLocation(), 1.0f, false);
			UE_LOG("[FirefighterCharacter] Playing WomanLetsGo sound");
		}
	}

	// Person의 SkeletalMeshComponent 가져오기
	USkeletalMeshComponent* PersonMesh = Cast<USkeletalMeshComponent>(
		PersonActor->GetComponent(USkeletalMeshComponent::StaticClass()));
	if (!PersonMesh)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: Person has no SkeletalMeshComponent");
		return;
	}

	// 본 인덱스 찾기
	USkeletalMesh* SkelMesh = PersonMesh->GetSkeletalMesh();
	if (!SkelMesh)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: Person has no SkeletalMesh");
		return;
	}

	const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
	if (!Skeleton)
	{
		UE_LOG("[FirefighterCharacter] StartCarryingPerson: Person has no Skeleton");
		return;
	}

	// 1단계: 현재 애니메이션 포즈 캐시 (래그돌 전환 전)
	int32 NumBones = Skeleton->Bones.Num();
	TArray<FTransform> CachedBonePoses;
	CachedBonePoses.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		CachedBonePoses[i] = PersonMesh->GetBoneWorldTransform(i);
	}
	UE_LOG("[FirefighterCharacter] Cached %d bone poses", NumBones);

	// 2단계: 래그돌 모드로 전환 (Constraint 스케일은 InitConstraint에서 자동 적용됨)
	PersonMesh->SetPhysicsMode(EPhysicsMode::Ragdoll);

	// 3단계: kinematic으로 설정할 본들 찾기 (상체: Hips, Spine, Neck, Head)
	TArray<int32> KinematicBones;
	const char* KinematicBoneNames[] = { "Hips", "Spine", "Spine1", "Spine2", "Neck", "Head" };
	for (const char* BoneName : KinematicBoneNames)
	{
		int32 BoneIndex = -1;
		FString FoundName;
		if (UBoneSocketComponent::FindBoneBySuffix(Skeleton, BoneName, BoneIndex, FoundName))
		{
			KinematicBones.Add(BoneIndex);
			UE_LOG("[FirefighterCharacter] Kinematic bone: %s (index: %d)", FoundName.c_str(), BoneIndex);
		}
	}

	// 4단계: 모든 바디를 kinematic으로 설정하고 포즈 복원
	const TArray<FBodyInstance*>& Bodies = PersonMesh->GetBodies();
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		FBodyInstance* Body = Bodies[i];
		if (Body && Body->IsValidBodyInstance())
		{
			Body->SetKinematic(true);

			if (Body->RigidActor)
			{
				PxRigidDynamic* DynActor = Body->RigidActor->is<PxRigidDynamic>();
				if (DynActor && i < CachedBonePoses.Num())
				{
					DynActor->setLinearVelocity(PxVec3(0, 0, 0));
					DynActor->setAngularVelocity(PxVec3(0, 0, 0));
					PxTransform PxPose = U2PTransform(CachedBonePoses[i]);
					DynActor->setGlobalPose(PxPose);
				}
			}
		}
	}

	// 5단계: 팔다리만 dynamic으로 전환 (KinematicBones에 없는 본들)
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		FBodyInstance* Body = Bodies[i];
		if (Body && Body->IsValidBodyInstance())
		{
			bool bShouldBeKinematic = KinematicBones.Contains(i);
			if (!bShouldBeKinematic)
			{
				Body->SetKinematic(false);  // 팔다리는 dynamic
			}
		}
	}

	// Head, Hips 본 인덱스 찾기
	int32 HeadBoneIndex = -1;
	FString HeadBoneName;
	UBoneSocketComponent::FindBoneBySuffix(Skeleton, "Head", HeadBoneIndex, HeadBoneName);

	int32 HipsBoneIndex = -1;
	FString HipsBoneName;
	UBoneSocketComponent::FindBoneBySuffix(Skeleton, "Hips", HipsBoneIndex, HipsBoneName);

	UE_LOG("[FirefighterCharacter] StartCarryingPerson: Head=%d, Hips=%d", HeadBoneIndex, HipsBoneIndex);

	// 6단계: 원래 척추 길이 계산 (소켓 거리 고정용)
	if (HeadBoneIndex >= 0 && HipsBoneIndex >= 0)
	{
		FTransform HipsWorldTransform = CachedBonePoses[HipsBoneIndex];
		FTransform HeadWorldTransform = CachedBonePoses[HeadBoneIndex];
		OriginalSpineLength = (HeadWorldTransform.Translation - HipsWorldTransform.Translation).Size();
		UE_LOG("[FirefighterCharacter] Original spine length: %.3f", OriginalSpineLength);
	}

	// 7단계: 소켓에 연결 (AttachRagdoll은 이미 Ragdoll 모드이므로 단순 연결만 수행)
	if (LeftHandSocket && HeadBoneIndex >= 0)
	{
		LeftHandSocket->AttachRagdoll(PersonMesh, HeadBoneIndex);
		UE_LOG("[FirefighterCharacter] Attached Head to LeftHand");
	}

	if (RightHandSocket && HipsBoneIndex >= 0)
	{
		RightHandSocket->AttachRagdoll(PersonMesh, HipsBoneIndex);
		UE_LOG("[FirefighterCharacter] Attached Hips to RightHand");
	}

	// 상태 업데이트
	CarriedPerson = PersonActor;
	bIsCarryingPerson = true;

	// SpringArm 충돌에서 무시하도록 설정 (카메라가 래그돌에 붙지 않도록)
	if (SpringArmComponent)
	{
		SpringArmComponent->AddIgnoredActor(PersonActor);
		UE_LOG("[FirefighterCharacter] Added CarriedPerson to SpringArm ignored actors");
	}

	UE_LOG("[FirefighterCharacter] StartCarryingPerson: Success!");
}

void AFirefighterCharacter::UpdateCarriedPersonPose()
{
	if (!bIsCarryingPerson || !CarriedPerson)
	{
		return;
	}

	// CarriedPerson의 SkeletalMeshComponent 가져오기
	USkeletalMeshComponent* PersonMesh = Cast<USkeletalMeshComponent>(
		CarriedPerson->GetComponent(USkeletalMeshComponent::StaticClass()));
	if (!PersonMesh)
	{
		return;
	}

	// 소켓 위치 가져오기 (GetWorldTransform 사용)
	FTransform LeftSocketWorld = LeftHandSocket ? LeftHandSocket->GetWorldTransform() : FTransform();
	FTransform RightSocketWorld = RightHandSocket ? RightHandSocket->GetWorldTransform() : FTransform();

	// 기본 소켓 위치
	FVector HipsSocketPos = RightSocketWorld.Translation;
	FVector HeadSocketPos = LeftSocketWorld.Translation;

	// 플레이어 Forward 방향으로 오프셋 추가 (충돌 방지)
	FVector ForwardOffset = GetActorForward() * 0.1f;  // 0.1m 앞으로

	// Spine 방향 계산 (두 소켓 방향 사용)
	FVector SocketDir = (HeadSocketPos - HipsSocketPos).GetNormalized();

	// Head 위치는 원래 척추 길이를 사용하여 계산 (소켓 거리 고정)
	FVector HeadPos = HipsSocketPos + ForwardOffset + SocketDir * OriginalSpineLength;

	// Hips 위치 (위로 올려서 손 관통 방지)
	FVector HipsPos = HipsSocketPos + ForwardOffset;
	HipsPos.Z += 0.2f;  // 엉덩이만 위로 올림

	FVector SpineDir = SocketDir;

	// 업힌 캐릭터가 하늘을 보도록 회전 계산
	// SpineDir을 Z축으로 하고, 업힌 캐릭터의 "앞면"이 위(하늘)를 향하게 설정
	FVector WorldUp(0, 0, 1);

	// SpineDir에 수직인 평면에 WorldUp 투영하여 X축(전방=얼굴 방향) 계산
	FVector ProjectedForward = WorldUp - SpineDir * FVector::Dot(WorldUp, SpineDir);
	ProjectedForward = ProjectedForward.GetNormalized();

	// Y축(우측)은 Z × X로 계산
	FVector RightDir = FVector::Cross(SpineDir, ProjectedForward).GetNormalized();

	// X축 재계산 (직교성 보장)
	FVector ForwardDir = FVector::Cross(RightDir, SpineDir).GetNormalized();

	// 회전 행렬에서 쿼터니언 생성
	// Forward = X축, Right = Y축, Spine = Z축
	FMatrix RotMatrix;
	RotMatrix.M[0][0] = ForwardDir.X; RotMatrix.M[0][1] = ForwardDir.Y; RotMatrix.M[0][2] = ForwardDir.Z; RotMatrix.M[0][3] = 0;
	RotMatrix.M[1][0] = RightDir.X;   RotMatrix.M[1][1] = RightDir.Y;   RotMatrix.M[1][2] = RightDir.Z;   RotMatrix.M[1][3] = 0;
	RotMatrix.M[2][0] = SpineDir.X;   RotMatrix.M[2][1] = SpineDir.Y;   RotMatrix.M[2][2] = SpineDir.Z;   RotMatrix.M[2][3] = 0;
	RotMatrix.M[3][0] = 0;            RotMatrix.M[3][1] = 0;            RotMatrix.M[3][2] = 0;            RotMatrix.M[3][3] = 1;

	FQuat SpineRotation(RotMatrix);

	// 본 인덱스 찾기
	USkeletalMesh* SkelMesh = PersonMesh->GetSkeletalMesh();
	if (!SkelMesh || !SkelMesh->GetSkeleton())
	{
		return;
	}
	const FSkeleton* Skeleton = SkelMesh->GetSkeleton();

	// kinematic 본들 위치 업데이트 (Hips(0) ~ Head(1.0))
	const char* KinematicBoneNames[] = { "Hips", "Spine", "Spine1", "Spine2", "Neck", "Head" };
	float BonePositions[] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };

	const TArray<FBodyInstance*>& Bodies = PersonMesh->GetBodies();

	for (int32 i = 0; i < 6; ++i)
	{
		int32 BoneIndex = -1;
		FString FoundName;
		if (UBoneSocketComponent::FindBoneBySuffix(Skeleton, KinematicBoneNames[i], BoneIndex, FoundName))
		{
			if (BoneIndex >= 0 && BoneIndex < Bodies.Num())
			{
				FBodyInstance* Body = Bodies[BoneIndex];
				if (Body && Body->IsValidBodyInstance())
				{
					// Hips와 Head 사이를 보간하여 위치 계산
					float T = BonePositions[i];
					FVector BonePos = FMath::Lerp(HipsPos, HeadPos, T);

					// 트랜스폼 설정
					FTransform BoneTransform(BonePos, SpineRotation, FVector(1, 1, 1));
					Body->SetKinematicTarget(BoneTransform);
				}
			}
		}
	}
}

void AFirefighterCharacter::StopCarryingPerson()
{
	if (!bIsCarryingPerson || !CarriedPerson)
	{
		return;
	}

	UE_LOG("[FirefighterCharacter] StopCarryingPerson");

	// Person의 SkeletalMeshComponent 가져오기
	USkeletalMeshComponent* PersonMesh = Cast<USkeletalMeshComponent>(
		CarriedPerson->GetComponent(USkeletalMeshComponent::StaticClass()));

	// ★ 1단계: 먼저 충돌 없는 위치 찾기 (실패하면 배치하지 않음)
	// SafeSpawner.lua 로직 참고: OverlapBox + 8꼭짓점 벽 체크 + 바닥 레이캐스트
	UWorld* World = GetWorld();
	FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;

	// 충돌 검사용 반크기 (사람 크기 대략)
	const FVector HalfExtent = FVector(0.4f, 0.4f, 0.5f);  // 80cm x 80cm x 100cm 박스
	const float StartDistance = 2.0f;  // 시작 거리 2m
	const float MaxDistance = 6.0f;  // 최대 거리 6m
	const float StepDistance = 0.5f;  // 검사 간격 0.5m

	FVector Forward = GetActorForward();
	FVector FirefighterPos = GetActorLocation();
	FVector DropPos;
	bool bFoundClearSpot = false;

	// 안전한 위치 찾기 (SafeSpawner 방식)
	if (PhysScene)
	{
		for (float Distance = StartDistance; Distance <= MaxDistance; Distance += StepDistance)
		{
			FVector TestPos = FirefighterPos + Forward * Distance;

			// 1. 바닥 찾기
			FVector RayStart = FVector(TestPos.X, TestPos.Y, TestPos.Z + 1.0f);
			FVector RayDir = FVector(0, 0, -1);
			FVector FloorHitLocation, FloorHitNormal;
			float FloorHitDistance;

			if (!PhysScene->Raycast(RayStart, RayDir, 3.0f, FloorHitLocation, FloorHitNormal, FloorHitDistance, CarriedPerson))
			{
				// 바닥 없음 - 다음 거리 시도
				UE_LOG("[FirefighterCharacter] StopCarrying: No floor at %.1fm", Distance);
				continue;
			}

			// 검사 위치 = 바닥 위 HalfExtent.Z + 여유
			FVector CheckPos = FVector(TestPos.X, TestPos.Y, FloorHitLocation.Z + HalfExtent.Z + 0.1f);

			// 2. OverlapBox로 공간 체크 (Convex 충돌체 검사)
			if (PhysScene->OverlapAnyBox(CheckPos, HalfExtent, FQuat::Identity(), CarriedPerson))
			{
				UE_LOG("[FirefighterCharacter] StopCarrying: OverlapBox collision at %.1fm", Distance);
				continue;
			}

			// 3. 8개 꼭짓점에서 중심으로 레이캐스트 (Triangle Mesh 벽 체크)
			// PhysX OverlapBox는 Triangle Mesh를 제대로 감지 못하므로 레이캐스트로 보완
			bool bWallIntersect = false;
			FVector Corners[8] = {
				CheckPos + FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
				CheckPos + FVector(-HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
				CheckPos + FVector(-HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
				CheckPos + FVector(-HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z),
				CheckPos + FVector(+HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
				CheckPos + FVector(+HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
				CheckPos + FVector(+HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
				CheckPos + FVector(+HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z)
			};

			for (int32 i = 0; i < 8; ++i)
			{
				FVector ToCenter = CheckPos - Corners[i];
				float Dist = ToCenter.Size();

				if (Dist > 0.01f)
				{
					FVector Dir = ToCenter / Dist;
					FVector WallHitLocation, WallHitNormal;
					float WallHitDistance;

					// 꼭짓점에서 중심 방향으로 레이캐스트 (중심 직전까지)
					if (PhysScene->Raycast(Corners[i], Dir, Dist - 0.01f, WallHitLocation, WallHitNormal, WallHitDistance, CarriedPerson))
					{
						// 벽이 박스를 관통함
						bWallIntersect = true;
						break;
					}
				}
			}

			if (bWallIntersect)
			{
				UE_LOG("[FirefighterCharacter] StopCarrying: Wall intersect at %.1fm", Distance);
				continue;
			}

			// 모든 조건 통과 - 이 위치 사용
			DropPos = CheckPos;
			bFoundClearSpot = true;
			UE_LOG("[FirefighterCharacter] StopCarrying: Found safe spot at distance %.1fm", Distance);
			break;
		}
	}

	// ★ 빈 공간을 찾지 못하면 배치하지 않고 리턴
	if (!bFoundClearSpot)
	{
		UE_LOG("[FirefighterCharacter] StopCarrying: FAILED - No clear spot found within %.1fm, keeping person", MaxDistance);
		return;
	}

	// ★ 2단계: 빈 공간을 찾았으므로 소켓 해제
	if (LeftHandSocket)
	{
		LeftHandSocket->DetachRagdoll();
	}
	if (RightHandSocket)
	{
		RightHandSocket->DetachRagdoll();
	}

	// SpringArm 무시 목록에서 제거
	if (SpringArmComponent)
	{
		SpringArmComponent->RemoveIgnoredActor(CarriedPerson);
		UE_LOG("[FirefighterCharacter] Removed CarriedPerson from SpringArm ignored actors");
	}

	// ★ 3단계: 바닥 배치 + Kinematic 모드 전환
	if (PersonMesh)
	{
		UE_LOG("[FirefighterCharacter] StopCarrying: Drop pos = (%.1f, %.1f, %.1f)",
			DropPos.X, DropPos.Y, DropPos.Z);

		// 최종 바닥 찾기
		FVector FloorPosition = DropPos;
		bool bFoundFloor = false;

		if (PhysScene)
		{
			FVector RayStart = FVector(DropPos.X, DropPos.Y, DropPos.Z + 0.5f);
			FVector RayDir = FVector(0, 0, -1);
			float MaxFloorDistance = 3.0f;

			FVector HitLocation, HitNormal;
			float HitDistance;

			if (PhysScene->Raycast(RayStart, RayDir, MaxFloorDistance, HitLocation, HitNormal, HitDistance, CarriedPerson))
			{
				FloorPosition.Z = HitLocation.Z + 0.05f;  // 바닥에서 5cm 위
				bFoundFloor = true;
				UE_LOG("[FirefighterCharacter] StopCarrying: Found floor at Z=%.2f", FloorPosition.Z);
			}
		}

		if (!bFoundFloor)
		{
			FloorPosition.Z = FirefighterPos.Z;
			UE_LOG("[FirefighterCharacter] StopCarrying: No floor found, using firefighter Z=%.2f", FloorPosition.Z);
		}

		// ★ Kinematic 모드로 전환 (바디 유지하여 선택 가능하게)
		PersonMesh->SetPhysicsMode(EPhysicsMode::Kinematic);

		// ★ 액터 위치를 바닥으로 설정
		CarriedPerson->SetActorLocation(FloorPosition);

		UE_LOG("[FirefighterCharacter] StopCarrying: Placed at (%.1f, %.1f, %.1f), Kinematic mode",
			FloorPosition.X, FloorPosition.Y, FloorPosition.Z);

		// ★ SafeZone 체크: 구조 구역 안에 배치되었는지 확인
		ARescueGameMode* RescueMode = World ? Cast<ARescueGameMode>(World->GetGameMode()) : nullptr;
		if (RescueMode && RescueMode->IsInSafeZone(FloorPosition))
		{
			// 구조 성공!
			UE_LOG("[FirefighterCharacter] StopCarrying: Person rescued in SafeZone!");
			RescueMode->OnPersonRescued();

			// 구출 완료 사운드 재생 (이름에 따라 남성/여성 구분)
			FString PersonName = CarriedPerson->GetName();
			if (PersonName.find("David") != FString::npos || PersonName.find("Lewis") != FString::npos)
			{
				if (ManThankYouSound)
				{
					FAudioDevice::PlaySound3D(ManThankYouSound, FloorPosition, 1.0f, false);
					UE_LOG("[FirefighterCharacter] Playing ManThankYou sound");
				}
			}
			else if (PersonName.find("Elizabeth") != FString::npos ||
			         PersonName.find("Sophie") != FString::npos ||
			         PersonName.find("Suzie") != FString::npos)
			{
				if (WomanThankYouSound)
				{
					FAudioDevice::PlaySound3D(WomanThankYouSound, FloorPosition, 1.0f, false);
					UE_LOG("[FirefighterCharacter] Playing WomanThankYou sound");
				}
			}

			// 사람 배치 및 조작 불가 처리 (사라지지 않고 그대로 유지)
			// Kinematic 모드 유지 (바닥에 고정)
			PersonMesh->SetPhysicsMode(EPhysicsMode::Kinematic);

			UItemComponent* ItemComp = Cast<UItemComponent>(
				CarriedPerson->GetComponent(UItemComponent::StaticClass()));
			if (ItemComp)
			{
				ItemComp->SetCanPickUp(false);  // 다시 선택 불가
			}

			// 상태 초기화
			CarriedPerson = nullptr;
			bIsCarryingPerson = false;
			return;
		}
	}

	// SafeZone 밖에 배치된 경우: 다시 줍기 가능하도록 설정
	UItemComponent* ItemComp = Cast<UItemComponent>(
		CarriedPerson->GetComponent(UItemComponent::StaticClass()));
	if (ItemComp)
	{
		ItemComp->SetCanPickUp(true);
		UE_LOG("[FirefighterCharacter] Restored bCanPickUp = true");
	}

	// 상태 초기화
	CarriedPerson = nullptr;
	bIsCarryingPerson = false;
	OriginalSpineLength = 0.0f;
}

void AFirefighterCharacter::RebindBoneSockets()
{
	UE_LOG("[FirefighterCharacter] RebindBoneSockets called");

	// 메시 변경 후 BoneSocketComponent에 새 스켈레톤의 본을 다시 바인딩
	if (LeftHandSocket && MeshComponent)
	{
		bool bFound = LeftHandSocket->SetTargetByName(MeshComponent, "mixamorig:LeftHand");
		UE_LOG("[FirefighterCharacter] RebindBoneSockets: LeftHandSocket BoneIndex=%d, found=%d",
			LeftHandSocket->BoneIndex, bFound ? 1 : 0);
	}
	if (RightHandSocket && MeshComponent)
	{
		bool bFound = RightHandSocket->SetTargetByName(MeshComponent, "mixamorig:RightHand");
		UE_LOG("[FirefighterCharacter] RebindBoneSockets: RightHandSocket BoneIndex=%d, found=%d",
			RightHandSocket->BoneIndex, bFound ? 1 : 0);
	}
}

void AFirefighterCharacter::ProcessGamepadInput()
{
	UInputManager& InputManager = UInputManager::GetInstance();

	// 게임패드가 연결되어 있지 않으면 리턴
	if (!InputManager.IsGamepadConnected())
	{
		return;
	}

	// 좌 스틱 이동 입력
	FVector2D LeftStick = InputManager.GetGamepadLeftStick();

	if (LeftStick.Y != 0.0f)
	{
		MoveForwardCamera(LeftStick.Y);
	}

	if (LeftStick.X != 0.0f)
	{
		MoveRightCamera(LeftStick.X);
	}

	// A 버튼 점프
	if (InputManager.IsGamepadButtonPressed(GamepadA))
	{
		Jump();
	}
	else if (InputManager.IsGamepadButtonReleased(GamepadA))
	{
		StopJumping();
	}
}

void AFirefighterCharacter::Kill()
{
	// 즉시 사망 처리 (산소 고갈 등 외부에서 호출)
	Health = 0.0f;
	Die();
}

void AFirefighterCharacter::ForceStopWaterMagic()
{
	if (!bIsUsingWaterMagic)
	{
		return;
	}

	UE_LOG("ForceStopWaterMagic: Forcefully stopping water magic");

	// 물 마법 상태 리셋
	bIsUsingWaterMagic = false;

	// 파티클 중지
	StopWaterMagicEffect();

	// ExtinguishGauge를 0으로 설정 (Lua에서 이를 감지하여 종료 처리)
	ExtinguishGauge = 0.0f;
}

void AFirefighterCharacter::SetCanUseWaterMagic(bool bCanUse)
{
	bCanUseWaterMagic = bCanUse;

	// 사용 불가능하게 설정되면 ExtinguishGauge도 0으로
	if (!bCanUse)
	{
		ExtinguishGauge = 0.0f;

		// 현재 사용 중이면 강제 종료
		if (bIsUsingWaterMagic)
		{
			ForceStopWaterMagic();
		}
	}
}

void AFirefighterCharacter::PlayCollisionEffect(const FVector& Position)
{
	// 쿨타임 중이면 재생하지 않음
	if (CollisionEffectCooldownTimer > 0.0f)
	{
		return;
	}

	// 쿨타임 시작
	CollisionEffectCooldownTimer = CollisionEffectCooldown;

	// 랜덤 파티클 선택 (1~4)
	int32 ParticleIndex = rand() % 4;
	if (CollisionBoomParticles[ParticleIndex])
	{
		UE_LOG("[CollisionEffect] Position: (%.2f, %.2f, %.2f), Particle: %d", Position.X, Position.Y, Position.Z, ParticleIndex + 1);
		CollisionBoomParticles[ParticleIndex]->SetWorldLocation(Position);
		CollisionBoomParticles[ParticleIndex]->ResetParticles();
		CollisionBoomParticles[ParticleIndex]->ActivateSystem();
	}

	// 랜덤 사운드 선택 (1~2)
	int32 SoundIndex = rand() % 2;
	if (GlassBreakSounds[SoundIndex])
	{
		FAudioDevice::PlaySound3D(GlassBreakSounds[SoundIndex], Position, 0.7f, false);
	}
}

void AFirefighterCharacter::HandleCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 사망 상태면 무시
	if (bIsDead)
	{
		return;
	}

	// 충돌한 컴포넌트가 없으면 무시
	if (!OtherComp)
	{
		return;
	}

	// 충돌한 컴포넌트가 StaticMesh인지 확인
	UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(OtherComp);
	if (!StaticMesh)
	{
		return;
	}

	// 물리 시뮬레이션 중인 (움직이는) 메시인지 확인
	if (!StaticMesh->bSimulatePhysics)
	{
		return;
	}

	// 충돌한 오브젝트 위치에서 이펙트 재생
	FVector ParticlePlayLocation = OtherComp->GetWorldLocation();
	ParticlePlayLocation.Z += 1.0f;
	PlayCollisionEffect(ParticlePlayLocation);
}