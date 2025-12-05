#pragma once

#include "SphereElem.h"
#include "BoxElem.h"
#include "SphylElem.h"
#include "ConvexElem.h"
#include "TriangleMeshElem.h"
#include "FKAggregateGeom.generated.h"

USTRUCT()
struct FKAggregateGeom
{
    GENERATED_REFLECTION_BODY()

    /** 구 형태 충돌체 배열 */
    UPROPERTY()
    TArray<FKSphereElem> SphereElems;

    /** 박스 형태 충돌체 배열 */
    UPROPERTY()
    TArray<FKBoxElem> BoxElems;

    /** 캡슐 형태 충돌체 배열 */
    UPROPERTY()
    TArray<FKSphylElem> SphylElems;

    /** 컨벡스 충돌체 배열 */
    UPROPERTY()
    TArray<FKConvexElem> ConvexElems;

    /** 트라이앵글 메시 충돌체 배열 (Static 전용) */
    UPROPERTY()
    TArray<FKTriangleMeshElem> TriangleMeshElems;

    FKAggregateGeom()
    {
    }

    FKAggregateGeom(const FKAggregateGeom& Other)
    {
        CloneAgg(Other);
    }

    const FKAggregateGeom& operator=(const FKAggregateGeom& Other)
    {
        CloneAgg(Other);
        return *this;
    }

    /** 전체 요소 개수 반환 */
    int32 GetElementCount() const
    {
        return SphereElems.Num() + BoxElems.Num() + SphylElems.Num() + ConvexElems.Num() + TriangleMeshElems.Num();
    }

    /** 특정 타입의 요소 개수 반환 */
    int32 GetElementCount(EAggCollisionShape Type) const
    {
        switch (Type)
        {
        case EAggCollisionShape::Sphere:
            return SphereElems.Num();
        case EAggCollisionShape::Box:
            return BoxElems.Num();
        case EAggCollisionShape::Sphyl:
            return SphylElems.Num();
        case EAggCollisionShape::Convex:
            return ConvexElems.Num();
        case EAggCollisionShape::TriangleMesh:
            return TriangleMeshElems.Num();
        default:
            return 0;
        }
    }

    /** 모든 요소 제거 */
    void EmptyElements()
    {
        SphereElems.Empty();
        BoxElems.Empty();
        SphylElems.Empty();
        ConvexElems.Empty();
        TriangleMeshElems.Empty();
    }

private:
    void CloneAgg(const FKAggregateGeom& Other)
    {
        SphereElems = Other.SphereElems;
        BoxElems = Other.BoxElems;
        SphylElems = Other.SphylElems;
        ConvexElems = Other.ConvexElems;
        TriangleMeshElems = Other.TriangleMeshElems;
    }
};
