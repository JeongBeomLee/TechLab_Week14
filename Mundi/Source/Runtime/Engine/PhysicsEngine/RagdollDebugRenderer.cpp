#include "pch.h"
#include "RagdollDebugRenderer.h"
#include "SkeletalMeshComponent.h"
#include "BodyInstance.h"
#include "BodySetup.h"
#include "AggregateGeom.h"
#include "PhysxConverter.h"
#include "Renderer.h"
#include "PhysicsAsset.h"
#include "SkeletalMesh.h"
#include "VertexData.h"  // FSkeleton

void FRagdollDebugRenderer::RenderSkeletalMeshRagdoll(
    URenderer* Renderer,
    USkeletalMeshComponent* SkelMeshComp,
    const FVector4& BoneColor,
    const FVector4& JointColor)
{
    if (!Renderer || !SkelMeshComp) return;

    const TArray<FBodyInstance*>& Bodies = SkelMeshComp->GetBodies();
    if (Bodies.Num() == 0) return;

    // 스켈레톤 정보 가져오기 (부모 관계 계산용)
    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMesh();
    if (!SkelMesh) return;
    const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
    if (!Skeleton) return;

    // 에디터 모드(시뮬레이션 중이 아닐 때)에서는 본 트랜스폼을 따라가도록
    bool bUsePhysicsTransform = SkelMeshComp->IsSimulatingPhysics();

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // 각 Body의 월드 트랜스폼을 저장 (Joint 렌더링용)
    // Week_Final_5: Bodies[BoneIndex] = 해당 본의 바디 (본 인덱스로 직접 접근)
    TArray<PxTransform> BodyWorldTransforms;
    BodyWorldTransforms.SetNum(Bodies.Num());

    for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
    {
        const FBodyInstance* Body = Bodies[BoneIndex];
        if (!Body || !Body->IsValidBodyInstance() || !Body->BodySetup) continue;

        // Week_Final_5: RigidActor 멤버 직접 사용
        PxRigidActor* RigidBody = Body->RigidActor;
        if (!RigidBody) continue;

        PxTransform WorldTransform;

        if (bUsePhysicsTransform)
        {
            // 시뮬레이션 중: PhysX Body의 실제 위치 사용
            WorldTransform = RigidBody->getGlobalPose();
        }
        else
        {
            // 에디터 모드: 본의 월드 트랜스폼 사용
            FTransform BoneWorldTransform = SkelMeshComp->GetBoneWorldTransform(BoneIndex);
            WorldTransform = PhysxConverter::ToPxTransform(BoneWorldTransform);
        }

        BodyWorldTransforms[BoneIndex] = WorldTransform;

        // UBodySetup의 AggGeom에서 Shape들 렌더링
        RenderAggGeom(Renderer, Body->BodySetup->AggGeom, WorldTransform, BoneColor,
                      StartPoints, EndPoints, Colors);

        // 부모와 연결선 (Joint 시각화)
        // 스켈레톤의 부모 인덱스를 사용해서 부모 바디 찾기
        int32 ParentBoneIndex = (BoneIndex < Skeleton->Bones.Num()) ? Skeleton->Bones[BoneIndex].ParentIndex : -1;
        if (ParentBoneIndex >= 0 && ParentBoneIndex < Bodies.Num())
        {
            const FBodyInstance* ParentBody = Bodies[ParentBoneIndex];
            if (ParentBody && ParentBody->IsValidBodyInstance())
            {
                PxVec3 Start = WorldTransform.p;
                PxVec3 End = BodyWorldTransforms[ParentBoneIndex].p;

                StartPoints.Add(PhysxConverter::ToFVector(Start));
                EndPoints.Add(PhysxConverter::ToFVector(End));
                Colors.Add(JointColor);
            }
        }
    }

    // 라인 렌더링
    if (StartPoints.Num() > 0)
    {
        Renderer->AddLines(StartPoints, EndPoints, Colors);
    }
}

void FRagdollDebugRenderer::RenderAggGeom(
    URenderer* Renderer,
    const FKAggregateGeom& AggGeom,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Sphere Shape들
    for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
    {
        RenderSphere(AggGeom.SphereElems[i], WorldTransform, Color,
                     OutStartPoints, OutEndPoints, OutColors);
    }

    // Box Shape들
    for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
    {
        RenderBox(AggGeom.BoxElems[i], WorldTransform, Color,
                  OutStartPoints, OutEndPoints, OutColors);
    }

    // Capsule Shape들
    for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
    {
        RenderCapsule(AggGeom.SphylElems[i], WorldTransform, Color,
                      OutStartPoints, OutEndPoints, OutColors);
    }

    // Convex Shape들
    for (int32 i = 0; i < AggGeom.ConvexElems.Num(); ++i)
    {
        RenderConvex(AggGeom.ConvexElems[i], WorldTransform, Color,
                     OutStartPoints, OutEndPoints, OutColors);
    }
}

