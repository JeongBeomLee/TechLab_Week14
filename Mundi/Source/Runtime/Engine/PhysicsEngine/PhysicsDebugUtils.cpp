#include "pch.h"
#include "PhysicsDebugUtils.h"
#include "PhysicsAsset.h"



void FPhysicsDebugUtils::GenerateSphereMesh(
	const FKSphereElem& Sphere,
	const FTransform& BoneTransform,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Sphere.Center);
	float Radius = Sphere.Radius;

	constexpr int32 NumLatitude = 12;   // 위도 분할 수
	constexpr int32 NumLongitude = 16;  // 경도 분할 수

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 정점 생성 (북극 ~ 남극)
	for (int32 lat = 0; lat <= NumLatitude; ++lat)
	{
		float theta = lat * M_PI / NumLatitude;  // 0 ~ PI
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);

		for (int32 lon = 0; lon <= NumLongitude; ++lon)
		{
			float phi = lon * 2.0f * M_PI / NumLongitude;  // 0 ~ 2*PI
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			FVector LocalPoint(
				Radius * sinTheta * cosPhi,
				Radius * sinTheta * sinPhi,
				Radius * cosTheta
			);

			OutVertices.push_back(Center + LocalPoint);
			OutColors.push_back(Color);
		}
	}

	// 인덱스 생성 (삼각형 스트립)
	for (int32 lat = 0; lat < NumLatitude; ++lat)
	{
		for (int32 lon = 0; lon < NumLongitude; ++lon)
		{
			uint32 Current = BaseVertexIndex + lat * (NumLongitude + 1) + lon;
			uint32 Next = Current + NumLongitude + 1;

			// 첫 번째 삼각형
			OutIndices.push_back(Current);
			OutIndices.push_back(Next);
			OutIndices.push_back(Current + 1);

			// 두 번째 삼각형
			OutIndices.push_back(Current + 1);
			OutIndices.push_back(Next);
			OutIndices.push_back(Next + 1);
		}
	}
}

void FPhysicsDebugUtils::GenerateBoxMesh(
	const FKBoxElem& Box,
	const FTransform& BoneTransform,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
	FQuat Rotation = BoneTransform.Rotation * Box.Rotation;

	// Box.X, Box.Y, Box.Z는 지름이므로 반으로 나눔
	FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);

	// 8개 코너 생성 (dx/dy/dz 패턴)
	constexpr int dx[] = {-1, 1, 1, -1, -1, 1, 1, -1};
	constexpr int dy[] = {-1, -1, 1, 1, -1, -1, 1, 1};
	constexpr int dz[] = {-1, -1, -1, -1, 1, 1, 1, 1};

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 8개 코너 정점 추가
	for (int i = 0; i < 8; ++i)
	{
		FVector Corner = Center + Rotation.RotateVector(
			FVector(HalfExtent.X * dx[i], HalfExtent.Y * dy[i], HalfExtent.Z * dz[i]));
		OutVertices.push_back(Corner);
		OutColors.push_back(Color);
	}

	// 6개 면에 대한 인덱스 (각 면 = 2개 삼각형)
	// 면 순서: -Z, +Z, -Y, +Y, -X, +X
	constexpr int Faces[6][4] = {
		{0, 1, 2, 3},  // 하단 (-Z)
		{4, 7, 6, 5},  // 상단 (+Z)
		{0, 4, 5, 1},  // 전면 (-Y)
		{2, 6, 7, 3},  // 후면 (+Y)
		{0, 3, 7, 4},  // 좌측 (-X)
		{1, 5, 6, 2}   // 우측 (+X)
	};

	for (const auto& Face : Faces)
	{
		// 삼각형 1
		OutIndices.push_back(BaseVertexIndex + Face[0]);
		OutIndices.push_back(BaseVertexIndex + Face[1]);
		OutIndices.push_back(BaseVertexIndex + Face[2]);

		// 삼각형 2
		OutIndices.push_back(BaseVertexIndex + Face[0]);
		OutIndices.push_back(BaseVertexIndex + Face[2]);
		OutIndices.push_back(BaseVertexIndex + Face[3]);
	}
}

