#include "pch.h"
#include "TriangleMeshElem.h"

FKTriangleMeshElem::~FKTriangleMeshElem()
{
    ReleaseTriangleMesh();
}

void FKTriangleMeshElem::SetMeshData(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices)
{
    VertexData = InVertices;
    IndexData = InIndices;
    UpdateBounds();
}

void FKTriangleMeshElem::UpdateBounds()
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

bool FKTriangleMeshElem::CreateTriangleMesh()
{
    if (VertexData.Num() < 3)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - Not enough vertices (need at least 3, got %d)", VertexData.Num());
        return false;
    }

    if (IndexData.Num() < 3 || IndexData.Num() % 3 != 0)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - Invalid index count (got %d, must be multiple of 3)", IndexData.Num());
        return false;
    }

    if (!GPhysXSDK || !GPhysXCooking)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - PhysX not initialized");
        return false;
    }

    // 기존 메시 해제
    ReleaseTriangleMesh();
    CookedData.Empty();

    // PhysX 정점 배열 생성
    TArray<PxVec3> PxVerts;
    PxVerts.SetNum(VertexData.Num());
    for (int32 i = 0; i < VertexData.Num(); ++i)
    {
        PxVerts[i] = U2PVector(VertexData[i]);
    }

    // Triangle Mesh Description 설정
    PxTriangleMeshDesc TriMeshDesc;
    TriMeshDesc.points.count = PxVerts.Num();
    TriMeshDesc.points.stride = sizeof(PxVec3);
    TriMeshDesc.points.data = PxVerts.GetData();

    TriMeshDesc.triangles.count = IndexData.Num() / 3;
    TriMeshDesc.triangles.stride = sizeof(uint32) * 3;
    TriMeshDesc.triangles.data = IndexData.GetData();

    // Cooking 수행 및 결과를 메모리에 저장
    PxDefaultMemoryOutputStream WriteBuffer;

    if (!GPhysXCooking->cookTriangleMesh(TriMeshDesc, WriteBuffer))
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - Cooking failed");
        return false;
    }

    // Cooked 데이터 저장 (캐시용)
    CookedData.SetNum(WriteBuffer.getSize());
    memcpy(CookedData.GetData(), WriteBuffer.getData(), WriteBuffer.getSize());

    // Triangle Mesh 생성
    PxDefaultMemoryInputData ReadBuffer(WriteBuffer.getData(), WriteBuffer.getSize());
    TriangleMesh = GPhysXSDK->createTriangleMesh(ReadBuffer);

    if (!TriangleMesh)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - Failed to create PxTriangleMesh");
        CookedData.Empty();
        return false;
    }

    UE_LOG("FKTriangleMeshElem::CreateTriangleMesh - Success (vertices: %d, triangles: %d, cooked size: %d bytes)",
           VertexData.Num(), GetTriangleCount(), CookedData.Num());

    return true;
}

bool FKTriangleMeshElem::CreateTriangleMeshFromCookedData()
{
    if (CookedData.Num() == 0)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMeshFromCookedData - No cooked data");
        return false;
    }

    if (!GPhysXSDK)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMeshFromCookedData - PhysX not initialized");
        return false;
    }

    // 기존 메시 해제
    ReleaseTriangleMesh();

    // Cooked 데이터로부터 Triangle Mesh 생성
    PxDefaultMemoryInputData ReadBuffer(CookedData.GetData(), CookedData.Num());
    TriangleMesh = GPhysXSDK->createTriangleMesh(ReadBuffer);

    if (!TriangleMesh)
    {
        UE_LOG("FKTriangleMeshElem::CreateTriangleMeshFromCookedData - Failed to create PxTriangleMesh");
        return false;
    }

    // 바운딩 박스 업데이트
    UpdateBounds();

    return true;
}

PxTriangleMeshGeometry FKTriangleMeshElem::GetPxGeometry(const FVector& Scale3D) const
{
    if (!TriangleMesh)
    {
        // 빈 geometry 반환 (에러 상황)
        return PxTriangleMeshGeometry();
    }

    FVector AbsScale(FMath::Abs(Scale3D.X), FMath::Abs(Scale3D.Y), FMath::Abs(Scale3D.Z));
    PxMeshScale MeshScale(U2PVector(AbsScale), PxQuat(PxIdentity));
    return PxTriangleMeshGeometry(TriangleMesh, MeshScale);
}

void FKTriangleMeshElem::ReleaseTriangleMesh()
{
    if (TriangleMesh)
    {
        TriangleMesh->release();
        TriangleMesh = nullptr;
    }
}