void FRagdollDebugRenderer::RenderSphere(
    const FKSphereElem& Sphere,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 로컬 위치를 월드로 변환
    PxVec3 LocalCenter = PhysxConverter::ToPxVec3(Sphere.Center);
    PxVec3 WorldCenter = WorldTransform.transform(LocalCenter);
    FVector Center = PhysxConverter::ToFVector(WorldCenter);

    // 3개 축의 원으로 구 표현
    const int32 NumSegments = 16;

    // XY 평면 원
    AddCircle(Center, FVector(0, 0, 1), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
    // XZ 평면 원
    AddCircle(Center, FVector(0, 1, 0), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
    // YZ 평면 원
    AddCircle(Center, FVector(1, 0, 0), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
}

void FRagdollDebugRenderer::RenderBox(
    const FKBoxElem& Box,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Week_Final_5: Box.Rotation은 이미 FQuat (Week13은 Euler angles)
    PxQuat LocalPxRot = PhysxConverter::ToPxQuat(Box.Rotation);
    PxTransform LocalTransform(PhysxConverter::ToPxVec3(Box.Center), LocalPxRot);

    // 최종 월드 변환
    PxTransform FinalTransform = WorldTransform * LocalTransform;

    FVector Center = PhysxConverter::ToFVector(FinalTransform.p);
    FQuat Rotation = PhysxConverter::ToFQuat(FinalTransform.q);

    FVector HalfExtent(Box.X, Box.Y, Box.Z);

    // 8개 코너 계산
    FVector Corners[8];
    Corners[0] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z));
    Corners[1] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z));
    Corners[2] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z));
    Corners[3] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z));
    Corners[4] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z));
    Corners[5] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z));
    Corners[6] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z));
    Corners[7] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z));

    // 12개 엣지
    // 아래 면
    OutStartPoints.Add(Corners[0]); OutEndPoints.Add(Corners[1]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[1]); OutEndPoints.Add(Corners[2]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[2]); OutEndPoints.Add(Corners[3]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[3]); OutEndPoints.Add(Corners[0]); OutColors.Add(Color);

    // 위 면
    OutStartPoints.Add(Corners[4]); OutEndPoints.Add(Corners[5]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[5]); OutEndPoints.Add(Corners[6]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[6]); OutEndPoints.Add(Corners[7]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[7]); OutEndPoints.Add(Corners[4]); OutColors.Add(Color);

    // 수직 연결
    OutStartPoints.Add(Corners[0]); OutEndPoints.Add(Corners[4]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[1]); OutEndPoints.Add(Corners[5]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[2]); OutEndPoints.Add(Corners[6]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[3]); OutEndPoints.Add(Corners[7]); OutColors.Add(Color);
}