void FPhysicsDebugUtils::GenerateCapsuleMesh(
	const FKSphylElem& Capsule,
	const FTransform& BoneTransform,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);
	FQuat Rotation = BoneTransform.Rotation * Capsule.Rotation;

	float HalfLength = Capsule.Length * 0.5f;
	float Radius = Capsule.Radius;

	constexpr int32 NumSegments = 16;  // 원주 분할 수
	constexpr int32 NumRings = 6;      // 반구 링 수

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 캡슐은 Z축 방향 (기존 엔진 규약)
	// 상단 반구 (Z+ 방향)
	for (int32 ring = 0; ring <= NumRings; ++ring)
	{
		float theta = (ring / static_cast<float>(NumRings)) * (M_PI * 0.5f);  // 0 ~ PI/2
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float ringRadius = Radius * sinTheta;
		float z = HalfLength + Radius * cosTheta;

		for (int32 seg = 0; seg <= NumSegments; ++seg)
		{
			float phi = (seg / static_cast<float>(NumSegments)) * 2.0f * M_PI;
			FVector LocalPoint(ringRadius * cos(phi), ringRadius * sin(phi), z);
			FVector WorldPoint = Center + Rotation.RotateVector(LocalPoint);
			OutVertices.push_back(WorldPoint);
			OutColors.push_back(Color);
		}
	}

	// 실린더 몸통 (상단 링 + 하단 링)
	uint32 CylinderBaseIndex = static_cast<uint32>(OutVertices.size());
	for (int32 i = 0; i <= 1; ++i)  // 상단, 하단
	{
		float z = (i == 0) ? HalfLength : -HalfLength;
		for (int32 seg = 0; seg <= NumSegments; ++seg)
		{
			float phi = (seg / static_cast<float>(NumSegments)) * 2.0f * M_PI;
			FVector LocalPoint(Radius * cos(phi), Radius * sin(phi), z);
			FVector WorldPoint = Center + Rotation.RotateVector(LocalPoint);
			OutVertices.push_back(WorldPoint);
			OutColors.push_back(Color);
		}
	}

	// 하단 반구 (Z- 방향)
	uint32 BottomHemiBaseIndex = static_cast<uint32>(OutVertices.size());
	for (int32 ring = 0; ring <= NumRings; ++ring)
	{
		float theta = (M_PI * 0.5f) + (ring / static_cast<float>(NumRings)) * (M_PI * 0.5f);  // PI/2 ~ PI
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float ringRadius = Radius * sinTheta;
		float z = -HalfLength + Radius * cosTheta;

		for (int32 seg = 0; seg <= NumSegments; ++seg)
		{
			float phi = (seg / static_cast<float>(NumSegments)) * 2.0f * M_PI;
			FVector LocalPoint(ringRadius * cos(phi), ringRadius * sin(phi), z);
			FVector WorldPoint = Center + Rotation.RotateVector(LocalPoint);
			OutVertices.push_back(WorldPoint);
			OutColors.push_back(Color);
		}
	}

	// 인덱스 생성 - 상단 반구
	for (int32 ring = 0; ring < NumRings; ++ring)
	{
		for (int32 seg = 0; seg < NumSegments; ++seg)
		{
			uint32 Current = BaseVertexIndex + ring * (NumSegments + 1) + seg;
			uint32 Next = Current + NumSegments + 1;

			OutIndices.push_back(Current);
			OutIndices.push_back(Next);
			OutIndices.push_back(Current + 1);

			OutIndices.push_back(Current + 1);
			OutIndices.push_back(Next);
			OutIndices.push_back(Next + 1);
		}
	}

	// 인덱스 생성 - 실린더 몸통
	for (int32 seg = 0; seg < NumSegments; ++seg)
	{
		uint32 TopCurrent = CylinderBaseIndex + seg;
		uint32 BottomCurrent = CylinderBaseIndex + (NumSegments + 1) + seg;

		OutIndices.push_back(TopCurrent);
		OutIndices.push_back(BottomCurrent);
		OutIndices.push_back(TopCurrent + 1);

		OutIndices.push_back(TopCurrent + 1);
		OutIndices.push_back(BottomCurrent);
		OutIndices.push_back(BottomCurrent + 1);
	}

	// 인덱스 생성 - 하단 반구
	for (int32 ring = 0; ring < NumRings; ++ring)
	{
		for (int32 seg = 0; seg < NumSegments; ++seg)
		{
			uint32 Current = BottomHemiBaseIndex + ring * (NumSegments + 1) + seg;
			uint32 Next = Current + NumSegments + 1;

			OutIndices.push_back(Current);
			OutIndices.push_back(Next);
			OutIndices.push_back(Current + 1);

			OutIndices.push_back(Current + 1);
			OutIndices.push_back(Next);
			OutIndices.push_back(Next + 1);
		}
	}
}

