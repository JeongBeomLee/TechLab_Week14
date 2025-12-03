#pragma once

#include "Archive.h"
#include "Vector.h"

/**
 * FConvexCacheData
 *
 * Convex Hull의 Cooked 바이너리 데이터 캐시용 구조체
 * FKConvexElem의 직렬화 전용 intermediate structure
 */
struct FConvexCacheData
{
    /** PhysX Cooked 바이너리 데이터 */
    TArray<uint8> CookedData;

    /** 로컬 변환 */
    FTransform Transform;

    friend FArchive& operator<<(FArchive& Ar, FConvexCacheData& Data)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteArray(Ar, Data.CookedData);
            Ar << Data.Transform.Translation.X;
            Ar << Data.Transform.Translation.Y;
            Ar << Data.Transform.Translation.Z;
            Ar << Data.Transform.Rotation.X;
            Ar << Data.Transform.Rotation.Y;
            Ar << Data.Transform.Rotation.Z;
            Ar << Data.Transform.Rotation.W;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadArray(Ar, Data.CookedData);
            Ar << Data.Transform.Translation.X;
            Ar << Data.Transform.Translation.Y;
            Ar << Data.Transform.Translation.Z;
            Ar << Data.Transform.Rotation.X;
            Ar << Data.Transform.Rotation.Y;
            Ar << Data.Transform.Rotation.Z;
            Ar << Data.Transform.Rotation.W;
        }
        return Ar;
    }
};

/**
 * FPhysicsCacheData
 *
 * StaticMesh 물리 데이터 캐시용 구조체
 * DerivedDataCache/*.physics.bin 파일 직렬화에 사용
 */
struct FPhysicsCacheData
{
    /** 캐시 포맷 버전 */
    uint32 Version = 1;

    /** Convex 요소 배열 */
    TArray<FConvexCacheData> ConvexElems;

    friend FArchive& operator<<(FArchive& Ar, FPhysicsCacheData& Data)
    {
        if (Ar.IsSaving())
        {
            Ar << Data.Version;

            uint32 ConvexCount = static_cast<uint32>(Data.ConvexElems.size());
            Ar << ConvexCount;

            for (auto& Convex : Data.ConvexElems)
            {
                Ar << Convex;
            }
        }
        else if (Ar.IsLoading())
        {
            Ar << Data.Version;

            uint32 ConvexCount = 0;
            Ar << ConvexCount;

            Data.ConvexElems.resize(ConvexCount);
            for (auto& Convex : Data.ConvexElems)
            {
                Ar << Convex;
            }
        }
        return Ar;
    }
};
