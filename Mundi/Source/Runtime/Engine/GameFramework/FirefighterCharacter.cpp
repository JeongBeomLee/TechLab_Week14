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
		UPhysicsAsset* PhysAsset = UResourceManager::GetInstance().Load<UPhysicsAsset>("Data/Physics/Firefighter.physicsasset");
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
}

AFirefighterCharacter::~AFirefighterCharacter()
{
}

void AFirefighterCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 애니메이션은 LuaScriptComponent(FirefighterController.lua)에서 처리
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
	// 이동 방향으로 캐릭터 회전 (Super::Tick 전에 처리해야 SpringArm이 올바른 회전을 사용)
	// MaxWalkSpeed가 0이면 (PickUp 등) 회전도 비활성화
	const bool bCanRotate = CharacterMovement && CharacterMovement->MaxWalkSpeed > 0.01f;
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

	// 데미지 쿨타임 타이머 업데이트
	if (DamageCooldownTimer > 0.0f)
	{
		DamageCooldownTimer -= DeltaSeconds;
	}
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
}

void AFirefighterCharacter::StopWaterMagicEffect()
{
	if (WaterMagicParticle)
	{
		WaterMagicParticle->DeactivateSystem();
	}
}

void AFirefighterCharacter::DrainExtinguishGauge(float Amount)
{
	ExtinguishGauge = FMath::Max(0.0f, ExtinguishGauge - Amount);
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

	// 데미지 적용
	Health -= DamageAmount;
	DamageCooldownTimer = DamageCooldown;

	UE_LOG("TakeDamage: %.1f damage, Health: %.1f/%.1f", DamageAmount, Health, MaxHealth);

	// 체력이 0 이하면 사망
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		Die();
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