#include "pch.h"
#include "FireActor.h"
#include "ParticleSystemComponent.h"
#include "ParticleSystem.h"
#include "SphereComponent.h"
#include "Sound.h"
#include "FAudioDevice.h"
#include "ResourceManager.h"
#include "JsonSerializer.h"
#include "World.h"
#include "FirefighterCharacter.h"

AFireActor::AFireActor()
	: bIsActive(true)
	, FireIntensity(1.0f)
	, InitialFireIntensity(1.0f)
	, MaxFireIntensity(2.0f)
	, BaseScale(1.0f, 1.0f, 1.0f)
	, FireLoopSound(nullptr)
	, FireExtinguishSound(nullptr)
	, FireLoopVoice(nullptr)
	, ExtinguishSoundCooldown(0.0f)
{
	ObjectName = "Fire Actor";

	// 불 파티클 컴포넌트 생성
	FireParticle = CreateDefaultSubobject<UParticleSystemComponent>("FireParticle");
	if (FireParticle)
	{
		RootComponent = FireParticle;
		FireParticle->bAutoActivate = true;

		// IntenseFire 파티클 로드
		UParticleSystem* FireEffect = UResourceManager::GetInstance().Load<UParticleSystem>("Data/Particles/IntenseFire.particle");
		if (FireEffect)
		{
			FireParticle->SetTemplate(FireEffect);
		}
	}

	// 데미지 감지용 스피어 컴포넌트 생성
	DamageSphere = CreateDefaultSubobject<USphereComponent>("DamageSphere");
	if (DamageSphere && FireParticle)
	{
		DamageSphere->SetupAttachment(FireParticle);
		DamageSphere->SetSphereRadius(FireRadius);
		DamageSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		DamageSphere->SetSimulatePhysics(false);
		DamageSphere->SetGenerateOverlapEvents(true);
		DamageSphere->CollisionMask = CollisionMasks::Pawn;  // 캐릭터만 감지
	}

	// 사운드 로드
	FireLoopSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/fire.wav");
	FireExtinguishSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/fire_over.wav");
}

AFireActor::~AFireActor()
{
	// 루프 사운드 정지
	if (FireLoopVoice)
	{
		FAudioDevice::StopSound(FireLoopVoice);
		FireLoopVoice = nullptr;
	}
}

void AFireActor::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 설정한 기본 스케일 저장
	BaseScale = GetActorScale();

	// 초기 FireIntensity를 0.7~1.5 사이 랜덤으로 설정
	InitialFireIntensity = FMath::RandRange(0.7f, 1.5f);
	MaxFireIntensity = InitialFireIntensity * 2.0f;
	FireIntensity = InitialFireIntensity;

	// 성장 속도 계산: 100초에 2배 (초기값만큼 증가)
	// GrowthRate = InitialFireIntensity / 100.0f
	FireGrowthRate = InitialFireIntensity / 100.0f;

	// 초기 스케일 적용
	UpdateFireScale();

	if (bIsActive && FireParticle)
	{
		FireParticle->ActivateSystem();
	}

	// 불이 활성화 상태면 루프 사운드 시작
	if (bIsActive && FireLoopSound)
	{
		FireLoopVoice = FAudioDevice::PlaySound3D(FireLoopSound, GetActorLocation(), 1.0f, true);
	}
}

void AFireActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 꺼지는 사운드 쿨다운 감소
	if (ExtinguishSoundCooldown > 0.0f)
	{
		ExtinguishSoundCooldown -= DeltaSeconds;
	}

	// 루프 사운드 위치 업데이트
	if (FireLoopVoice)
	{
		FAudioDevice::UpdateSoundPosition(FireLoopVoice, GetActorLocation());
	}

	// 불이 비활성화 상태면 데미지 없음
	if (!bIsActive)
	{
		return;
	}

	// 불 성장: 시간에 따라 FireIntensity 증가 (최대값까지)
	if (FireGrowthRate > 0.0f && FireIntensity < MaxFireIntensity)
	{
		FireIntensity += FireGrowthRate * DeltaSeconds;
		if (FireIntensity > MaxFireIntensity)
		{
			FireIntensity = MaxFireIntensity;
		}
		UpdateFireScale();
	}

	// 월드에서 플레이어 캐릭터 찾기
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AFirefighterCharacter* Player = World->FindActor<AFirefighterCharacter>();
	if (!Player || Player->bIsDead)
	{
		return;
	}

	// 거리 체크 (스케일 적용된 반경 사용)
	FVector FireLocation = GetActorLocation();
	FVector PlayerLocation = Player->GetActorLocation();
	float Distance = (FireLocation - PlayerLocation).Size();

	// 스케일 적용된 데미지 반경
	FVector CurrentScale = GetActorScale();
	float ScaledRadius = FireRadius * CurrentScale.X;

	if (Distance <= ScaledRadius)
	{
		float Damage = DamagePerSecond * FireIntensity;

		Player->TakeDamage(Damage);
	}
}

void AFireActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UParticleSystemComponent* Particle = Cast<UParticleSystemComponent>(Component))
		{
			FireParticle = Particle;
		}
		else if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
		{
			DamageSphere = Sphere;
		}
	}

	// 사운드 다시 로드
	FireLoopSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/fire.wav");
	FireExtinguishSound = UResourceManager::GetInstance().Load<USound>("Data/Audio/fire_over.wav");
}

void AFireActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FireParticle = Cast<UParticleSystemComponent>(RootComponent);

		for (UActorComponent* Component : OwnedComponents)
		{
			if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
			{
				DamageSphere = Sphere;
				break;
			}
		}

		// 불 상태 로드
		FJsonSerializer::ReadBool(InOutHandle, "bIsActive", bIsActive, true);
		FJsonSerializer::ReadFloat(InOutHandle, "FireIntensity", FireIntensity, 1.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "DamagePerSecond", DamagePerSecond, 50.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "FireRadius", FireRadius, 2.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "WaterDamageMultiplier", WaterDamageMultiplier, 1.0f);

		// 로딩 후 반경 업데이트
		if (DamageSphere)
		{
			DamageSphere->SetSphereRadius(FireRadius);
		}
	}
	else
	{
		// 불 상태 저장
		InOutHandle["bIsActive"] = bIsActive;
		InOutHandle["FireIntensity"] = FireIntensity;
		InOutHandle["DamagePerSecond"] = DamagePerSecond;
		InOutHandle["FireRadius"] = FireRadius;
		InOutHandle["WaterDamageMultiplier"] = WaterDamageMultiplier;
	}
}

void AFireActor::SetFireActive(bool bActive)
{
	bIsActive = bActive;

	if (FireParticle)
	{
		if (bActive)
		{
			FireParticle->ResetParticles();
			FireParticle->ActivateSystem();
		}
		else
		{
			FireParticle->DeactivateSystem();
		}
	}

	// 불 루프 사운드 제어
	if (bActive)
	{
		if (FireLoopSound && !FireLoopVoice)
		{
			FireLoopVoice = FAudioDevice::PlaySound3D(FireLoopSound, GetActorLocation(), 1.0f, true);
		}
	}
	else
	{
		if (FireLoopVoice)
		{
			FAudioDevice::StopSound(FireLoopVoice);
			FireLoopVoice = nullptr;
		}
	}
}

void AFireActor::SetFireIntensity(float Intensity)
{
	// 클램프: 0 ~ MaxFireIntensity (최대 3.0 정도까지)
	FireIntensity = FMath::Clamp(Intensity, 0.0f, MaxFireIntensity);

	UpdateFireScale();

	// 불이 완전히 꺼지면 비활성화
	if (FireIntensity <= 0.0f)
	{
		SetFireActive(false);
	}
}

void AFireActor::UpdateFireScale()
{
	// 스케일 계산: Intensity 0->0.5, 1->1.0, 3->2.0
	// Scale = 0.5 + 0.5 * Intensity
	float ScaleMultiplier = 0.5f + 0.5f * FireIntensity;

	// 에디터에서 설정한 기본 스케일에 곱함
	FVector FinalScale = BaseScale * ScaleMultiplier;
	SetActorScale(FinalScale);
}

void AFireActor::ApplyWaterDamage(float DamageAmount)
{
	if (!bIsActive)
	{
		UE_LOG("ApplyWaterDamage: Fire is not active, skipping");
		return;
	}

	// 물 데미지를 불 세기에 적용
	float ActualDamage = DamageAmount * WaterDamageMultiplier;
	float NewIntensity = FireIntensity - ActualDamage;

	UE_LOG("ApplyWaterDamage: DamageAmount=%.4f, Multiplier=%.2f, ActualDamage=%.4f, OldIntensity=%.2f, NewIntensity=%.2f",
		DamageAmount, WaterDamageMultiplier, ActualDamage, FireIntensity, NewIntensity);

	// 불이 작아질 때 fire_over 사운드 재생 (쿨다운 적용)
	if (FireExtinguishSound && ExtinguishSoundCooldown <= 0.0f)
	{
		FAudioDevice::PlaySound3D(FireExtinguishSound, GetActorLocation(), 1.0f, false);
		ExtinguishSoundCooldown = 0.3f;  // 0.3초 쿨다운
	}

	SetFireIntensity(NewIntensity);
}
