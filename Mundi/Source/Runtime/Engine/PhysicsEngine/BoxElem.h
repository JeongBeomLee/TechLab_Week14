#pragma once
#include <cmath>
#include "ShapeElem.h"
#include "PhysXSupport.h"
#include "PhysxConverter.h"
#include "FKBoxElem.generated.h"

/** 충돌을 위해 사용되는 박스 도형 */
USTRUCT()
struct FKBoxElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

    static inline EAggCollisionShape StaticShapeType = EAggCollisionShape::Box;

    /** 박스의 원점 */
    UPROPERTY(EditAnywhere, Category="Transform")
    FVector Center;

    /** 박스의 회전 (쿼터니언) */
    UPROPERTY(EditAnywhere, Category="Transform")
    FQuat Rotation;

    /** x축 방향 박스의 Half Extent (중심에서 면까지 거리) */
    UPROPERTY(EditAnywhere, Category="Shape")
    float X;

    /** Y축 방향 박스의 Half Extent (중심에서 면까지 거리) */
    UPROPERTY(EditAnywhere, Category="Shape")
    float Y;

    /** Z축 방향 박스의 Half Extent (중심에서 면까지 거리) */
    UPROPERTY(EditAnywhere, Category="Shape")
    float Z;

    FKBoxElem()
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(1.f), Y(1.f), Z(1.f)
    {
    }

    FKBoxElem(float s)
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(s), Y(s), Z(s)
    {
    }

    FKBoxElem(float InX, float InY, float InZ)
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(InX), Y(InY), Z(InZ)
    {
    }

    virtual ~FKBoxElem() = default;

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

    PxBoxGeometry GetPxGeometry(const FVector& Scale3D) const
    {
        // X, Y, Z는 half-extent로 취급 (에디터와 일치)
        float HalfX = X * FMath::Abs(Scale3D.X);
        float HalfY = Y * FMath::Abs(Scale3D.Y);
        float HalfZ = Z * FMath::Abs(Scale3D.Z);

        // 축변환 적용: 엔진 (X,Y,Z) → PhysX (Y,Z,-X)
        // 엔진 Z-up → PhysX Y-up 변환
        physx::PxVec3 HalfExtent = PhysxConverter::ToPxVec3(FVector(HalfX, HalfY, HalfZ));
        // 축변환으로 인한 음수 처리
        HalfExtent.x = std::abs(HalfExtent.x);
        HalfExtent.y = std::abs(HalfExtent.y);
        HalfExtent.z = std::abs(HalfExtent.z);

        constexpr float MinExtent = 0.01f;

        return physx::PxBoxGeometry(
            FMath::Max(HalfExtent.x, MinExtent),
            FMath::Max(HalfExtent.y, MinExtent),
            FMath::Max(HalfExtent.z, MinExtent)
        );
    }
};
