#include "pch.h"
#include "BodyShapeCalculator.h"
#include "BodySetup.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
#include "ShapeElem.h"
#include "SphereElem.h"
#include "BoxElem.h"
#include "SphylElem.h"
#include <algorithm>

constexpr float kBoneWeightThreshold = 0.1f;  // 10% 미만 영향 무시

FBodyShapeCalculationResult FBodyShapeCalculator::CalculateFromVertices(
	int32 BoneIndex,
	const USkeletalMesh* Mesh,
	USkeletalMeshComponent* MeshComp,
	float MinBoneSize
)
{
	FBodyShapeCalculationResult Result;
	Result.bValid = false;

	if (!Mesh || !MeshComp)
	{
		return Result;
	}

	const FSkeletalMeshData* MeshData = Mesh->GetSkeletalMeshData();
	if (!MeshData)
	{
		return Result;
	}

	const FSkeleton* Skeleton = &MeshData->Skeleton;
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton->Bones.size()))
	{
		return Result;
	}

	const int32 BoneCount = static_cast<int32>(Skeleton->Bones.size());

	// ────────────────────────────────────────────────
	// 1단계: 본 월드 트랜스폼 캐싱
	// ────────────────────────────────────────────────
	TArray<FTransform> BoneWorldTransforms;
	BoneWorldTransforms.SetNum(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		BoneWorldTransforms[i] = MeshComp->GetBoneWorldTransform(i);
	}

	// ────────────────────────────────────────────────
	// 2단계: 자식 본 맵 구축
	// ────────────────────────────────────────────────
	TArray<TArray<int32>> ChildrenMap;
	ChildrenMap.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentIdx >= 0 && ParentIdx < BoneCount)
		{
			ChildrenMap[ParentIdx].Add(i);
		}
	}

	// ────────────────────────────────────────────────
	// 3단계: 해당 본의 정점 수집 (월드 좌표)
	// ────────────────────────────────────────────────
	TArray<FVector> WorldVertices;
	WorldVertices.Reserve(MeshData->Vertices.size() / BoneCount);  // 대략 추정

	for (const FSkinnedVertex& V : MeshData->Vertices)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (V.BoneWeights[i] >= kBoneWeightThreshold)
			{
				uint32 BoneIdx = V.BoneIndices[i];
				if (BoneIdx == static_cast<uint32>(BoneIndex))
				{
					WorldVertices.Add(V.Position);
					break;  // 동일 정점을 중복 추가하지 않음
				}
			}
		}
	}

	// 정점이 부족하면 실패
	if (WorldVertices.Num() < 3)
	{
		return Result;
	}

	Result.VertexCount = WorldVertices.Num();

	// ────────────────────────────────────────────────
	// 4단계: 본 방향 계산
	// ────────────────────────────────────────────────
	const FTransform& BoneTransform = BoneWorldTransforms[BoneIndex];
	const FVector& BoneWorldPos = BoneTransform.Translation;
	const FQuat& BoneWorldRot = BoneTransform.Rotation;
	const FQuat BoneInvRot = BoneWorldRot.Inverse();

	FVector WorldBoneDir = CalculateBoneDirection(BoneIndex, Skeleton, BoneWorldTransforms, ChildrenMap);
	Result.DebugBoneDirection = WorldBoneDir;

	// ────────────────────────────────────────────────
	// 5단계: 정점 분포 분석
	// ────────────────────────────────────────────────
	FVector VertexWorldCenter;
	AnalyzeVertexDistribution(
		WorldVertices,
		BoneWorldPos,
		WorldBoneDir,
		Result.Radius,
		Result.Length,
		VertexWorldCenter
	);

	// ────────────────────────────────────────────────
	// 6단계: 본 로컬 좌표로 변환
	// ────────────────────────────────────────────────
	// Shape.Center: 정점 중심을 본 로컬 좌표로 변환
	Result.LocalCenter = BoneInvRot.RotateVector(VertexWorldCenter - BoneWorldPos);

	// Shape.Rotation: 본 방향을 본 로컬로 변환하여 회전 계산
	FVector LocalBoneDir = BoneInvRot.RotateVector(WorldBoneDir);

	// 캡슐/박스는 기본 Z축 방향 -> LocalBoneDir 방향으로 회전 필요
	FVector DefaultAxis(0, 0, 1);
	float DotVal = FVector::Dot(DefaultAxis, LocalBoneDir);

	if (DotVal > 0.9999f)
	{
		Result.LocalRotation = FQuat::Identity();
	}
	else if (DotVal < -0.9999f)
	{
		Result.LocalRotation = FQuat::FromAxisAngle(FVector(1, 0, 0), PI);
	}
	else
	{
		FVector CrossVal = FVector::Cross(DefaultAxis, LocalBoneDir);
		float W = 1.0f + DotVal;
		Result.LocalRotation = FQuat(CrossVal.X, CrossVal.Y, CrossVal.Z, W).GetNormalized();
	}

	// ────────────────────────────────────────────────
	// 7단계: Min Bone Size 체크
	// ────────────────────────────────────────────────
	float MaxExtent = std::max(Result.Length, Result.Radius * 2.0f);
	if (MaxExtent < MinBoneSize)
	{
		return Result;  // bValid = false
	}

	Result.bValid = true;
	return Result;
}

