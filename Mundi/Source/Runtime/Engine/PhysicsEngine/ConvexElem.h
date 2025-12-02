#pragma once

#include "ShapeElem.h"
#include "PhysXSupport.h"
#include "AABB.h"
#include "ConvexSource.h"
#include "FKConvexElem.generated.h"

/**
 * FKConvexElem
 *
 * Convex Hull 형태의 충돌체
 * 메시 정점으로부터 Convex Hull을 생성하여 물리 충돌에 사용
 */
USTRUCT()
struct FKConvexElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

    static inline EAggCollisionShape StaticShapeType = EAggCollisionShape::Convex;

    /** 정점 데이터 소스 */
    UPROPERTY()
    EConvexSource Source = EConvexSource::Mesh;

    /** Convex Hull 정점 데이터 (Source == Custom일 때만 사용) */
    UPROPERTY()
    TArray<FVector> VertexData;

    /** 렌더링/디버그용 인덱스 데이터 */
    UPROPERTY()
    TArray<int32> IndexData;

    /** 로컬 변환 */
    UPROPERTY(EditAnywhere, Category="Transform")
    FTransform ElemTransform;

    /** 바운딩 박스 */
    UPROPERTY()
    FAABB ElemBox;

    /** Cooked PhysX Convex Mesh (런타임, 직렬화 안함) */
    PxConvexMesh* ConvexMesh = nullptr;

    /** Cooked 바이너리 데이터 (캐시 저장/로드용) */
    TArray<uint8> CookedData;

    FKConvexElem()
        : FKShapeElem(EAggCollisionShape::Convex)
        , ElemTransform()
        , ElemBox(FAABB())
        , ConvexMesh(nullptr)
    {
    }

    FKConvexElem(const FKConvexElem& Other)
        : FKShapeElem(Other)
        , VertexData(Other.VertexData)
        , IndexData(Other.IndexData)
        , ElemTransform(Other.ElemTransform)
        , ElemBox(Other.ElemBox)
        , CookedData(Other.CookedData)
        , ConvexMesh(nullptr)  // ConvexMesh는 복사하지 않음, 필요시 재생성
    {
    }

    virtual ~FKConvexElem();

    FKConvexElem& operator=(const FKConvexElem& Other)
    {
        if (this != &Other)
        {
            FKShapeElem::operator=(Other);
            VertexData = Other.VertexData;
            IndexData = Other.IndexData;
            ElemTransform = Other.ElemTransform;
            ElemBox = Other.ElemBox;
            CookedData = Other.CookedData;
            // ConvexMesh는 복사하지 않음
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

    /** 정점 데이터 설정 및 바운딩 박스 계산 */
    void SetVertices(const TArray<FVector>& InVertices);

    /** 정점 데이터로부터 Convex Mesh 생성 (Cooking) */
    bool CreateConvexMesh();

    /** Cooked 데이터로부터 Convex Mesh 생성 (런타임 로드) */
    bool CreateConvexMeshFromCookedData();

    /** PhysX Convex Geometry 반환 */
    PxConvexMeshGeometry GetPxGeometry(const FVector& Scale3D) const;

    /** Convex Mesh 해제 */
    void ReleaseConvexMesh();

    /** PxConvexMesh에서 VertexData/IndexData 추출 (디버그 렌더링용) */
    void ExtractDataFromConvexMesh();

    /** 바운딩 박스 재계산 */
    void UpdateBounds();

    /** 유효한 Convex인지 확인 */
    bool IsValid() const { return ConvexMesh != nullptr || CookedData.Num() > 0; }

    /** 정점 개수 반환 */
    int32 GetVertexCount() const { return VertexData.Num(); }
};
