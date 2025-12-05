#pragma once

#include "ShapeElem.h"
#include "PhysXSupport.h"
#include "AABB.h"
#include "TriangleMeshSource.h"
#include "FKTriangleMeshElem.generated.h"

/**
 * FKTriangleMeshElem
 *
 * Triangle Mesh 형태의 충돌체
 * 렌더링 메시를 그대로 물리 충돌에 사용 (Static 전용)
 */
USTRUCT()
struct FKTriangleMeshElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

    static inline EAggCollisionShape StaticShapeType = EAggCollisionShape::TriangleMesh;

    /** 정점 데이터 소스 */
    UPROPERTY()
    ETriangleMeshSource Source = ETriangleMeshSource::Mesh;

    /** 정점 데이터 */
    UPROPERTY()
    TArray<FVector> VertexData;

    /** 삼각형 인덱스 (3개씩 그룹) */
    UPROPERTY()
    TArray<uint32> IndexData;

    /** 로컬 변환 */
    UPROPERTY(EditAnywhere, Category="Transform")
    FTransform ElemTransform;

    /** 바운딩 박스 */
    UPROPERTY()
    FAABB ElemBox;

    /** Cooked PhysX Triangle Mesh (런타임, 직렬화 안함) */
    PxTriangleMesh* TriangleMesh = nullptr;

    /** Cooked 바이너리 데이터 (캐시 저장/로드용) */
    TArray<uint8> CookedData;

    FKTriangleMeshElem()
        : FKShapeElem(EAggCollisionShape::TriangleMesh)
        , ElemTransform()
        , ElemBox(FAABB())
        , TriangleMesh(nullptr)
    {
    }

    FKTriangleMeshElem(const FKTriangleMeshElem& Other)
        : FKShapeElem(Other)
        , Source(Other.Source)
        , VertexData(Other.VertexData)
        , IndexData(Other.IndexData)
        , ElemTransform(Other.ElemTransform)
        , ElemBox(Other.ElemBox)
        , CookedData(Other.CookedData)
        , TriangleMesh(nullptr)  // TriangleMesh는 복사하지 않음, 필요시 재생성
    {
    }

    virtual ~FKTriangleMeshElem();

    FKTriangleMeshElem& operator=(const FKTriangleMeshElem& Other)
    {
        if (this != &Other)
        {
            // 기존 TriangleMesh 해제 (메모리 누수 방지)
            ReleaseTriangleMesh();

            FKShapeElem::operator=(Other);
            Source = Other.Source;
            VertexData = Other.VertexData;
            IndexData = Other.IndexData;
            ElemTransform = Other.ElemTransform;
            ElemBox = Other.ElemBox;
            CookedData = Other.CookedData;
            // TriangleMesh는 복사하지 않음, CookedData에서 재생성 필요
        }
        return *this;
    }

    /** 변환 반환 */
    virtual FTransform GetTransform() const override final
    {
        return ElemTransform;
    }

    /** 변환 설정 */
    void SetTransform(const FTransform& InTransform)
    {
        ElemTransform = InTransform;
    }

    /** 정점 및 인덱스 데이터 설정 */
    void SetMeshData(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices);

    /** 정점/인덱스 데이터로부터 Triangle Mesh 생성 (Cooking) */
    bool CreateTriangleMesh();

    /** Cooked 데이터로부터 Triangle Mesh 생성 (런타임 로드) */
    bool CreateTriangleMeshFromCookedData();

    /** PhysX Triangle Mesh Geometry 반환 */
    PxTriangleMeshGeometry GetPxGeometry(const FVector& Scale3D) const;

    /** Triangle Mesh 해제 */
    void ReleaseTriangleMesh();

    /** 바운딩 박스 재계산 */
    void UpdateBounds();

    /** 유효한 TriangleMesh인지 확인 */
    bool IsValid() const { return TriangleMesh != nullptr || CookedData.Num() > 0; }

    /** 정점 개수 반환 */
    int32 GetVertexCount() const { return VertexData.Num(); }

    /** 삼각형 개수 반환 */
    int32 GetTriangleCount() const { return IndexData.Num() / 3; }
};
