#pragma once

#include "AggregateGeom.h"
#include "BodySetup.h"
#include "FConstraintSetup.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"

/**
 * PhysicsAsset 디버그 시각화 유틸리티
 * StaticMesh, SkeletalMesh 등에서 공용으로 사용
 */
namespace
{
	constexpr float M_PI = 3.14159265358979f;
	constexpr int32 CircleSegments = 16;
	constexpr int32 CapsuleVerticalLines = 8;
	constexpr int32 ConeSegments = 12;        // 원뿔 세그먼트 수
	constexpr int32 SphereRings = 3;          // 구체 링 수
	constexpr float ConstraintSphereRadius = 0.02f;  // 조인트 구체 크기
}

class FPhysicsDebugUtils
{
public:
	/**
	 * Sphere Shape 면 기반 시각화 생성
	 * UV Sphere 방식으로 위도/경도 라인을 따라 삼각형 생성
	 */
	static void GenerateSphereMesh(
		const FKSphereElem& Sphere,
		const FTransform& BoneTransform,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * Box Shape 면 기반 시각화 생성
	 * 6개 면에 대해 각각 2개의 삼각형 생성
	 */
	static void GenerateBoxMesh(
		const FKBoxElem& Box,
		const FTransform& BoneTransform,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * Capsule(Sphyl) Shape 면 기반 시각화 생성
	 * 실린더 몸통 + 상단/하단 반구
	 */
	static void GenerateCapsuleMesh(
		const FKSphylElem& Capsule,
		const FTransform& BoneTransform,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * UBodySetup의 모든 Shape에 대해 면 기반 메쉬 좌표 생성
	 */
	static void GenerateBodyShapeMesh(
		const UBodySetup* BodySetup,
		const FTransform& BoneTransform,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * PhysicsAsset의 모든 BodySetup에 대해 디버그 메쉬 생성
	 * @param PhysicsAsset 물리 에셋
	 * @param BoneTransforms 각 본의 월드 트랜스폼 배열
	 * @param Color 메쉬 색상
	 * @param OutBatch 출력 삼각형 배치
	 */
	static void GeneratePhysicsAssetDebugMesh(
		class UPhysicsAsset* PhysicsAsset,
		const TArray<FTransform>& BoneTransforms,
		const FVector4& Color,
		FTrianglesBatch& OutBatch);

	// ========== Constraint 시각화 함수들 ==========

	/**
	 * Swing Limit 원뿔(Cone) 면 기반 시각화 생성
	 * @param JointPos 조인트 위치
	 * @param JointRotation 조인트 회전 (부모→자식 방향)
	 * @param Swing1Limit Y축 회전 제한 (degrees)
	 * @param Swing2Limit Z축 회전 제한 (degrees)
	 * @param ConeLength 원뿔 길이
	 */
	static void GenerateSwingConeMesh(
		const FVector& JointPos,
		const FQuat& JointRotation,
		float Swing1Limit,
		float Swing2Limit,
		float ConeLength,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * Twist Limit 부채꼴(Fan) 면 기반 시각화 생성
	 * @param JointPos 조인트 위치
	 * @param JointRotation 조인트 회전
	 * @param TwistMin 최소 회전 (degrees)
	 * @param TwistMax 최대 회전 (degrees)
	 * @param ArcRadius 원호 반지름
	 */
	static void GenerateTwistFanMesh(
		const FVector& JointPos,
		const FQuat& JointRotation,
		float TwistMin,
		float TwistMax,
		float ArcRadius,
		const FVector4& Color,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors);

	/**
	 * Constraint 전체 면 기반 시각화 생성
	 * @param Constraint 제약조건 설정
	 * @param ParentPos 부모 본 위치
	 * @param ChildPos 자식 본 위치
	 * @param JointRotation 조인트 회전
	 * @param bSelected 선택 상태 여부
	 * @param OutVertices 출력 정점 배열
	 * @param OutIndices 출력 인덱스 배열
	 * @param OutColors 출력 색상 배열
	 * @param OutLineBatch 출력 라인 배치 (연결선, 축 마커 등)
	 */
	static void GenerateConstraintMeshVisualization(
		const FConstraintSetup& Constraint,
		const FVector& ParentPos,
		const FVector& ChildPos,
		const FQuat& JointRotation,
		bool bSelected,
		TArray<FVector>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FVector4>& OutColors,
		FLinesBatch& OutLineBatch);

	/**
	 * PhysicsAsset의 모든 Constraint에 대해 디버그 시각화 생성
	 * @param PhysicsAsset 물리 에셋
	 * @param BoneTransforms 각 본의 월드 트랜스폼 배열
	 * @param SelectedConstraintIndex 선택된 Constraint 인덱스 (-1이면 없음)
	 * @param OutTriangleBatch 출력 삼각형 배치
	 * @param OutLineBatch 출력 라인 배치
	 */
	static void GenerateConstraintsDebugMesh(
		class UPhysicsAsset* PhysicsAsset,
		const TArray<FTransform>& BoneTransforms,
		int32 SelectedConstraintIndex,
		FTrianglesBatch& OutTriangleBatch,
		FLinesBatch& OutLineBatch);
};