void FRagdollDebugRenderer::RenderCapsule(
    const FKSphylElem& Capsule,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축)
    // RagdollSystem과 동일한 회전 적용
    FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
    // Week_Final_5: Capsule.Rotation은 이미 FQuat (Week13은 Euler angles)
    FQuat FinalLocalRotation = Capsule.Rotation * BaseRotation;

    PxQuat LocalPxRot = PhysxConverter::ToPxQuat(FinalLocalRotation);
    PxTransform LocalTransform(PhysxConverter::ToPxVec3(Capsule.Center), LocalPxRot);

    // 최종 월드 변환
    PxTransform FinalTransform = WorldTransform * LocalTransform;

    FVector Center = PhysxConverter::ToFVector(FinalTransform.p);
    FQuat Rotation = PhysxConverter::ToFQuat(FinalTransform.q);

    float Radius = Capsule.Radius;
    float HalfLength = Capsule.Length / 2.0f;

    // PhysX 캡슐은 PhysX-X축 방향이 기본
    // 좌표계 변환 후: PhysX X → Engine Y
    // ToFVector: (x,y,z) → (-z, x, y) 이므로 PhysX(1,0,0) → Engine(0,1,0)
    FVector CapsuleAxis = Rotation.RotateVector(FVector(0, 1, 0));  // Engine Y = PhysX X
    FVector AxisY = Rotation.RotateVector(FVector(0, 0, 1));  // 원 그리기용
    FVector AxisZ = Rotation.RotateVector(FVector(1, 0, 0));  // 원 그리기용

    FVector TopCenter = Center + CapsuleAxis * HalfLength;
    FVector BottomCenter = Center - CapsuleAxis * HalfLength;

    const int32 NumSegments = 16;

    // 상단 원 (캡슐 축에 수직인 원)
    AddCircle(TopCenter, CapsuleAxis, Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);

    // 하단 원
    AddCircle(BottomCenter, CapsuleAxis, Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);

    // 세로 라인 (4개)
    for (int32 i = 0; i < 4; ++i)
    {
        float Angle = i * PxPi / 2.0f;
        FVector Offset = (AxisY * std::cos(Angle) + AxisZ * std::sin(Angle)) * Radius;

        FVector TopPoint = TopCenter + Offset;
        FVector BottomPoint = BottomCenter + Offset;

        OutStartPoints.Add(TopPoint);
        OutEndPoints.Add(BottomPoint);
        OutColors.Add(Color);
    }

    // 반구 표현 (반원 2개로 반구처럼 보이게)
    // 상단 반구: 캡슐 축 방향(+)으로 볼록한 반원 2개
    AddSemicircle(TopCenter, CapsuleAxis, AxisY, Radius, NumSegments, Color,
                  OutStartPoints, OutEndPoints, OutColors);
    AddSemicircle(TopCenter, CapsuleAxis, AxisZ, Radius, NumSegments, Color,
                  OutStartPoints, OutEndPoints, OutColors);

    // 하단 반구: 캡슐 축 방향(-)으로 볼록한 반원 2개
    AddSemicircle(BottomCenter, CapsuleAxis * -1.0f, AxisY, Radius, NumSegments, Color,
                  OutStartPoints, OutEndPoints, OutColors);
    AddSemicircle(BottomCenter, CapsuleAxis * -1.0f, AxisZ, Radius, NumSegments, Color,
                  OutStartPoints, OutEndPoints, OutColors);
}

void FRagdollDebugRenderer::AddCircle(
    const FVector& Center,
    const FVector& Normal,
    float Radius,
    int32 NumSegments,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Normal에 수직인 두 벡터 계산
    FVector Tangent, Bitangent;

    if (FMath::Abs(Normal.Z) < 0.9f)
    {
        Tangent = FVector::Cross(Normal, FVector(0, 0, 1)).GetNormalized();
    }
    else
    {
        Tangent = FVector::Cross(Normal, FVector(1, 0, 0)).GetNormalized();
    }
    Bitangent = FVector::Cross(Normal, Tangent).GetNormalized();

    FVector PrevPoint = Center + Tangent * Radius;

    for (int32 i = 1; i <= NumSegments; ++i)
    {
        float Angle = (2.0f * PxPi * i) / NumSegments;
        FVector CurrentPoint = Center + (Tangent * std::cos(Angle) + Bitangent * std::sin(Angle)) * Radius;

        OutStartPoints.Add(PrevPoint);
        OutEndPoints.Add(CurrentPoint);
        OutColors.Add(Color);

        PrevPoint = CurrentPoint;
    }
}

void FRagdollDebugRenderer::AddSemicircle(
    const FVector& Center,
    const FVector& BulgeDir,      // 반구가 볼록하게 나갈 방향
    const FVector& PlaneAxis,     // 반원이 그려질 평면의 한 축
    float Radius,
    int32 NumSegments,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 반원: 0도에서 180도까지 (Pi 라디안)
    int32 HalfSegments = NumSegments / 2;
    if (HalfSegments < 4) HalfSegments = 4;

    FVector PrevPoint = Center + PlaneAxis * Radius;  // 0도 위치

    for (int32 i = 1; i <= HalfSegments; ++i)
    {
        float Angle = (PxPi * i) / HalfSegments;  // 0 ~ Pi

        // 반원 위의 점 계산
        FVector CurrentPoint = Center +
            PlaneAxis * (Radius * std::cos(Angle)) +
            BulgeDir * (Radius * std::sin(Angle));

        OutStartPoints.Add(PrevPoint);
        OutEndPoints.Add(CurrentPoint);
        OutColors.Add(Color);

        PrevPoint = CurrentPoint;
    }
}