void FBodyShapeCalculator::ApplyToBody(
	UBodySetup* Body,
	const FBodyShapeCalculationResult& Result,
	EAggCollisionShape PrimitiveType
)
{
	if (!Body || !Result.bValid)
	{
		return;
	}

	// 기존 Shape 모두 삭제
	Body->AggGeom.SphereElems.Empty();
	Body->AggGeom.BoxElems.Empty();
	Body->AggGeom.SphylElems.Empty();

	switch (PrimitiveType)
	{
	case EAggCollisionShape::Sphere:
	{
		FKSphereElem Sphere;
		Sphere.Center = Result.LocalCenter;
		Sphere.Radius = std::max(Result.Length * 0.5f, Result.Radius);
		Body->AggGeom.SphereElems.Add(Sphere);
		break;
	}

	case EAggCollisionShape::Sphyl:  // Capsule
	{
		FKSphylElem Capsule;
		Capsule.Center = Result.LocalCenter;
		Capsule.Rotation = Result.LocalRotation;

		// 캡슐 길이 조정 (양 끝 반구 부분 제외)
		Capsule.Length = std::max(0.0f, Result.Length - 2.0f * Result.Radius);
		Capsule.Radius = std::max(0.001f, Result.Radius);

		Body->AggGeom.SphylElems.Add(Capsule);
		break;
	}

	case EAggCollisionShape::Box:
	{
		FKBoxElem Box;
		Box.Center = Result.LocalCenter;
		Box.Rotation = Result.LocalRotation;

		// 본 방향 = Z축, 나머지 = Radius
		Box.X = std::max(0.001f, Result.Radius * 2.0f);
		Box.Y = std::max(0.001f, Result.Radius * 2.0f);
		Box.Z = std::max(0.001f, Result.Length);

		Body->AggGeom.BoxElems.Add(Box);
		break;
	}

	default:
		break;
	}
}

bool FBodyShapeCalculator::GenerateBodyShapeFromVertices(
	UBodySetup* Body,
	int32 BoneIndex,
	const USkeletalMesh* Mesh,
	USkeletalMeshComponent* MeshComp,
	EAggCollisionShape PrimitiveType,
	float MinBoneSize
)
{
	FBodyShapeCalculationResult Result = CalculateFromVertices(BoneIndex, Mesh, MeshComp, MinBoneSize);

	if (!Result.bValid)
	{
		return false;
	}

	ApplyToBody(Body, Result, PrimitiveType);
	return true;
}

FVector FBodyShapeCalculator::CalculateBoneDirection(
	int32 BoneIndex,
	const FSkeleton* Skeleton,
	const TArray<FTransform>& BoneWorldTransforms,
	const TArray<TArray<int32>>& ChildrenMap
)
{
	FVector WorldBoneDir(0, 0, 1);  // 기본 방향
	const FBone& Bone = Skeleton->Bones[BoneIndex];
	const FVector& BoneWorldPos = BoneWorldTransforms[BoneIndex].Translation;

	if (!ChildrenMap[BoneIndex].IsEmpty())
	{
		// 자식이 있으면: 현재 본 → 자식 본 방향
		int32 ChildIdx = ChildrenMap[BoneIndex][0];
		FVector ChildWorldPos = BoneWorldTransforms[ChildIdx].Translation;
		FVector Dir = ChildWorldPos - BoneWorldPos;
		if (Dir.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			WorldBoneDir = Dir.GetNormalized();
		}
	}
	else if (Bone.ParentIndex >= 0)
	{
		// 리프 본: 부모 본 → 현재 본 방향
		FVector ParentWorldPos = BoneWorldTransforms[Bone.ParentIndex].Translation;
		FVector Dir = BoneWorldPos - ParentWorldPos;
		if (Dir.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			WorldBoneDir = Dir.GetNormalized();
		}
	}

	return WorldBoneDir;
}

void FBodyShapeCalculator::AnalyzeVertexDistribution(
	const TArray<FVector>& WorldVertices,
	const FVector& BoneWorldPos,
	const FVector& WorldBoneDir,
	float& OutRadius,
	float& OutLength,
	FVector& OutVertexWorldCenter
)
{
	float MinProj = FLT_MAX, MaxProj = -FLT_MAX;
	TArray<float> RadialDistances;
	RadialDistances.Reserve(WorldVertices.Num());
	FVector VertexCenterSum = FVector::Zero();

	for (const FVector& WorldV : WorldVertices)
	{
		VertexCenterSum += WorldV;

		// 본 위치 기준 상대 위치
		FVector RelativePos = WorldV - BoneWorldPos;

		// 본 방향으로 투영 (길이)
		float Proj = FVector::Dot(RelativePos, WorldBoneDir);
		MinProj = std::min(MinProj, Proj);
		MaxProj = std::max(MaxProj, Proj);

		// 본 방향 수직 거리 (반경) - 이상치 제거를 위해 배열에 수집
		FVector Radial = RelativePos - WorldBoneDir * Proj;
		RadialDistances.Add(Radial.Size());
	}

	// 90th percentile로 Radius 계산 (이상치 제거)
	std::sort(RadialDistances.GetData(), RadialDistances.GetData() + RadialDistances.Num());
	int32 PercentileIndex = static_cast<int32>(RadialDistances.Num() * 0.66f);
	PercentileIndex = std::min(PercentileIndex, RadialDistances.Num() - 1);

	OutRadius = RadialDistances[PercentileIndex];
	OutLength = MaxProj - MinProj;
	OutVertexWorldCenter = VertexCenterSum / static_cast<float>(WorldVertices.Num());
}
