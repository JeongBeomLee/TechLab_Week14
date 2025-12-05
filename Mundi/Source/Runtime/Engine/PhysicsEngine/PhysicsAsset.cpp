#include "pch.h"
#include "PhysicsAsset.h"
#include "JsonSerializer.h"

IMPLEMENT_CLASS(UPhysicsAsset)

// ────────────────────────────────────────────────────────────────
// Body 관리 메서드
// ────────────────────────────────────────────────────────────────

int32 UPhysicsAsset::AddBody(const FName& BoneName, int32 BoneIndex)
{
	// 이미 존재하는지 확인
	int32 ExistingIndex = FindBodyIndex(BoneName);
	if (ExistingIndex != -1)
	{
		return ExistingIndex;
	}

	// 새 바디 생성 (UObject이므로 NewObject 사용)
	UBodySetup* NewBody = NewObject<UBodySetup>();
	NewBody->BoneName = BoneName;
	NewBody->BoneIndex = BoneIndex;

	int32 NewIndex = static_cast<int32>(Bodies.size());
	Bodies.Add(NewBody);

	// 캐시 갱신
	BodySetupIndexMap.Add(BoneName, NewIndex);

	return NewIndex;
}

bool UPhysicsAsset::RemoveBody(int32 BodyIndex)
{
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(Bodies.size()))
	{
		return false;
	}

	// 제거할 바디의 본 이름
	FName RemovedBoneName = Bodies[BodyIndex] ? Bodies[BodyIndex]->BoneName : FName();

	// 연결된 제약 조건도 제거 (본 이름 기준)
	for (int32 i = static_cast<int32>(Constraints.size()) - 1; i >= 0; --i)
	{
		FConstraintInstance& Constraint = Constraints[i];
		if (Constraint.ConstraintBone1 == RemovedBoneName || Constraint.ConstraintBone2 == RemovedBoneName)
		{
			Constraints.RemoveAt(i);
		}
	}

	// 바디 제거
	Bodies.RemoveAt(BodyIndex);

	// 캐시 재구성
	RebuildIndexMap();

	return true;
}

int32 UPhysicsAsset::FindBodyIndex(const FName& BoneName) const
{
	const int32* Found = BodySetupIndexMap.Find(BoneName);
	return Found ? *Found : -1;
}

int32 UPhysicsAsset::FindBodyIndexByBone(int32 BoneIndex) const
{
	for (int32 i = 0; i < static_cast<int32>(Bodies.size()); ++i)
	{
		if (Bodies[i] && Bodies[i]->BoneIndex == BoneIndex)
		{
			return i;
		}
	}
	return -1;
}

// ────────────────────────────────────────────────────────────────
// Constraint 관리 메서드
// ────────────────────────────────────────────────────────────────

int32 UPhysicsAsset::AddConstraint(const FName& ChildBone, const FName& ParentBone)
{
	// 이미 존재하는지 확인
	int32 ExistingIndex = FindConstraintIndex(ChildBone, ParentBone);
	if (ExistingIndex != -1)
	{
		return ExistingIndex;
	}

	// 새 제약 조건 생성
	FConstraintInstance NewConstraint;
	NewConstraint.ConstraintBone1 = ChildBone;   // Child Bone
	NewConstraint.ConstraintBone2 = ParentBone;  // Parent Bone
	int32 NewIndex = static_cast<int32>(Constraints.size());
	Constraints.Add(NewConstraint);

	return NewIndex;
}

bool UPhysicsAsset::RemoveConstraint(int32 ConstraintIndex)
{
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(Constraints.size()))
	{
		return false;
	}

	Constraints.RemoveAt(ConstraintIndex);
	return true;
}

int32 UPhysicsAsset::FindConstraintIndex(const FName& ChildBone, const FName& ParentBone) const
{
	for (int32 i = 0; i < static_cast<int32>(Constraints.size()); ++i)
	{
		const FConstraintInstance& Constraint = Constraints[i];
		if (Constraint.ConstraintBone1 == ChildBone && Constraint.ConstraintBone2 == ParentBone)
		{
			return i;
		}
	}
	return -1;
}

