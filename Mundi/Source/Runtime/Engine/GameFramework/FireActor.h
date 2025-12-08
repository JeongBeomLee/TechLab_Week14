#pragma once

#include "Actor.h"
#include "AFireActor.generated.h"

class UParticleSystemComponent;
class USphereComponent;
class USound;
struct IXAudio2SourceVoice;

UCLASS(DisplayName = "불 액터", Description = "강렬한 불 이펙트를 생성하는 액터입니다")
class AFireActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AFireActor();

protected:
	~AFireActor() override;

public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	/** 파티클 컴포넌트 반환 */
	UParticleSystemComponent* GetParticleComponent() const { return FireParticle; }

	/** 불 활성화/비활성화 */
	UFUNCTION(LuaBind, DisplayName = "SetFireActive")
	void SetFireActive(bool bActive);

	/** 불이 활성화 상태인지 확인 */
	UFUNCTION(LuaBind, DisplayName = "IsFireActive")
	bool IsFireActive() const { return bIsActive; }

	/** 불 세기 설정 (0.0 ~ 1.0) */
	UFUNCTION(LuaBind, DisplayName = "SetFireIntensity")
	void SetFireIntensity(float Intensity);

	/** 불 세기 반환 */
	UFUNCTION(LuaBind, DisplayName = "GetFireIntensity")
	float GetFireIntensity() const { return FireIntensity; }

	/** 물 데미지 적용 (물 마법에 맞으면 호출) */
	UFUNCTION(LuaBind, DisplayName = "ApplyWaterDamage")
	void ApplyWaterDamage(float DamageAmount);

	/** 물에 의한 데미지 배율 (기본 1.0) */
	UPROPERTY(LuaBind, DisplayName = "WaterDamageMultiplier")
	float WaterDamageMultiplier = 1.0f;

	/** 데미지 양 */
	UPROPERTY(LuaBind, DisplayName = "DamagePerSecond")
	float DamagePerSecond = 50.0f;

	/** 불 반경 */
	UPROPERTY(LuaBind, DisplayName = "FireRadius")
	float FireRadius = 2.0f;

protected:
	/** 불 파티클 컴포넌트 */
	UParticleSystemComponent* FireParticle;

	/** 데미지 감지용 스피어 컴포넌트 */
	USphereComponent* DamageSphere;

	/** 불 루프 사운드 (fire.wav) */
	USound* FireLoopSound;

	/** 불 꺼지는 사운드 (fire_over.wav) */
	USound* FireExtinguishSound;

	/** 현재 재생 중인 루프 사운드 Voice */
	IXAudio2SourceVoice* FireLoopVoice;

	/** 꺼지는 사운드 쿨다운 타이머 */
	float ExtinguishSoundCooldown;

	/** 불 활성화 상태 */
	bool bIsActive;

	/** 불 세기 (0.0 ~ 1.0) */
	float FireIntensity;
};
