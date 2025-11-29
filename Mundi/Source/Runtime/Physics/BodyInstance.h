#pragma once
#include "PhysicsSystem.h"

// 전방 선언
class UPrimitiveComponent;
class UPhysicalMaterial;
class UBodySetup;

// DOF(Degree of Freedom) 잠금 플래그
UENUM()
enum class EDOFMode : uint8
{
    None,           // 잠금 없음
    SixDOF,         // 6DOF 모드 (개별 축 잠금 가능)
    YZPlane,        // X축 고정 (YZ 평면에서만 이동)
    XZPlane,        // Y축 고정 (XZ 평면에서만 이동)
    XYPlane,        // Z축 고정 (XY 평면에서만 이동)
    CustomPlane     // 커스텀 평면
};

// NOTE: FBodyInstance는 PhysX 리소스를 직접 관리하며 복사/이동이 금지된 런타임 객체임
// USTRUCT 리플렉션을 적용하지 않는다. (TArray 조작 불가)

struct FBodyInstance
{
public:
    // --- 소유자 정보 ---
    UPrimitiveComponent* OwnerComponent = nullptr;
    FName BoneName = "None"; // 랙돌용 뼈 이름
    int32 BoneIndex = -1;

    // --- PhysX 객체 ---
    physx::PxRigidActor* RigidActor = nullptr;
    TArray<physx::PxShape*> Shapes; // 다중 Shape 지원

    // --- 기본 설정값 ---
    bool bSimulatePhysics = false;  // true=Dynamic, false=Static/Kinematic
    bool bUseGravity = true;
    bool bStartAwake = true;        // 시작 시 활성 상태
    bool bEnableGravity = true;

    // --- 질량 설정 ---
    bool bOverrideMass = false;
    float MassInKg = 10.0f;
    float LinearDamping = 0.01f;
    float AngularDamping = 0.05f;

    // --- 충돌 설정 ---
    bool bIsTrigger = false;
    bool bNotifyRigidBodyCollision = true;  // 충돌 이벤트 발생 여부
    bool bUseCCD = false;                    // Continuous Collision Detection

    // --- DOF 잠금 ---
    EDOFMode DOFMode = EDOFMode::None;
    bool bLockXLinear = false;
    bool bLockYLinear = false;
    bool bLockZLinear = false;
    bool bLockXAngular = false;
    bool bLockYAngular = false;
    bool bLockZAngular = false;

    // --- 속도 제한 ---
    float MaxLinearVelocity = 0.0f;     // 0 = 무제한
    float MaxAngularVelocity = 0.0f;    // 0 = 무제한 (deg/s)

    // --- 슬립 설정 ---
    float SleepThresholdMultiplier = 1.0f;

    // --- 물리 재질 ---
    UPhysicalMaterial* PhysMaterialOverride = nullptr;

    // --- 스케일 ---
    FVector Scale3D = FVector::One();

    // --- 생성자/소멸자 ---
    FBodyInstance();
    explicit FBodyInstance(UPrimitiveComponent* Owner);
    ~FBodyInstance();

    // 복사/이동 방지 (PhysX 리소스 관리)
    FBodyInstance(const FBodyInstance&) = delete;
    FBodyInstance& operator=(const FBodyInstance&) = delete;

    // --- 초기화/해제 ---
    // 단일 Geometry로 초기화 (기존 호환)
    void InitBody(const FTransform& Transform, const physx::PxGeometry& Geometry, UPhysicalMaterial* PhysMat = nullptr);

    // UBodySetup으로 초기화 (다중 Shape 지원)
    void InitBodyFromSetup(const FTransform& Transform, class UBodySetup* InBodySetup,
                           UPhysicalMaterial* PhysMat = nullptr, const FVector& Scale3D = FVector::One());

    void TermBody();
    bool IsValidBodyInstance() const { return RigidActor != nullptr; }

    // --- Transform ---
    FTransform GetWorldTransform() const;
    void SetWorldTransform(const FTransform& NewTransform, bool bTeleport = false);
    FVector GetWorldLocation() const;
    FQuat GetWorldRotation() const;

    // --- 물리 상태 ---
    bool IsDynamic() const;
    bool IsAwake() const;
    void WakeUp();
    void PutToSleep();

    // --- 힘/토크/속도 ---
    void AddForce(const FVector& Force, bool bAccelChange = false);
    void AddForceAtLocation(const FVector& Force, const FVector& Location);
    void AddTorque(const FVector& Torque, bool bAccelChange = false);
    void AddImpulse(const FVector& Impulse, bool bVelChange = false);
    void AddImpulseAtLocation(const FVector& Impulse, const FVector& Location);
    void AddAngularImpulse(const FVector& AngularImpulse, bool bVelChange = false);

    FVector GetLinearVelocity() const;
    void SetLinearVelocity(const FVector& Velocity, bool bAddToCurrent = false);
    FVector GetAngularVelocity() const;
    void SetAngularVelocity(const FVector& AngularVelocity, bool bAddToCurrent = false);

    // --- 질량 ---
    float GetBodyMass() const;
    void SetMassOverride(float InMassInKg, bool bNewOverrideMass = true);
    FVector GetBodyInertiaTensor() const;

    // --- 충돌 설정 ---
    void SetCollisionEnabled(bool bEnabled);
    void SetSimulatePhysics(bool bNewSimulate);
    void SetEnableGravity(bool bNewEnableGravity);

    // --- DOF 잠금 적용 ---
    // init 전에 설정해주면 설정된 잠금 축 이외의 모든 움직임을 강제로 막음
    void ApplyDOFLock();

private:
    // 내부 헬퍼 함수
    void UpdateMassProperties();
    void ApplyPhysicsSettings();
    physx::PxRigidDynamic* GetDynamicActor() const;
};
