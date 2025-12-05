#pragma once

#include "PhysXPublic.h"

// Week_Final_5 PhysX 변환 함수
// 왼손 좌표계 (Z-up, X-forward) → 오른손 좌표계 (Y-up, Z-forward)

namespace PhysxConverter
{
	// 엔진 → PhysX 벡터 변환
	// 왼손 (X, Y, Z) → 오른손 (Y, Z, -X)
	inline physx::PxVec3 ToPxVec3(const FVector& InVector)
	{
		return physx::PxVec3(InVector.Y, InVector.Z, -InVector.X);
	}

	// 엔진 → PhysX 쿼터니언 변환
	// 쿼터니언은 축 변환 + 회전 방향 보정 (허수부 부호 반전)
	inline physx::PxQuat ToPxQuat(const FQuat& InQuat)
	{
		return physx::PxQuat(-InQuat.Y, -InQuat.Z, InQuat.X, InQuat.W);
	}

	inline physx::PxTransform ToPxTransform(const FTransform& InTransform)
	{
		return physx::PxTransform(ToPxVec3(InTransform.Translation), ToPxQuat(InTransform.Rotation));
	}

	// PhysX → 엔진 벡터 변환
	// 오른손 (x, y, z) → 왼손 (-z, x, y)
	inline FVector ToFVector(const physx::PxVec3& InVector)
	{
		return FVector(-InVector.z, InVector.x, InVector.y);
	}

	// PhysX → 엔진 쿼터니언 변환
	inline FQuat ToFQuat(const physx::PxQuat& InQuat)
	{
		return FQuat(InQuat.z, -InQuat.x, -InQuat.y, InQuat.w);
	}

	inline FTransform ToFTransform(const physx::PxTransform& InTransform)
	{
		return FTransform(ToFVector(InTransform.p), ToFQuat(InTransform.q), FVector(1, 1, 1));
	}
}
