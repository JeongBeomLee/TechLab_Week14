#pragma once
#include "Character.h"
#include "AFirefighterCharacter.generated.h"

class UAudioComponent;
class USphereComponent;
class ULuaScriptComponent;
class UParticleSystemComponent;
class UBoneSocketComponent;

UCLASS(DisplayName = "파이어 파이터 캐릭터", Description = "렛츠고 파이어 파이터")
class AFirefighterCharacter : public ACharacter
{
    GENERATED_REFLECTION_BODY()

public:
    AFirefighterCharacter();

    // ────────────────────────────────────────────────
    // 체력 및 소화 게이지
    // ────────────────────────────────────────────────

    /** 현재 체력 */
    UPROPERTY(LuaBind, DisplayName="Health")
    float Health = 100.0f;

    /** 최대 체력 */
    UPROPERTY(LuaBind, DisplayName="MaxHealth")
    float MaxHealth = 100.0f;

    /** 현재 소화 게이지 */
    UPROPERTY(LuaBind, DisplayName="ExtinguishGauge")
    float ExtinguishGauge = 100.0f;

    /** 최대 소화 게이지 */
    UPROPERTY(LuaBind, DisplayName="MaxExtinguishGauge")
    float MaxExtinguishGauge = 100.0f;
    
    // ────────────────────────────────────────────────
    // 스케일 설정
    // ────────────────────────────────────────────────

    /**
     * 캐릭터 스케일을 설정합니다.
     * 캡슐, 메쉬 오프셋, 스프링암, 아이템 감지 반경 등 모든 관련 수치를 조절합니다.
     */
    void SetCharacterScale(float Scale);

    /** 현재 캐릭터 스케일 반환 */
    float GetCharacterScale() const { return CharacterScale; }

    // ────────────────────────────────────────────────
    // 파티클 이펙트
    // ────────────────────────────────────────────────

    /** 소방복 장착 파티클 재생 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="PlayFireSuitEquipEffect")
    void PlayFireSuitEquipEffect();

    /** 물 마법 파티클 재생 시작 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="PlayWaterMagicEffect")
    void PlayWaterMagicEffect();

    /** 물 마법 파티클 재생 중지 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="StopWaterMagicEffect")
    void StopWaterMagicEffect();

    /** 소화 게이지 감소 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="DrainExtinguishGauge")
    void DrainExtinguishGauge(float Amount);

    /** 물 마법 발사 - 전방 Raycast로 불 액터에 데미지 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="FireWaterMagic")
    void FireWaterMagic(float DamageAmount);

    /** 물 마법 사거리 */
    UPROPERTY(LuaBind, DisplayName="WaterMagicRange")
    float WaterMagicRange = 14.0f;

    // ────────────────────────────────────────────────
    // 데미지 및 사망 시스템
    // ────────────────────────────────────────────────

    /** 데미지를 받음 (쿨타임 적용) */
    UFUNCTION(LuaBind, DisplayName="TakeDamage")
    void TakeDamage(float DamageAmount);

    /** 사망 여부 */
    UPROPERTY(LuaBind, DisplayName="bIsDead")
    bool bIsDead = false;

    /** 물 마법 사용 중 설정 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="SetUsingWaterMagic")
    void SetUsingWaterMagic(bool bUsing) { bIsUsingWaterMagic = bUsing; }

    /** 물 마법 사용 중 여부 반환 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="IsUsingWaterMagic")
    bool IsUsingWaterMagic() const { return bIsUsingWaterMagic; }

    /** 데미지 쿨타임 (초) */
    UPROPERTY(LuaBind, DisplayName="DamageCooldown")
    float DamageCooldown = 0.5f;

protected:
    ~AFirefighterCharacter() override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent) override;

    // ────────────────────────────────────────────────
    // 아이템 감지
    // ────────────────────────────────────────────────

    /** 아이템 감지용 스피어 컴포넌트 */
    USphereComponent* ItemDetectionSphere;

    /** 아이템 줍기 스크립트 */
    ULuaScriptComponent* ItemPickupScript;

    // ────────────────────────────────────────────────
    // 입력 바인딩
    // ────────────────────────────────────────────────

    virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

    // ────────────────────────────────────────────────
    // 이동 입력 (카메라 방향 기준)
    // ────────────────────────────────────────────────

    /** 카메라 방향 기준 앞/뒤 이동 */
    void MoveForwardCamera(float Value);

    /** 카메라 방향 기준 좌/우 이동 */
    void MoveRightCamera(float Value);

private:
    // ────────────────────────────────────────────────
    // 스케일 관련
    // ────────────────────────────────────────────────

    /** 현재 캐릭터 스케일 */
    float CharacterScale = 1.0f;

    // ────────────────────────────────────────────────
    // 회전 설정
    // ────────────────────────────────────────────────

    /** 이동 방향으로 캐릭터 회전 여부 */
    bool bOrientRotationToMovement;

    /** 회전 보간 속도 (도/초) */
    float RotationRate;

    /** 현재 프레임의 이동 방향 (회전 계산용) */
    FVector CurrentMovementDirection;

    /** Lua 스크립트 컴포넌트 (애니메이션 제어용) */
    ULuaScriptComponent* LuaScript;

    /** 소방복 장착 파티클 컴포넌트 */
    UParticleSystemComponent* FireSuitEquipParticle;

    /** 물 마법 파티클 컴포넌트 */
    UParticleSystemComponent* WaterMagicParticle;

    /** 데미지 쿨타임 타이머 */
    float DamageCooldownTimer = 0.0f;

    /** 물 마법 사용 중 여부 */
    bool bIsUsingWaterMagic = false;

    /** 사망 처리 */
    void Die();

    /** 게임패드 입력 처리 (좌 스틱 이동, A 버튼 점프) */
    void ProcessGamepadInput();

    //USound* SorrySound;
    //USound* HitSound;

    // ────────────────────────────────────────────────
    // 손 본 소켓 (사람 들기용)
    // ────────────────────────────────────────────────
public:
    /** 왼손 본 소켓 컴포넌트 */
    UBoneSocketComponent* LeftHandSocket;

    /** 오른손 본 소켓 컴포넌트 */
    UBoneSocketComponent* RightHandSocket;

    /** 현재 들고 있는 사람 (스켈레탈 메시 컴포넌트의 오너 액터) */
    AActor* CarriedPerson = nullptr;

    /** 사람을 들고 있는지 여부 */
    UPROPERTY(LuaBind, DisplayName="bIsCarryingPerson")
    bool bIsCarryingPerson = false;

    /** 왼손 소켓 getter (Lua용) */
    UFUNCTION(LuaBind, DisplayName="GetLeftHandSocket")
    UBoneSocketComponent* GetLeftHandSocket() const { return LeftHandSocket; }

    /** 오른손 소켓 getter (Lua용) */
    UFUNCTION(LuaBind, DisplayName="GetRightHandSocket")
    UBoneSocketComponent* GetRightHandSocket() const { return RightHandSocket; }

    /** 사람 들기 시작 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="StartCarryingPerson")
    void StartCarryingPerson(FGameObject* PersonGameObject);

    /** 사람 내려놓기 (Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="StopCarryingPerson")
    void StopCarryingPerson();

    /** 본 소켓 재바인딩 (메시 변경 후 호출, Lua에서 호출) */
    UFUNCTION(LuaBind, DisplayName="RebindBoneSockets")
    void RebindBoneSockets();
};
