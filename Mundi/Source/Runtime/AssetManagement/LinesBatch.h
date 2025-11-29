#pragma once

/**
 * FLinesBatch - DOD 기반 라인 데이터 컨테이너
 *
 * 다수의 라인을 효율적으로 저장하기 위한 SoA(Structure of Arrays) 구조.
 * 개별 ULine 객체 생성 오버헤드 없이 대량의 라인을 관리.
 */
struct FLinesBatch
{
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	int32 Add(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1, 1, 1, 1))
	{
		int32 Index = static_cast<int32>(StartPoints.size());
		StartPoints.push_back(Start);
		EndPoints.push_back(End);
		Colors.push_back(Color);
		return Index;
	}

	void SetLine(int32 Index, const FVector& Start, const FVector& End)
	{
		StartPoints[Index] = Start;
		EndPoints[Index] = End;
	}

	void SetColor(int32 Index, const FVector4& Color)
	{
		Colors[Index] = Color;
	}

	void SetColorRange(int32 StartIndex, int32 Count, const FVector4& Color)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			Colors[StartIndex + i] = Color;
		}
	}

	void Clear()
	{
		StartPoints.clear();
		EndPoints.clear();
		Colors.clear();
	}

	void Reserve(int32 Count)
	{
		StartPoints.reserve(Count);
		EndPoints.reserve(Count);
		Colors.reserve(Count);
	}

	int32 Num() const { return static_cast<int32>(StartPoints.size()); }
};