void FPhysicsDebugUtils::GenerateBodyShapeMesh(
	const UBodySetup* BodySetup,
	const FTransform& BoneTransform,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	if (!BodySetup) return;

	// Sphere shapes
	for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
	{
		GenerateSphereMesh(Sphere, BoneTransform, Color, OutVertices, OutIndices, OutColors);
	}

	// Box shapes
	for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
	{
		GenerateBoxMesh(Box, BoneTransform, Color, OutVertices, OutIndices, OutColors);
	}

	// Capsule shapes
	for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
	{
		GenerateCapsuleMesh(Capsule, BoneTransform, Color, OutVertices, OutIndices, OutColors);
	}
}

void FPhysicsDebugUtils::GeneratePhysicsAssetDebugMesh(
	UPhysicsAsset* PhysicsAsset,
	const TArray<FTransform>& BoneTransforms,
	const FVector4& Color,
	FTrianglesBatch& OutBatch)
{
	if (!PhysicsAsset) return;

	OutBatch.Vertices.Empty();
	OutBatch.Indices.Empty();
	OutBatch.Colors.Empty();

	for (int32 BodyIdx = 0; BodyIdx < PhysicsAsset->BodySetups.Num(); ++BodyIdx)
	{
		UBodySetup* BodySetup = PhysicsAsset->BodySetups[BodyIdx];
		if (!BodySetup) continue;

		// BodySetup의 BoneIndex로 해당 본의 트랜스폼 가져오기
		int32 BoneIndex = BodySetup->BoneIndex;
		if (BoneIndex < 0 || BoneIndex >= BoneTransforms.Num())
		{
			continue;
		}

		const FTransform& BoneTransform = BoneTransforms[BoneIndex];
		GenerateBodyShapeMesh(BodySetup, BoneTransform, Color,
			OutBatch.Vertices, OutBatch.Indices, OutBatch.Colors);
	}
}

// ========== Constraint 시각화 구현 ==========