// PhysicsAsset 기반 미리보기 렌더링 (Bodies 없이도 작동)
void FRagdollDebugRenderer::RenderPhysicsAssetPreview(
    URenderer* Renderer,
    USkeletalMeshComponent* SkelMeshComp,
    UPhysicsAsset* PhysAsset,
    const FVector4& BoneColor,
    const FVector4& JointColor)
{
    if (!Renderer || !SkelMeshComp || !PhysAsset) return;

    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMesh();
    if (!SkelMesh) return;

    const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
    if (!Skeleton) return;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // 본 이름 → 월드 트랜스폼 매핑 (Joint 렌더링용)
    TMap<FString, PxTransform> BoneWorldTransforms;

    // 각 BodySetup에 대해 렌더링
    for (int32 i = 0; i < PhysAsset->Bodies.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->Bodies[i];
        if (!Setup) continue;

        // BodySetup의 BoneName으로 스켈레톤에서 본 인덱스 찾기
        const auto* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(Setup->BoneName.ToString());
        if (!BoneIndexPtr) continue;

        int32 BoneIndex = *BoneIndexPtr;

        // 본의 월드 트랜스폼 가져오기
        FTransform BoneWorldTransform = SkelMeshComp->GetBoneWorldTransform(BoneIndex);
        PxTransform WorldTransform = PhysxConverter::ToPxTransform(BoneWorldTransform);

        // 본 이름으로 트랜스폼 저장 (Joint 렌더링용)
        BoneWorldTransforms.Add(Setup->BoneName.ToString(), WorldTransform);

        // BodySetup의 AggGeom에서 Shape들 렌더링
        RenderAggGeom(Renderer, Setup->AggGeom, WorldTransform, BoneColor,
                      StartPoints, EndPoints, Colors);
    }

    // Constraint (Joint) 렌더링
    for (int32 i = 0; i < PhysAsset->Constraints.Num(); ++i)
    {
        const FConstraintInstance& Constraint = PhysAsset->Constraints[i];

        // 두 본의 트랜스폼 찾기
        PxTransform* Transform1 = BoneWorldTransforms.Find(Constraint.ConstraintBone1.ToString());
        PxTransform* Transform2 = BoneWorldTransforms.Find(Constraint.ConstraintBone2.ToString());

        if (Transform1 && Transform2)
        {
            FVector Start = PhysxConverter::ToFVector(Transform1->p);
            FVector End = PhysxConverter::ToFVector(Transform2->p);

            StartPoints.Add(Start);
            EndPoints.Add(End);
            Colors.Add(JointColor);
        }
    }

    // 라인 렌더링
    if (StartPoints.Num() > 0)
    {
        Renderer->AddLines(StartPoints, EndPoints, Colors);
    }
}

void FRagdollDebugRenderer::RenderConvex(
    const FKConvexElem& Convex,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Convex의 로컬 변환 (Translation, Rotation, Scale)
    const FTransform& ConvexTransform = Convex.GetTransform();
    PxTransform LocalTransform = PhysxConverter::ToPxTransform(ConvexTransform);

    // 최종 월드 변환
    PxTransform FinalTransform = WorldTransform * LocalTransform;

    // 스케일 가져오기
    const FVector& Scale = ConvexTransform.Scale3D;

    // 삼각형 와이어프레임으로 Convex 렌더링
    for (int32 i = 0; i + 2 < Convex.IndexData.Num(); i += 3)
    {
        int32 i0 = Convex.IndexData[i];
        int32 i1 = Convex.IndexData[i + 1];
        int32 i2 = Convex.IndexData[i + 2];

        if (i0 < Convex.VertexData.Num() && i1 < Convex.VertexData.Num() && i2 < Convex.VertexData.Num())
        {
            // 스케일 적용 후 변환
            FVector ScaledV0 = Convex.VertexData[i0] * Scale;
            FVector ScaledV1 = Convex.VertexData[i1] * Scale;
            FVector ScaledV2 = Convex.VertexData[i2] * Scale;

            PxVec3 LocalV0 = PhysxConverter::ToPxVec3(ScaledV0);
            PxVec3 LocalV1 = PhysxConverter::ToPxVec3(ScaledV1);
            PxVec3 LocalV2 = PhysxConverter::ToPxVec3(ScaledV2);

            FVector V0 = PhysxConverter::ToFVector(FinalTransform.transform(LocalV0));
            FVector V1 = PhysxConverter::ToFVector(FinalTransform.transform(LocalV1));
            FVector V2 = PhysxConverter::ToFVector(FinalTransform.transform(LocalV2));

            OutStartPoints.Add(V0); OutEndPoints.Add(V1); OutColors.Add(Color);
            OutStartPoints.Add(V1); OutEndPoints.Add(V2); OutColors.Add(Color);
            OutStartPoints.Add(V2); OutEndPoints.Add(V0); OutColors.Add(Color);
        }
    }
}
