#include "pch.h"
#include "ConvexElem.h"

FKConvexElem::~FKConvexElem()
{
    ReleaseConvexMesh();
}

void FKConvexElem::SetVertices(const TArray<FVector>& InVertices)
{
    VertexData = InVertices;
    UpdateBounds();
}

void FKConvexElem::UpdateBounds()
{
    if (VertexData.Num() == 0)
    {
        ElemBox = FAABB();
        return;
    }

    FVector Min = VertexData[0];
    FVector Max = VertexData[0];

    for (const FVector& V : VertexData)
    {
        Min = Min.ComponentMin(V);
        Max = Max.ComponentMax(V);
    }

    ElemBox = FAABB(Min, Max);
}

bool FKConvexElem::CreateConvexMesh()
{
    if (VertexData.Num() < 4)
    {
        UE_LOG("FKConvexElem::CreateConvexMesh - Not enough vertices (need at least 4, got %d)", VertexData.Num());
        return false;
    }

    if (!GPhysXSDK || !GPhysXCooking)
    {
        UE_LOG("FKConvexElem::CreateConvexMesh - PhysX not initialized");
        return false;
    }

    // 기존 메시 해제
    ReleaseConvexMesh();
    CookedData.Empty();

    // PhysX 정점 배열 생성
    TArray<PxVec3> PxVerts;
    PxVerts.SetNum(VertexData.Num());
    for (int32 i = 0; i < VertexData.Num(); ++i)
    {
        PxVerts[i] = U2PVector(VertexData[i]);
    }

    // Convex Mesh Description 설정
    PxConvexMeshDesc ConvexDesc;
    ConvexDesc.points.count = PxVerts.Num();
    ConvexDesc.points.stride = sizeof(PxVec3);
    ConvexDesc.points.data = PxVerts.GetData();
    ConvexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

    // Cooking 수행 및 결과를 메모리에 저장
    PxDefaultMemoryOutputStream WriteBuffer;
    PxConvexMeshCookingResult::Enum CookingResult;

    if (!GPhysXCooking->cookConvexMesh(ConvexDesc, WriteBuffer, &CookingResult))
    {
        UE_LOG("FKConvexElem::CreateConvexMesh - Cooking failed (result: %d)", (int32)CookingResult);
        return false;
    }

    // Cooked 데이터 저장 (캐시용)
    CookedData.SetNum(WriteBuffer.getSize());
    memcpy(CookedData.GetData(), WriteBuffer.getData(), WriteBuffer.getSize());

    // Convex Mesh 생성
    PxDefaultMemoryInputData ReadBuffer(WriteBuffer.getData(), WriteBuffer.getSize());
    ConvexMesh = GPhysXSDK->createConvexMesh(ReadBuffer);

    if (!ConvexMesh)
    {
        UE_LOG("FKConvexElem::CreateConvexMesh - Failed to create PxConvexMesh");
        CookedData.Empty();
        return false;
    }

    UE_LOG("FKConvexElem::CreateConvexMesh - Success (vertices: %d, cooked size: %d bytes)",
           VertexData.Num(), CookedData.Num());

    return true;
}

bool FKConvexElem::CreateConvexMeshFromCookedData()
{
    if (CookedData.Num() == 0)
    {
        UE_LOG("FKConvexElem::CreateConvexMeshFromCookedData - No cooked data");
        return false;
    }

    if (!GPhysXSDK)
    {
        UE_LOG("FKConvexElem::CreateConvexMeshFromCookedData - PhysX not initialized");
        return false;
    }

    // 기존 메시 해제
    ReleaseConvexMesh();

    // Cooked 데이터로부터 Convex Mesh 생성
    PxDefaultMemoryInputData ReadBuffer(CookedData.GetData(), CookedData.Num());
    ConvexMesh = GPhysXSDK->createConvexMesh(ReadBuffer);

    if (!ConvexMesh)
    {
        UE_LOG("FKConvexElem::CreateConvexMeshFromCookedData - Failed to create PxConvexMesh");
        return false;
    }

    // PxConvexMesh에서 VertexData/IndexData 복원 (디버그 렌더링용)
    ExtractDataFromConvexMesh();

    return true;
}

void FKConvexElem::ExtractDataFromConvexMesh()
{
    if (!ConvexMesh)
    {
        return;
    }

    // 정점 추출
    uint32 NumVertices = ConvexMesh->getNbVertices();
    const PxVec3* PxVertices = ConvexMesh->getVertices();

    VertexData.Empty();
    VertexData.SetNum(NumVertices);
    for (uint32 i = 0; i < NumVertices; ++i)
    {
        VertexData[i] = FVector(PxVertices[i].x, PxVertices[i].y, PxVertices[i].z);
    }

    // 폴리곤에서 인덱스 추출 (삼각형화)
    IndexData.Empty();
    uint32 NumPolygons = ConvexMesh->getNbPolygons();
    const uint8* IndexBuffer = ConvexMesh->getIndexBuffer();

    for (uint32 PolyIdx = 0; PolyIdx < NumPolygons; ++PolyIdx)
    {
        PxHullPolygon PolyData;
        if (ConvexMesh->getPolygonData(PolyIdx, PolyData))
        {
            // 폴리곤을 삼각형 팬으로 분할
            uint16 FirstIndex = IndexBuffer[PolyData.mIndexBase];
            for (uint32 TriIdx = 1; TriIdx < PolyData.mNbVerts - 1; ++TriIdx)
            {
                uint16 Index1 = IndexBuffer[PolyData.mIndexBase + TriIdx];
                uint16 Index2 = IndexBuffer[PolyData.mIndexBase + TriIdx + 1];

                IndexData.Add(FirstIndex);
                IndexData.Add(Index1);
                IndexData.Add(Index2);
            }
        }
    }

    // 바운딩 박스 업데이트
    UpdateBounds();
}

PxConvexMeshGeometry FKConvexElem::GetPxGeometry(const FVector& Scale3D) const
{
    if (!ConvexMesh)
    {
        // 빈 geometry 반환 (에러 상황)
        return PxConvexMeshGeometry();
    }

    FVector AbsScale(FMath::Abs(Scale3D.X), FMath::Abs(Scale3D.Y), FMath::Abs(Scale3D.Z));
    PxMeshScale MeshScale(U2PVector(AbsScale), PxQuat(PxIdentity));
    return PxConvexMeshGeometry(ConvexMesh, MeshScale);
}

void FKConvexElem::ReleaseConvexMesh()
{
    if (ConvexMesh)
    {
        ConvexMesh->release();
        ConvexMesh = nullptr;
    }
}
