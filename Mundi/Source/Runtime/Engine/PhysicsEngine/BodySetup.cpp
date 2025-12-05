#include "pch.h"
#include "BodySetup.h"
#include "BodyInstance.h"
#include "StaticMesh.h"

#include "PhysicalMaterial.h"
#include "ConvexElem.h"
#include "TriangleMeshElem.h"
#include "TriangleMeshSource.h"
#include "ImGui/imgui.h"

UBodySetup::UBodySetup()
    : BoneName("None")
{
} 

UBodySetup::~UBodySetup()
{
}

void UBodySetup::AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, const FVector& Scale3D, PxRigidActor* PDestActor, UPhysicalMaterial* InPhysicalMaterial)
{
    if (!PDestActor || !GPhysXSDK)
    {
        return;
    }

    PxMaterial* PhysMaterialToUse;
    if (!InPhysicalMaterial)
    {
        assert(GPhysicalMaterial);
        PhysMaterialToUse = GPhysicalMaterial->GetPxMaterial();
    }
    else
    {
        PhysMaterialToUse = InPhysicalMaterial->GetPxMaterial();
    }

    // ====================================================================
    // 1. Box Elements
    // ====================================================================
    for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
    {
        if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        PxBoxGeometry BoxGeom = BoxElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(BoxGeom, *PhysMaterialToUse, true);

        if (NewShape)
        {
            FTransform ElemTM = BoxElem.GetTransform();
            
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            PxTransform PxLocalPose(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation));
            NewShape->setLocalPose(PxLocalPose);

            if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }
            else if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
            }
            else if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }

            PDestActor->attachShape(*NewShape);

            NewShape->release();
        }
    }

    // ====================================================================
    // 2. Sphere Elements
    // ====================================================================
    for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
    {
        if (SphereElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

        PxSphereGeometry SphereGeom = SphereElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(SphereGeom, *PhysMaterialToUse, true);
        if (NewShape)
        {
            FTransform ElemTM = SphereElem.GetTransform();
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            NewShape->setLocalPose(PxTransform(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation)));

            NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            PDestActor->attachShape(*NewShape);
            NewShape->release();
        }
    }

    // ====================================================================
    // 3. Sphyl (Capsule) Elements
    // ====================================================================
    for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
    {
        if (SphylElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

        PxCapsuleGeometry CapsuleGeom = SphylElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(CapsuleGeom, *PhysMaterialToUse, true);
        if (NewShape)
        {
            FTransform ElemTM = SphylElem.GetTransform();
            
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            // [중요] 회전 보정 (Week13 방식)
            // 엔진: 캡슐 길이 방향 = Z축
            // PhysX: 캡슐 길이 방향 = X축
            // 엔진 공간에서 Z→X 회전을 적용한 후 축변환
            FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
            FQuat UserRotation = ElemTM.Rotation;
            FQuat FinalRotation = UserRotation * BaseRotation;

            PxTransform LocalPose(U2PVector(ScaledCenter), U2PQuat(FinalRotation));
            NewShape->setLocalPose(LocalPose);

            NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            PDestActor->attachShape(*NewShape);
            NewShape->release();
        }
    }

    // ====================================================================
    // 4. Convex Elements
    // ====================================================================
    for (FKConvexElem& ConvexElem : AggGeom.ConvexElems)
    {
        if (ConvexElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

        // ConvexMesh가 없으면 CookedData로부터 생성 시도
        if (!ConvexElem.ConvexMesh && ConvexElem.CookedData.Num() > 0)
        {
            ConvexElem.CreateConvexMeshFromCookedData();
        }

        if (!ConvexElem.ConvexMesh)
        {
            UE_LOG("UBodySetup::AddShapesToRigidActor - Skipping invalid ConvexElem (no mesh)");
            continue;
        }

        PxConvexMeshGeometry ConvexGeom = ConvexElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(ConvexGeom, *PhysMaterialToUse, true);
        if (NewShape)
        {
            FTransform ElemTM = ConvexElem.GetTransform();

            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            NewShape->setLocalPose(PxTransform(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation)));

            NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            PDestActor->attachShape(*NewShape);
            NewShape->release();
        }
    }

    // ====================================================================
    // 5. Triangle Mesh Elements (Static 전용)
    // ====================================================================
    // TriangleMesh는 Static Actor에서만 사용 가능
    if (OwningInstance && OwningInstance->IsStatic())
    {
        for (FKTriangleMeshElem& TriMeshElem : AggGeom.TriangleMeshElems)
        {
            if (TriMeshElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

            // TriangleMesh가 없으면 CookedData로부터 생성 시도
            if (!TriMeshElem.TriangleMesh && TriMeshElem.CookedData.Num() > 0)
            {
                TriMeshElem.CreateTriangleMeshFromCookedData();
            }

            if (!TriMeshElem.TriangleMesh)
            {
                UE_LOG("UBodySetup::AddShapesToRigidActor - Skipping invalid TriangleMeshElem (no mesh)");
                continue;
            }

            PxTriangleMeshGeometry TriMeshGeom = TriMeshElem.GetPxGeometry(Scale3D);

            PxShape* NewShape = GPhysXSDK->createShape(TriMeshGeom, *PhysMaterialToUse, true);
            if (NewShape)
            {
                FTransform ElemTM = TriMeshElem.GetTransform();

                FVector ScaledCenter;
                ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
                ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
                ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

                NewShape->setLocalPose(PxTransform(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation)));

                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

                PDestActor->attachShape(*NewShape);
                NewShape->release();
            }
        }
    }
}

