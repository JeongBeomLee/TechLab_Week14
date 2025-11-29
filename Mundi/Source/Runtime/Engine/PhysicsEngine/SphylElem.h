#pragma once
#include "ShapeElem.h"
#include "PhysXSupport.h"

/** 충돌을 위해 사용되는 캡슐(Sphyl) 도형
 *  Sphyl = Sphere + Cylinder (양 끝이 반구인 원기둥)
 */
USTRUCT()
struct FKSphylElem : public FKShapeElem
{
	static inline EAggCollisionShape StaticShapeType = EAggCollisionShape::Sphyl;

	/** 캡슐의 중심 */
	FVector Center;

	/** 캡슐의 회전 (쿼터니언) */
	FQuat Rotation;

	/** 캡슐의 반지름 */
	float Radius;

	/** 캡슐의 길이 (반구 제외, 원기둥 부분만) */
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
		// 캡슐은 기본적으로 X축 방향으로 정렬
		// Radius는 YZ 평면 스케일, Length는 X 스케일 적용
		float RadiusScale = FMath::Max(FMath::Abs(Scale3D.Y), FMath::Abs(Scale3D.Z));
		float LengthScale = FMath::Abs(Scale3D.X);

		constexpr float MinRadius = 0.01f;
		constexpr float MinHalfHeight = 0.01f;

		float ScaledRadius = FMath::Max(Radius * RadiusScale, MinRadius);
		float ScaledHalfHeight = FMath::Max(GetHalfLength() * LengthScale, MinHalfHeight);

		return physx::PxCapsuleGeometry(ScaledRadius, ScaledHalfHeight);
	}
};
