#pragma once
#include "Vector.h"
#include "EAggCollisionShape.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UBodySetup;

/**
 * FBodyShapeCalculationResult
 *
 * 본의 정점 분포를 분석한 결과를 담는 구조체
 * 계산(Calculation)과 적용(Application)을 분리하기 위해 사용
 */
struct FBodyShapeCalculationResult
{
	// 계산 성공 여부
	bool bValid = false;

	// 본 로컬 좌표계에서의 Shape 중심
	FVector LocalCenter = FVector::Zero();

	// 본 로컬 좌표계에서의 Shape 회전
	FQuat LocalRotation = FQuat::Identity();

	// Shape 크기 정보
	float Radius = 0.0f;   // Sphere/Capsule의 반지름, Box의 XY 크기
	float Length = 0.0f;   // Capsule/Box의 길이 (Z축 방향)

	// 디버깅 정보
	int32 VertexCount = 0;
	FVector DebugBoneDirection = FVector::Zero();
};

/**
 * FBodyShapeCalculator
 *
 * Physics Body Shape 계산 유틸리티 클래스
 * 정점 분포 분석 기반으로 최적의 콜리전 형상을 계산합니다.
 *
 * 특징:
 * - 순수 계산 로직 (side-effect 없음)
 * - 테스트 가능한 구조
 * - UI 미리보기 지원 가능
 */
class FBodyShapeCalculator
{
public:
	/**
	 * 본의 정점 분포를 분석하여 최적의 Shape 파라미터를 계산
	 *
	 * @param BoneIndex 분석할 본의 인덱스
	 * @param Mesh 스켈레탈 메시 (정점 데이터 소스)
	 * @param MeshComp 메시 컴포넌트 (본 트랜스폼 소스)
	 * @param MinBoneSize 최소 본 크기 (이보다 작으면 무시)
	 * @return 계산 결과 (bValid가 false면 실패)
	 */
	static FBodyShapeCalculationResult CalculateFromVertices(
		int32 BoneIndex,
		const USkeletalMesh* Mesh,
		USkeletalMeshComponent* MeshComp,
		float MinBoneSize
	);

	/**
	 * 계산된 결과를 BodySetup에 적용
	 *
	 * @param Body 적용할 BodySetup
	 * @param Result 계산 결과
	 * @param PrimitiveType 생성할 Primitive 타입
	 */
	static void ApplyToBody(
		UBodySetup* Body,
		const FBodyShapeCalculationResult& Result,
		EAggCollisionShape PrimitiveType
	);

	/**
	 * 원스텝으로 계산 + 적용
	 *
	 * @return 성공 여부
	 */
	static bool GenerateBodyShapeFromVertices(
		UBodySetup* Body,
		int32 BoneIndex,
		const USkeletalMesh* Mesh,
		USkeletalMeshComponent* MeshComp,
		EAggCollisionShape PrimitiveType,
		float MinBoneSize
	);

private:
	// 본 방향 벡터 계산 (월드 좌표계)
	static FVector CalculateBoneDirection(
		int32 BoneIndex,
		const class FSkeleton* Skeleton,
		const TArray<FTransform>& BoneWorldTransforms,
		const TArray<TArray<int32>>& ChildrenMap
	);

	// 정점 분포를 분석하여 반지름과 길이 계산
	static void AnalyzeVertexDistribution(
		const TArray<FVector>& WorldVertices,
		const FVector& BoneWorldPos,
		const FVector& WorldBoneDir,
		float& OutRadius,
		float& OutLength,
		FVector& OutVertexWorldCenter
	);
};