void UBodySetup::AddBoxTest(const FVector& Extent)
{
    FKBoxElem Box;
    Box.X = Extent.X;
    Box.Y = Extent.Y;
    Box.Z = Extent.Z;
    AggGeom.BoxElems.Add(Box);
}

void UBodySetup::AddSphereTest(float Radius)
{
    FKSphereElem Sphere(Radius);
    AggGeom.SphereElems.Add(Sphere);
}

void UBodySetup::AddCapsuleTest(float Radius, float Length)
{
    FKSphylElem Capsule(Radius, Length);
    AggGeom.SphylElems.Add(Capsule);
}

void UBodySetup::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// PhysicalMaterial은 리소스 참조이므로 포인터 공유 (복제 안함)
	// AggGeom은 값 타입이므로 얕은 복사로 이미 복제됨

	// OwningMesh는 새 소유자가 설정해야 함 (원본 참조 방지)
	OwningMesh = nullptr;
}

void UBodySetup::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// UPROPERTY로 선언된 필드들 자동 직렬화 (BoneName, BoneIndex, PhysicalMaterial)
	UObject::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// CollisionComplexity 로드
		FString ComplexityStr;
		FJsonSerializer::ReadString(InOutHandle, "CollisionComplexity", ComplexityStr, "UseSimple", false);
		CollisionComplexity = (ComplexityStr == "UseComplexAsSimple")
			? ECollisionComplexity::UseComplexAsSimple
			: ECollisionComplexity::UseSimple;

		// AggGeom 로드 (수동 처리)
		AggGeom.BoxElems.Empty();
		AggGeom.SphereElems.Empty();
		AggGeom.SphylElems.Empty();
		AggGeom.ConvexElems.Empty();
		AggGeom.TriangleMeshElems.Empty();

		JSON AggGeomJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "AggGeom", AggGeomJson, JSON(), false))
		{
			// Sphere Elements
			JSON SphereElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "SphereElems", SphereElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(SphereElemsJson.size()); ++i)
				{
					const JSON& SphereJson = SphereElemsJson.at(i);
					FKSphereElem Sphere;
					FJsonSerializer::ReadVector(SphereJson, "Center", Sphere.Center, FVector::Zero(), false);
					FJsonSerializer::ReadFloat(SphereJson, "Radius", Sphere.Radius, 0.5f, false);
					AggGeom.SphereElems.Add(Sphere);
				}
			}

			// Box Elements
			JSON BoxElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "BoxElems", BoxElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(BoxElemsJson.size()); ++i)
				{
					const JSON& BoxJson = BoxElemsJson.at(i);
					FKBoxElem Box;
					FJsonSerializer::ReadVector(BoxJson, "Center", Box.Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(BoxJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					Box.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);
					FJsonSerializer::ReadFloat(BoxJson, "X", Box.X, 1.f, false);
					FJsonSerializer::ReadFloat(BoxJson, "Y", Box.Y, 1.f, false);
					FJsonSerializer::ReadFloat(BoxJson, "Z", Box.Z, 1.f, false);
					AggGeom.BoxElems.Add(Box);
				}
			}

			// Sphyl (Capsule) Elements
			JSON SphylElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "SphylElems", SphylElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(SphylElemsJson.size()); ++i)
				{
					const JSON& SphylJson = SphylElemsJson.at(i);
					FKSphylElem Sphyl;
					FJsonSerializer::ReadVector(SphylJson, "Center", Sphyl.Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(SphylJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					Sphyl.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);
					FJsonSerializer::ReadFloat(SphylJson, "Radius", Sphyl.Radius, 0.5f, false);
					FJsonSerializer::ReadFloat(SphylJson, "Length", Sphyl.Length, 1.f, false);
					AggGeom.SphylElems.Add(Sphyl);
				}
			}

			// Convex Elements
			JSON ConvexElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "ConvexElems", ConvexElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(ConvexElemsJson.size()); ++i)
				{
					const JSON& ConvexJson = ConvexElemsJson.at(i);
					FKConvexElem Convex;

					// Source (기본값: Mesh)
					FString SourceStr;
					FJsonSerializer::ReadString(ConvexJson, "Source", SourceStr, "Mesh", false);
					Convex.Source = (SourceStr == "Custom") ? EConvexSource::Custom : EConvexSource::Mesh;

					// Transform
					FVector Center;
					FJsonSerializer::ReadVector(ConvexJson, "Center", Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(ConvexJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					Convex.ElemTransform.Translation = Center;
					Convex.ElemTransform.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);

					// VertexData (Source == Custom일 때만)
					if (Convex.Source == EConvexSource::Custom)
					{
						JSON VertexDataJson;
						if (FJsonSerializer::ReadArray(ConvexJson, "VertexData", VertexDataJson, JSON(), false))
						{
							for (int32 j = 0; j < static_cast<int32>(VertexDataJson.size()); ++j)
							{
								FVector V;
								V.X = VertexDataJson.at(j).at(0).ToFloat();
								V.Y = VertexDataJson.at(j).at(1).ToFloat();
								V.Z = VertexDataJson.at(j).at(2).ToFloat();
								Convex.VertexData.Add(V);
							}
						}
						Convex.UpdateBounds();

						// VertexData로부터 ConvexMesh 생성 (PhysX Cooking)
						if (Convex.VertexData.Num() >= 4)
						{
							Convex.CreateConvexMesh();
						}
					}
					// Source == Mesh일 때는 StaticMesh에서 메시 정점으로 ConvexMesh 생성

					AggGeom.ConvexElems.Add(Convex);
				}
			}

			// TriangleMesh Elements
			JSON TriMeshElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "TriangleMeshElems", TriMeshElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(TriMeshElemsJson.size()); ++i)
				{
					const JSON& TriMeshJson = TriMeshElemsJson.at(i);
					FKTriangleMeshElem TriMesh;

					// Source (기본값: Mesh)
					FString SourceStr;
					FJsonSerializer::ReadString(TriMeshJson, "Source", SourceStr, "Mesh", false);
					TriMesh.Source = (SourceStr == "Custom") ? ETriangleMeshSource::Custom : ETriangleMeshSource::Mesh;

					// Transform
					FVector Center;
					FJsonSerializer::ReadVector(TriMeshJson, "Center", Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(TriMeshJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					TriMesh.ElemTransform.Translation = Center;
					TriMesh.ElemTransform.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);

					// VertexData/IndexData (Source == Custom일 때만)
					if (TriMesh.Source == ETriangleMeshSource::Custom)
					{
						JSON VertexDataJson;
						if (FJsonSerializer::ReadArray(TriMeshJson, "VertexData", VertexDataJson, JSON(), false))
						{
							for (int32 j = 0; j < static_cast<int32>(VertexDataJson.size()); ++j)
							{
								FVector V;
								V.X = VertexDataJson.at(j).at(0).ToFloat();
								V.Y = VertexDataJson.at(j).at(1).ToFloat();
								V.Z = VertexDataJson.at(j).at(2).ToFloat();
								TriMesh.VertexData.Add(V);
							}
						}

						JSON IndexDataJson;
						if (FJsonSerializer::ReadArray(TriMeshJson, "IndexData", IndexDataJson, JSON(), false))
						{
							for (int32 j = 0; j < static_cast<int32>(IndexDataJson.size()); ++j)
							{
								TriMesh.IndexData.Add(static_cast<uint32>(IndexDataJson.at(j).ToInt()));
							}
						}

						TriMesh.UpdateBounds();

						// VertexData/IndexData로부터 TriangleMesh 생성
						if (TriMesh.VertexData.Num() >= 3 && TriMesh.IndexData.Num() >= 3)
						{
							TriMesh.CreateTriangleMesh();
						}
					}
					// Source == Mesh일 때는 StaticMesh에서 자동 생성

					AggGeom.TriangleMeshElems.Add(TriMesh);
				}
			}
		}
	}
	else
	{
		// CollisionComplexity 저장
		InOutHandle["CollisionComplexity"] = (CollisionComplexity == ECollisionComplexity::UseComplexAsSimple)
			? "UseComplexAsSimple" : "UseSimple";
		// AggGeom 저장 (수동 처리)
		JSON AggGeomJson = JSON::Make(JSON::Class::Object);

		// Sphere Elements
		JSON SphereElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKSphereElem& Sphere : AggGeom.SphereElems)
		{
			JSON SphereJson = JSON::Make(JSON::Class::Object);
			SphereJson["Center"] = FJsonSerializer::VectorToJson(Sphere.Center);
			SphereJson["Radius"] = Sphere.Radius;
			SphereElemsJson.append(SphereJson);
		}
		AggGeomJson["SphereElems"] = SphereElemsJson;

		// Box Elements
		JSON BoxElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKBoxElem& Box : AggGeom.BoxElems)
		{
			JSON BoxJson = JSON::Make(JSON::Class::Object);
			BoxJson["Center"] = FJsonSerializer::VectorToJson(Box.Center);
			BoxJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z, Box.Rotation.W));
			BoxJson["X"] = Box.X;
			BoxJson["Y"] = Box.Y;
			BoxJson["Z"] = Box.Z;
			BoxElemsJson.append(BoxJson);
		}
		AggGeomJson["BoxElems"] = BoxElemsJson;

		// Sphyl (Capsule) Elements
		JSON SphylElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
		{
			JSON SphylJson = JSON::Make(JSON::Class::Object);
			SphylJson["Center"] = FJsonSerializer::VectorToJson(Sphyl.Center);
			SphylJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(Sphyl.Rotation.X, Sphyl.Rotation.Y, Sphyl.Rotation.Z, Sphyl.Rotation.W));
			SphylJson["Radius"] = Sphyl.Radius;
			SphylJson["Length"] = Sphyl.Length;
			SphylElemsJson.append(SphylJson);
		}
		AggGeomJson["SphylElems"] = SphylElemsJson;

		// Convex Elements
		JSON ConvexElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKConvexElem& Convex : AggGeom.ConvexElems)
		{
			JSON ConvexJson = JSON::Make(JSON::Class::Object);

			// Source
			ConvexJson["Source"] = (Convex.Source == EConvexSource::Custom) ? "Custom" : "Mesh";

			ConvexJson["Center"] = FJsonSerializer::VectorToJson(Convex.ElemTransform.Translation);
			ConvexJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(
				Convex.ElemTransform.Rotation.X,
				Convex.ElemTransform.Rotation.Y,
				Convex.ElemTransform.Rotation.Z,
				Convex.ElemTransform.Rotation.W));

			// VertexData (Source == Custom일 때만 저장)
			if (Convex.Source == EConvexSource::Custom)
			{
				JSON VertexDataJson = JSON::Make(JSON::Class::Array);
				for (const FVector& V : Convex.VertexData)
				{
					JSON VJson = JSON::Make(JSON::Class::Array);
					VJson.append(V.X);
					VJson.append(V.Y);
					VJson.append(V.Z);
					VertexDataJson.append(VJson);
				}
				ConvexJson["VertexData"] = VertexDataJson;
			}

			// CookedData는 별도 바이너리 파일로 저장 (JSON에는 저장 안함)

			ConvexElemsJson.append(ConvexJson);
		}
		AggGeomJson["ConvexElems"] = ConvexElemsJson;

		// TriangleMesh Elements
		JSON TriMeshElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKTriangleMeshElem& TriMesh : AggGeom.TriangleMeshElems)
		{
			JSON TriMeshJson = JSON::Make(JSON::Class::Object);

			// Source
			TriMeshJson["Source"] = (TriMesh.Source == ETriangleMeshSource::Custom) ? "Custom" : "Mesh";

			TriMeshJson["Center"] = FJsonSerializer::VectorToJson(TriMesh.ElemTransform.Translation);
			TriMeshJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(
				TriMesh.ElemTransform.Rotation.X,
				TriMesh.ElemTransform.Rotation.Y,
				TriMesh.ElemTransform.Rotation.Z,
				TriMesh.ElemTransform.Rotation.W));

			// VertexData/IndexData (Source == Custom일 때만 저장)
			if (TriMesh.Source == ETriangleMeshSource::Custom)
			{
				JSON VertexDataJson = JSON::Make(JSON::Class::Array);
				for (const FVector& V : TriMesh.VertexData)
				{
					JSON VJson = JSON::Make(JSON::Class::Array);
					VJson.append(V.X);
					VJson.append(V.Y);
					VJson.append(V.Z);
					VertexDataJson.append(VJson);
				}
				TriMeshJson["VertexData"] = VertexDataJson;

				JSON IndexDataJson = JSON::Make(JSON::Class::Array);
				for (uint32 Idx : TriMesh.IndexData)
				{
					IndexDataJson.append(static_cast<int32>(Idx));
				}
				TriMeshJson["IndexData"] = IndexDataJson;
			}

			// CookedData는 별도 바이너리 파일로 저장 (JSON에는 저장 안함)

			TriMeshElemsJson.append(TriMeshJson);
		}
		AggGeomJson["TriangleMeshElems"] = TriMeshElemsJson;

		InOutHandle["AggGeom"] = AggGeomJson;
	}
}

