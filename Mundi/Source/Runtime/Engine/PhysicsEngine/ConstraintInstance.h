#pragma once

#include "Vector.h"
#include "Name.h"
#include "ELinearConstraintMotion.h"
#include "EAngularConstraintMotion.h"
#include "FConstraintInstance.generated.h"

// Forward declarations
namespace physx { class PxD6Joint; }
class FBodyInstance;
class UPrimitiveComponent;

// ===== Constraint Instance =====
// 두 본(Body) 사이의 물리 제약 조건
// Ragdoll 관절 설정에 사용
USTRUCT(DisplayName = "Constraint Instance", Description = "두 본 사이의 관절 제약 조건")
struct FConstraintInstance
{
    GENERATED_REFLECTION_BODY()
public:
    // ===== Constraint Bodies =====
    // 어떤 뼈와 어떤 뼈를 연결할지 (언리얼 규칙: 1=Child, 2=Parent)
    UPROPERTY(EditAnywhere, Category = "Constraint")
    FName ConstraintBone1;  // Child Bone (자손 본, 현재 본)

    UPROPERTY(EditAnywhere, Category = "Constraint")
    FName ConstraintBone2;  // Parent Bone (부모 본, 이전 본)

    // ===== Linear Limits (이동 제한) =====
    // Ragdoll은 뼈가 빠지면 안 되므로 보통 다 Locked
    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearXMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearYMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearZMotion = ELinearConstraintMotion::Locked;

    // 선형 제한 거리 (Limited일 때 사용)
    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    float LinearLimit = 0.0f;

    // ===== Angular Limits (회전 제한) =====
    // PhysX D6 Joint 기준: Twist=X축, Swing1=Y축, Swing2=Z축

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion TwistMotion = EAngularConstraintMotion::Limited;

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion Swing1Motion = EAngularConstraintMotion::Limited;

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion Swing2Motion = EAngularConstraintMotion::Limited;

    // 각도 제한값 (Degrees)
    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float TwistLimitAngle = 45.0f;      // X축 회전 제한

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float Swing1LimitAngle = 45.0f;     // Y축 회전 제한

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float Swing2LimitAngle = 45.0f;     // Z축 회전 제한

    // ===== Constraint Transform (Frame) =====
    // Child Bone (Bone1) 공간에서의 조인트 위치 및 회전
    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Position1 = FVector::Zero();

    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Rotation1 = FVector::Zero();    // Euler Angles (Degrees)

    // Parent Bone (Bone2) 공간에서의 조인트 위치 및 회전
    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Position2 = FVector::Zero();

    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Rotation2 = FVector::Zero();    // Euler Angles (Degrees)

    // ===== Collision Settings (충돌 설정) =====
    UPROPERTY(EditAnywhere, Category = "Collision")
    bool bDisableCollision = true;

    // ===== Motor Settings (Drive) =====
    UPROPERTY(EditAnywhere, Category = "Motor")
    bool bAngularMotorEnabled = false;

    UPROPERTY(EditAnywhere, Category = "Motor")
    float AngularMotorStrength = 0.0f;  // Stiffness (스프링 강도)

    UPROPERTY(EditAnywhere, Category = "Motor")
    float AngularMotorDamping = 0.0f;   // Damping (감쇠) - 떨림 방지

    // ===== Runtime Members (런타임 전용, 직렬화 안 됨) =====
    UPrimitiveComponent* OwnerComponent = nullptr;  // 소유자 컴포넌트
    physx::PxD6Joint* ConstraintData = nullptr;     // PhysX Joint (런타임에 생성)

    // ===== Runtime Methods =====
    FConstraintInstance() = default;
    ~FConstraintInstance();

    // Joint 초기화 (두 FBodyInstance 사이에 Joint 생성)
    // Body1 = Child Body, Body2 = Parent Body (언리얼 규칙)
    void InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, UPrimitiveComponent* InOwnerComponent);

    // Joint 해제
    void TermConstraint();

    // Joint가 유효한지 확인
    bool IsValidConstraintInstance() const { return ConstraintData != nullptr; }

    // PhysX Joint 접근
    physx::PxD6Joint* GetPxD6Joint() const { return ConstraintData; }

};
