#pragma once

#include "EAggCollisionShape.h"
#include "Picking.h"

class UBodySetup;
struct FTransform;

/**
 * FPrimitiveHitResult
 *
 * Primitive 레벨 피킹 결과
 * 어떤 타입의 primitive가 hit됐는지, 해당 배열 내 인덱스, hit 거리 포함
 */
struct FPrimitiveHitResult
{
	EAggCollisionShape PrimitiveType = EAggCollisionShape::Unknown;
	int32 PrimitiveIndex = -1;
	float HitT = FLT_MAX;

	bool IsValid() const
	{
		return PrimitiveType != EAggCollisionShape::Unknown && PrimitiveIndex >= 0;
	}

	void Clear()
	{
		PrimitiveType = EAggCollisionShape::Unknown;
		PrimitiveIndex = -1;
		HitT = FLT_MAX;
	}
};

/**
 * PhysicsPickingHelper
 *
 * Physics Asset Editor 전용 피킹 헬퍼
 * Body 내 개별 Primitive를 식별하는 ray intersection 기능 제공
 */
namespace PhysicsPickingHelper
{
	/**
	 * Ray-Body intersection with primitive identification.
	 * Tests against all shapes in AggGeom and returns which primitive was hit.
	 *
	 * @param WorldRay - The ray in world space
	 * @param Body - The body setup containing primitives
	 * @param BoneWorldTransform - Transform of the bone this body is attached to
	 * @param OutResult - Output: which primitive was hit and at what T
	 * @return true if any primitive was hit
	 */
	bool IntersectRayBodyWithPrimitive(
		const FRay& WorldRay,
		const UBodySetup* Body,
		const FTransform& BoneWorldTransform,
		FPrimitiveHitResult& OutResult);
}