void FPhysicsDebugUtils::GenerateSwingConeMesh(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float Swing1Limit,
	float Swing2Limit,
	float ConeLength,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	// 각도를 라디안으로 변환 (89도로 클램프하여 tan 폭발 방지)
	float Swing1Rad = FMath::Min(Swing1Limit, 89.0f) * M_PI / 180.0f;
	float Swing2Rad = FMath::Min(Swing2Limit, 89.0f) * M_PI / 180.0f;

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 원뿔 꼭지점 (조인트 위치)
	OutVertices.push_back(JointPos);
	OutColors.push_back(Color);
	uint32 ApexIndex = BaseVertexIndex;

	// 원뿔 베이스 점들
	for (int32 i = 0; i <= ConeSegments; ++i)
	{
		float Angle = (i / static_cast<float>(ConeSegments)) * 2.0f * M_PI;

		// 타원형 원뿔 (Swing1 = Y축, Swing2 = Z축)
		float RadiusY = ConeLength * tan(Swing1Rad);
		float RadiusZ = ConeLength * tan(Swing2Rad);

		// 로컬 좌표에서 원뿔 베이스 점
		FVector LocalPoint(
			ConeLength,  // X축 방향이 원뿔 축
			RadiusY * cos(Angle),
			RadiusZ * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		OutVertices.push_back(WorldPoint);
		OutColors.push_back(Color);
	}

	// 원뿔 측면 삼각형 (꼭지점 → 베이스 원 순환)
	for (int32 i = 0; i < ConeSegments; ++i)
	{
		uint32 BaseI = BaseVertexIndex + 1 + i;
		uint32 BaseNext = BaseVertexIndex + 1 + ((i + 1) % (ConeSegments + 1));

		// 삼각형: Apex → BaseI → BaseNext
		OutIndices.push_back(ApexIndex);
		OutIndices.push_back(BaseI);
		OutIndices.push_back(BaseNext);
	}
}

void FPhysicsDebugUtils::GenerateTwistFanMesh(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float TwistMin,
	float TwistMax,
	float ArcRadius,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	// 각도를 라디안으로 변환
	float TwistMinRad = TwistMin * M_PI / 180.0f;
	float TwistMaxRad = TwistMax * M_PI / 180.0f;

	// Twist 각도 범위
	float TwistRange = TwistMaxRad - TwistMinRad;
	int32 NumSegments = FMath::Max(4, static_cast<int32>(TwistRange / (M_PI / 8.0f)));

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 중심점 (조인트 위치)
	OutVertices.push_back(JointPos);
	OutColors.push_back(Color);
	uint32 CenterIndex = BaseVertexIndex;

	// 원호 점들
	for (int32 i = 0; i <= NumSegments; ++i)
	{
		float T = static_cast<float>(i) / static_cast<float>(NumSegments);
		float Angle = TwistMinRad + T * TwistRange;

		// 로컬 좌표에서 원호 점 (X축이 Twist 축)
		FVector LocalPoint(
			0.0f,
			ArcRadius * cos(Angle),
			ArcRadius * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		OutVertices.push_back(WorldPoint);
		OutColors.push_back(Color);
	}

	// 부채꼴 삼각형들 (중심 → 원호 점들)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		uint32 ArcI = BaseVertexIndex + 1 + i;
		uint32 ArcNext = BaseVertexIndex + 1 + i + 1;

		// 삼각형: Center → ArcI → ArcNext
		OutIndices.push_back(CenterIndex);
		OutIndices.push_back(ArcI);
		OutIndices.push_back(ArcNext);
	}
}

void FPhysicsDebugUtils::GenerateConstraintMeshVisualization(
	const FConstraintSetup& Constraint,
	const FVector& ParentPos,
	const FVector& ChildPos,
	const FQuat& JointRotation,
	bool bSelected,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors,
	FLinesBatch& OutLineBatch)
{
	// 색상 설정 (반투명)
	FVector4 SwingColor = bSelected
		? FVector4(0.0f, 1.0f, 0.5f, 0.4f)   // 선택: 밝은 초록
		: FVector4(0.0f, 0.8f, 0.4f, 0.25f);  // 기본: 초록 (25% 투명)
	FVector4 TwistColor = bSelected
		? FVector4(0.5f, 0.5f, 1.0f, 0.4f)   // 선택: 밝은 파랑
		: FVector4(0.3f, 0.3f, 0.8f, 0.25f);  // 기본: 파랑 (25% 투명)
	FVector4 LineColor = bSelected
		? FVector4(1.0f, 1.0f, 0.0f, 1.0f)   // 선택: 노랑
		: FVector4(1.0f, 0.5f, 0.0f, 1.0f);  // 기본: 주황

	// 조인트 위치 = 자식 본 위치
	FVector JointPos = ChildPos;

	// 1. 부모-자식 연결선 (라인으로 유지)
	OutLineBatch.Add(ParentPos, ChildPos, LineColor);

	// 2. 조인트 위치에 십자 마커 (라인)
	float MarkerSize = bSelected ? 0.03f : 0.02f;
	FVector XAxis = JointRotation.RotateVector(FVector(1, 0, 0)) * MarkerSize;
	FVector YAxis = JointRotation.RotateVector(FVector(0, 1, 0)) * MarkerSize;
	FVector ZAxis = JointRotation.RotateVector(FVector(0, 0, 1)) * MarkerSize;
	OutLineBatch.Add(JointPos - XAxis, JointPos + XAxis, FVector4(1, 0, 0, 1));  // X축: 빨강
	OutLineBatch.Add(JointPos - YAxis, JointPos + YAxis, FVector4(0, 1, 0, 1));  // Y축: 초록
	OutLineBatch.Add(JointPos - ZAxis, JointPos + ZAxis, FVector4(0, 0, 1, 1));  // Z축: 파랑

	// 3. 원뿔 길이 계산 (부모-자식 거리 기반)
	float Distance = (ChildPos - ParentPos).Size();
	float ConeLength = FMath::Max(0.02f, Distance * 0.3f);  // 본 거리의 30%

	// 4. Swing Limit 원뿔 면 (각도가 유의미할 때만)
	if (Constraint.Swing1Limit > 1.0f || Constraint.Swing2Limit > 1.0f)
	{
		GenerateSwingConeMesh(JointPos, JointRotation,
			Constraint.Swing1Limit, Constraint.Swing2Limit, ConeLength, SwingColor,
			OutVertices, OutIndices, OutColors);
	}

	// 5. Twist Limit 부채꼴 면 (각도가 유의미할 때만)
	float TwistRange = Constraint.TwistLimitMax - Constraint.TwistLimitMin;
	if (TwistRange > 1.0f && TwistRange < 359.0f)
	{
		float ArcRadius = ConeLength * 0.5f;
		GenerateTwistFanMesh(JointPos, JointRotation,
			Constraint.TwistLimitMin, Constraint.TwistLimitMax, ArcRadius, TwistColor,
			OutVertices, OutIndices, OutColors);
	}
}

void FPhysicsDebugUtils::GenerateConstraintsDebugMesh(
	UPhysicsAsset* PhysicsAsset,
	const TArray<FTransform>& BoneTransforms,
	int32 SelectedConstraintIndex,
	FTrianglesBatch& OutTriangleBatch,
	FLinesBatch& OutLineBatch)
{
	if (!PhysicsAsset) return;

	OutTriangleBatch.Vertices.Empty();
	OutTriangleBatch.Indices.Empty();
	OutTriangleBatch.Colors.Empty();
	OutLineBatch.Clear();

	int32 NumConstraints = static_cast<int32>(PhysicsAsset->ConstraintSetups.size());

	for (int32 i = 0; i < NumConstraints; ++i)
	{
		const FConstraintSetup& Constraint = PhysicsAsset->ConstraintSetups[i];
		if (Constraint.ParentBodyIndex < 0 || Constraint.ChildBodyIndex < 0) continue;
		if (Constraint.ParentBodyIndex >= static_cast<int32>(PhysicsAsset->BodySetups.size())) continue;
		if (Constraint.ChildBodyIndex >= static_cast<int32>(PhysicsAsset->BodySetups.size())) continue;

		UBodySetup* ParentBody = PhysicsAsset->BodySetups[Constraint.ParentBodyIndex];
		UBodySetup* ChildBody = PhysicsAsset->BodySetups[Constraint.ChildBodyIndex];
		if (!ParentBody || !ChildBody) continue;

		// 본 트랜스폼 가져오기
		int32 ParentBoneIndex = ParentBody->BoneIndex;
		int32 ChildBoneIndex = ChildBody->BoneIndex;

		if (ParentBoneIndex < 0 || ParentBoneIndex >= BoneTransforms.Num()) continue;
		if (ChildBoneIndex < 0 || ChildBoneIndex >= BoneTransforms.Num()) continue;

		FVector ParentPos = BoneTransforms[ParentBoneIndex].Translation;
		FVector ChildPos = BoneTransforms[ChildBoneIndex].Translation;

		// 조인트 회전 계산 (부모 → 자식 방향)
		FVector BoneDir = (ChildPos - ParentPos).GetNormalized();
		FQuat JointRotation = FQuat::Identity();
		if (BoneDir.SizeSquared() > 0.0001f)
		{
			// X축(1,0,0)에서 BoneDir로 회전하는 쿼터니언 계산
			FVector DefaultDir(1, 0, 0);
			FVector Cross = FVector::Cross(DefaultDir, BoneDir);
			float Dot = FVector::Dot(DefaultDir, BoneDir);

			if (Dot < -0.9999f)
			{
				// 거의 반대 방향인 경우: Y축 기준 180도 회전
				JointRotation = FQuat::FromAxisAngle(FVector(0, 1, 0), M_PI);
			}
			else
			{
				// 일반적인 경우: 외적을 축으로, 내적으로 각도 계산
				float S = std::sqrt((1.0f + Dot) * 2.0f);
				float InvS = 1.0f / S;
				JointRotation = FQuat(Cross.X * InvS, Cross.Y * InvS, Cross.Z * InvS, S * 0.5f);
			}
		}

		bool bSelected = (SelectedConstraintIndex == i);

		GenerateConstraintMeshVisualization(
			Constraint, ParentPos, ChildPos, JointRotation, bSelected,
			OutTriangleBatch.Vertices, OutTriangleBatch.Indices, OutTriangleBatch.Colors,
			OutLineBatch);
	}
}