bool UBodySetup::RenderShapesUI(UBodySetup* BodySetup, bool& OutSaveRequested)
{
	// TODO: PropertyRenderer에 USTRUCT 리플렉션 렌더링 지원 후 이 함수 제거
	// 현재는 수동 ImGui 렌더링 사용

	if (!BodySetup)
	{
		return false;
	}

	bool bChanged = false;
	OutSaveRequested = false;

	int32 SphereCount = BodySetup->AggGeom.SphereElems.Num();
	int32 BoxCount = BodySetup->AggGeom.BoxElems.Num();
	int32 SphylCount = BodySetup->AggGeom.SphylElems.Num();
	int32 ConvexCount = BodySetup->AggGeom.ConvexElems.Num();
	int32 TriMeshCount = BodySetup->AggGeom.TriangleMeshElems.Num();
	int32 TotalShapes = SphereCount + BoxCount + SphylCount + ConvexCount + TriMeshCount;

	ImGui::Text("Total Shapes: %d", TotalShapes);

	// Shape 추가 버튼
	if (ImGui::Button("Add Sphere"))
	{
		FKSphereElem NewSphere;
		NewSphere.Radius = 0.5f;
		BodySetup->AggGeom.SphereElems.Add(NewSphere);
		bChanged = true;
		OutSaveRequested = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Box"))
	{
		FKBoxElem NewBox(0.5f, 0.5f, 0.5f);
		BodySetup->AggGeom.BoxElems.Add(NewBox);
		bChanged = true;
		OutSaveRequested = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Capsule"))
	{
		FKSphylElem NewCapsule(0.5f, 1.0f);
		BodySetup->AggGeom.SphylElems.Add(NewCapsule);
		bChanged = true;
		OutSaveRequested = true;
	}

	ImGui::Separator();

	// Sphere 렌더링
	for (int32 i = 0; i < SphereCount; ++i)
	{
		ImGui::PushID(i);
		FKSphereElem& Sphere = BodySetup->AggGeom.SphereElems[i];

		if (ImGui::TreeNode("Sphere", "Sphere [%d]", i))
		{
			float Center[3] = { Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z };
			if (ImGui::DragFloat3("Center", Center, 0.01f))
			{
				Sphere.Center = FVector(Center[0], Center[1], Center[2]);
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::DragFloat("Radius", &Sphere.Radius, 0.01f, 0.01f, 100.0f))
			{
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::Button("Remove"))
			{
				BodySetup->AggGeom.SphereElems.RemoveAt(i);
				bChanged = true;
				OutSaveRequested = true;
				ImGui::TreePop();
				ImGui::PopID();
				return bChanged;  // 배열 변경 후 즉시 반환
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Box 렌더링
	for (int32 i = 0; i < BoxCount; ++i)
	{
		ImGui::PushID(SphereCount + i);
		FKBoxElem& Box = BodySetup->AggGeom.BoxElems[i];

		if (ImGui::TreeNode("Box", "Box [%d]", i))
		{
			float Center[3] = { Box.Center.X, Box.Center.Y, Box.Center.Z };
			if (ImGui::DragFloat3("Center", Center, 0.01f))
			{
				Box.Center = FVector(Center[0], Center[1], Center[2]);
				bChanged = true;
				OutSaveRequested = true;
			}
			FVector RotEuler = Box.Rotation.ToEulerZYXDeg();
			float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
			if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
			{
				Box.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
				bChanged = true;
				OutSaveRequested = true;
			}
			float Extents[3] = { Box.X, Box.Y, Box.Z };
			if (ImGui::DragFloat3("Size (XYZ)", Extents, 0.01f, 0.01f, 100.0f))
			{
				Box.X = Extents[0];
				Box.Y = Extents[1];
				Box.Z = Extents[2];
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::Button("Remove"))
			{
				BodySetup->AggGeom.BoxElems.RemoveAt(i);
				bChanged = true;
				OutSaveRequested = true;
				ImGui::TreePop();
				ImGui::PopID();
				return bChanged;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Capsule 렌더링
	for (int32 i = 0; i < SphylCount; ++i)
	{
		ImGui::PushID(SphereCount + BoxCount + i);
		FKSphylElem& Capsule = BodySetup->AggGeom.SphylElems[i];

		if (ImGui::TreeNode("Capsule", "Capsule [%d]", i))
		{
			float Center[3] = { Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z };
			if (ImGui::DragFloat3("Center", Center, 0.01f))
			{
				Capsule.Center = FVector(Center[0], Center[1], Center[2]);
				bChanged = true;
				OutSaveRequested = true;
			}
			FVector RotEuler = Capsule.Rotation.ToEulerZYXDeg();
			float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
			if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
			{
				Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::DragFloat("Radius", &Capsule.Radius, 0.01f, 0.01f, 100.0f))
			{
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::DragFloat("Length", &Capsule.Length, 0.01f, 0.01f, 100.0f))
			{
				bChanged = true;
				OutSaveRequested = true;
			}
			if (ImGui::Button("Remove"))
			{
				BodySetup->AggGeom.SphylElems.RemoveAt(i);
				bChanged = true;
				OutSaveRequested = true;
				ImGui::TreePop();
				ImGui::PopID();
				return bChanged;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Convex 렌더링
	for (int32 i = 0; i < ConvexCount; ++i)
	{
		ImGui::PushID(SphereCount + BoxCount + SphylCount + i);
		const FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems[i];

		const char* SourceStr = (Convex.Source == EConvexSource::Mesh) ? "Mesh" : "Custom";
		if (ImGui::TreeNode("Convex", "Convex [%d] (%s)", i, SourceStr))
		{
			ImGui::Text("Source: %s", SourceStr);
			ImGui::Text("CookedData: %d bytes", Convex.CookedData.Num());
			ImGui::Text("Vertices: %d", Convex.VertexData.Num());
			if (ImGui::Button("Remove"))
			{
				BodySetup->AggGeom.ConvexElems.RemoveAt(i);
				bChanged = true;
				OutSaveRequested = true;
				ImGui::TreePop();
				ImGui::PopID();
				return bChanged;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// TriangleMesh 렌더링
	for (int32 i = 0; i < TriMeshCount; ++i)
	{
		ImGui::PushID(SphereCount + BoxCount + SphylCount + ConvexCount + i);
		const FKTriangleMeshElem& TriMesh = BodySetup->AggGeom.TriangleMeshElems[i];

		const char* SourceStr = (TriMesh.Source == ETriangleMeshSource::Mesh) ? "Mesh" : "Custom";
		if (ImGui::TreeNode("TriangleMesh", "TriangleMesh [%d] (%s)", i, SourceStr))
		{
			ImGui::Text("Source: %s", SourceStr);
			ImGui::Text("CookedData: %d bytes", TriMesh.CookedData.Num());
			ImGui::Text("Vertices: %d", TriMesh.VertexData.Num());
			ImGui::Text("Indices: %d", TriMesh.IndexData.Num());
			if (ImGui::Button("Remove"))
			{
				BodySetup->AggGeom.TriangleMeshElems.RemoveAt(i);
				bChanged = true;
				OutSaveRequested = true;
				ImGui::TreePop();
				ImGui::PopID();
				return bChanged;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	return bChanged;
}

void UBodySetup::OnPropertyChanged(const FProperty& Prop)
{
	UObject::OnPropertyChanged(Prop);

	// CollisionComplexity 변경 시 충돌체 재생성
	if (Prop.Name == "CollisionComplexity")
	{
		if (OwningMesh)
		{
			OwningMesh->RegenerateCollision();
		}
	}
}