UBodySetup* UPhysicsAsset::FindBodySetup(const FName& BoneName) const
{
	for (UBodySetup* Body : Bodies)
	{
		if (Body && Body->BoneName == BoneName)
		{
			return Body;
		}
	}
	return nullptr;
}

// ────────────────────────────────────────────────────────────────
// 유틸리티
// ────────────────────────────────────────────────────────────────

void UPhysicsAsset::ClearAll()
{
	Bodies.Empty();
	Constraints.Empty();
	BodySetupIndexMap.Empty();
}

void UPhysicsAsset::ClearBodySelection()
{
	// 선택 상태는 에디터(PhysicsAssetEditorState)에서 관리
	// UBodySetup은 데이터만 저장
}

void UPhysicsAsset::ClearConstraintSelection()
{
	// FConstraintInstance는 bSelected 필드가 없으므로 에디터 상태에서 관리
}

bool UPhysicsAsset::IsValid() const
{
	// 최소한 하나의 바디가 있어야 함
	return !Bodies.IsEmpty();
}

void UPhysicsAsset::RebuildIndexMap()
{
	BodySetupIndexMap.Empty();
	for (int32 i = 0; i < static_cast<int32>(Bodies.size()); ++i)
	{
		if (Bodies[i])
		{
			BodySetupIndexMap.Add(Bodies[i]->BoneName, i);
		}
	}
}

// ────────────────────────────────────────────────────────────────
// 직렬화
// ────────────────────────────────────────────────────────────────

void UPhysicsAsset::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	FWideString WidePath(InFilePath.begin(), InFilePath.end());

	JSON JsonHandle;
	if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
	{
		UE_LOG("[UPhysicsAsset] Load 실패: %s", InFilePath.c_str());
		return;
	}

	Serialize(true, JsonHandle);
	UE_LOG("[UPhysicsAsset] Load 성공: %s", InFilePath.c_str());
}

void UPhysicsAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	if (bInIsLoading)
	{
		UObject::Serialize(bInIsLoading, InOutHandle);
		Bodies.Empty();
		JSON BodiesJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Bodies", BodiesJson, JSON(), false))
		{
			for (int32 i = 0; i < static_cast<int32>(BodiesJson.size()); ++i)
			{
				JSON BodyJson = BodiesJson.at(i);
				UBodySetup* NewBody = NewObject<UBodySetup>();
				// UBodySetup::Serialize가 UPROPERTY (BoneName, BoneIndex, PhysicalMaterial)와 AggGeom을 모두 처리
				NewBody->Serialize(true, BodyJson);
				Bodies.Add(NewBody);
			}
		}
		RebuildIndexMap();
	}
	else
	{
		UObject::Serialize(bInIsLoading, InOutHandle);
		JSON BodiesJson = JSON::Make(JSON::Class::Array);
		for (UBodySetup* Body : Bodies)
		{
			if (!Body) continue;
			JSON BodyJson = JSON::Make(JSON::Class::Object);
			// UBodySetup::Serialize가 UPROPERTY (BoneName, BoneIndex, PhysicalMaterial)와 AggGeom을 모두 처리
			Body->Serialize(false, BodyJson);
			BodiesJson.append(BodyJson);
		}
		InOutHandle["Bodies"] = BodiesJson;
	}
}

void UPhysicsAsset::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// UBodySetup* 배열 복제 - 각 UBodySetup의 Duplicate() 호출
	TArray<UBodySetup*> DuplicatedSetups;
	for (UBodySetup* BodySetup : Bodies)
	{
		if (BodySetup)
		{
			// UBodySetup::Duplicate()가 내부적으로 DuplicateSubObjects() 호출
			UBodySetup* NewBodySetup = static_cast<UBodySetup*>(BodySetup->Duplicate());
			DuplicatedSetups.Add(NewBodySetup);
		}
	}
	Bodies = DuplicatedSetups;

	// FConstraintInstance는 값 타입이므로 자동 복제됨
}
