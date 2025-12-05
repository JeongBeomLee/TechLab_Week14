#pragma once
#include "ShapeElem.h"
#include "PhysXSupport.h"
#include "FKSphylElem.generated.h"

/** 충돌을 위해 사용되는 캡슐(Sphyl) 도형
 *  Sphyl = Sphere + Cylinder (양 끝이 반구인 원기둥)
 */
USTRUCT()
struct FKSphylElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

    static inline EAggCollisionShape StaticShapeType = EAggCollisionShape::Sphyl;

    /** 캡슐의 중심 */
    UPROPERTY(EditAnywhere, Category="Transform")
    FVector Center;

    /** 캡슐의 회전 (쿼터니언) */
    UPROPERTY(EditAnywhere, Category="Transform")
    FQuat Rotation;

    /** 캡슐의 반지름 */
    UPROPERTY(EditAnywhere, Category="Shape")
    float Radius;

    /** 캡슐의 길이 (반구 제외, 원기둥 부분만) */
    UPROPERTY(EditAnywhere, Category="Shape")
    float Length;

    FKSphylElem()
        : FKShapeElem(EAggCollisionShape::Sphyl)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , Radius(0.5f)
        , Length(1.0f)
    {
    }

    FKSphylElem(float InRadius, float InLength)
        : FKShapeElem(EAggCollisionShape::Sphyl)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , Radius(InRadius)
        , Length(InLength)
    {
    }

    virtual ~FKSphylElem() = default;

    virtual FTransform GetTransform() const override final
    {
        FTransform Transform;
        Transform.Translation = Center;
        Transform.Rotation = Rotation;
        return Transform;
    }

    void SetTransform(const FTransform& InTransform)
    {
        Center = InTransform.Translation;
        Rotation = InTransform.Rotation;
    }

    /** 캡슐의 반높이 (Length / 2) */
    float GetHalfLength() const { return Length * 0.5f; }

    /** 캡슐의 전체 높이 (Length + 2 * Radius) */
    float GetTotalHeight() const { return Length + 2.0f * Radius; }

    PxCapsuleGeometry GetPxGeometry(const FVector& Scale3D) const
    {
        // 엔진에서 캡슐 길이 방향 = Z축, 단면 = XY 평면
        // (BodySetup.cpp에서 Z→X 회전을 적용하여 PhysX 캡슐(X축 길이)로 변환)
        float RadiusScale = FMath::Max(FMath::Abs(Scale3D.X), FMath::Abs(Scale3D.Y));
        float LengthScale = FMath::Abs(Scale3D.Z);

        constexpr float MinRadius = 0.01f;
        constexpr float MinHalfHeight = 0.01f;

        float ScaledRadius = FMath::Max(Radius * RadiusScale, MinRadius);
        float ScaledHalfHeight = FMath::Max(GetHalfLength() * LengthScale, MinHalfHeight);

        return physx::PxCapsuleGeometry(ScaledRadius, ScaledHalfHeight);
    }
};
