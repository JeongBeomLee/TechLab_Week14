#pragma once
#include "BodySetup.h"

#include "FBodyInstance.generated.h"

USTRUCT()
struct FBodyInstance
{
    GENERATED_REFLECTION_BODY()

    FBodyInstance();
    ~FBodyInstance();

    // 복사 시 런타임 포인터는 복사하지 않음 (PhysX 리소스 공유 방지)
    FBodyInstance(const FBodyInstance& Other);
    FBodyInstance& operator=(const FBodyInstance& Other);

    /**
     * 단일 강체를 초기화한다.
     * @param Setup         모양 정보 (UBodySetup)
     * @param Transform     월드 트랜스폼
     * @param Component     바디를 소유한 컴포넌트
     * @param InRBScene     바디가 속할 물리 씬
     */
    void InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* Component, FPhysScene* InRBScene);

    /** 바디를 씬에서 제거하고 자원을 해제한다. */
    void TermBody();

    /** 물리 바디로부터 현재 월드 공간 트랜스폼을 가져온다. */
    FTransform GetUnrealWorldTransform() const;

    /** 바디의 선형 속도를 설정한다. */
    void SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent = false);

    /** 바디에 힘을 더한다. */
    void AddForce(const FVector& Force, bool bAccelChange = false);

    /** 바디에 토크를 더한다. */
    void AddTorque(const FVector& Torque, bool bAccelChange = false);

    /** 동적 바디여부 확인*/
    bool IsDynamic() const;
    
    /** 유효성 검사 */
    bool IsValidBodyInstance() const { return RigidActor != nullptr;}

public:
    /** 바디를 소유하고 있는 컴포넌트 */
    UPrimitiveComponent* OwnerComponent;

    /** 충돌체 정보를 가지고있는 바디 설정 */
    UBodySetup* BodySetup;

    /** 바디를 소유하고 있는 물리 씬 */
    FPhysScene* PhysScene;

    /** PhysX Actor */
    PxRigidActor* RigidActor;

    /** 바디를 생성하기 위한 3D 스케일 */
    FVector Scale3D;

    /** True일 경우, 물리 시뮬레이션을 수행한다 (False일 경우 kinematic/static). */
    bool bSimulatePhysics;

    /** 선형 댐핑 (이동에 대한 저항력) */
    float LinearDamping;

    /** 각형 댐핑 (회전에 대한 저항력) */
    float AngularDamping;

    /** 질량 오버라이드 */
    float MassInKgOverride;

    /** True일 경우, MassInKgOverride를 사용 */
    bool bOverrideMass;
};
