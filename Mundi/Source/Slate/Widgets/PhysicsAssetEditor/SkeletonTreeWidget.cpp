#include "pch.h"
#include "SkeletonTreeWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletonTreeWidget);

USkeletonTreeWidget::USkeletonTreeWidget()
	: UWidget("SkeletonTreeWidget")
{
}

void USkeletonTreeWidget::Initialize()
{
	bCacheValid = false;
}

void USkeletonTreeWidget::Update()
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton) return;

	// 변경 감지: 본/바디/제약조건 수가 변경되면 캐시 무효화
	size_t CurrentBoneCount = Skeleton->Bones.size();
	size_t CurrentBodyCount = EditorState->EditingAsset ? EditorState->EditingAsset->BodySetups.size() : 0;
	size_t CurrentConstraintCount = EditorState->EditingAsset ? EditorState->EditingAsset->ConstraintSetups.size() : 0;

	if (CurrentBoneCount != CachedBoneCount ||
		CurrentBodyCount != CachedBodyCount ||
		CurrentConstraintCount != CachedConstraintCount)
	{
		bCacheValid = false;
		CachedBoneCount = CurrentBoneCount;
		CachedBodyCount = CurrentBodyCount;
		CachedConstraintCount = CurrentConstraintCount;
	}

	// 캐시가 무효화되었으면 재구축
	if (!bCacheValid)
	{
		RebuildCache();
	}
}

void USkeletonTreeWidget::RenderWidget()
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return;

	// 루트 본부터 시작 (부모가 -1인 본)
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		if (Skeleton->Bones[i].ParentIndex == -1)
		{
			RenderBoneNode(i, 0);
		}
	}
}

void USkeletonTreeWidget::RebuildCache()
{
	Cache.Clear();

	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton) return;

	// 부모 -> 자식 맵 구축
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentIdx >= 0)
		{
			Cache.ChildrenMap[ParentIdx].push_back(i);
		}
	}

	// 본 -> 바디 맵 구축
	if (EditorState->EditingAsset)
	{
		for (int32 i = 0; i < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()); ++i)
		{
			UBodySetup* BodySetup = EditorState->EditingAsset->BodySetups[i];
			if (BodySetup && BodySetup->BoneIndex >= 0)
			{
				Cache.BoneToBodyMap[BodySetup->BoneIndex] = i;
			}
		}

		// 본 -> 제약조건 맵 구축 (자식 바디의 본 인덱스 기준)
		for (int32 i = 0; i < static_cast<int32>(EditorState->EditingAsset->ConstraintSetups.size()); ++i)
		{
			const FConstraintSetup& Constraint = EditorState->EditingAsset->ConstraintSetups[i];
			if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
			{
				UBodySetup* ChildBody = EditorState->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
				if (ChildBody && ChildBody->BoneIndex >= 0)
				{
					Cache.BoneToConstraintMap[ChildBody->BoneIndex].push_back(i);
				}
			}
		}
	}

	bCacheValid = true;
}

void USkeletonTreeWidget::RenderBoneNode(int32 BoneIndex, int32 Depth)
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	const FBone& Bone = Skeleton->Bones[BoneIndex];

	// 이 본에 연결된 바디 찾기 (캐시 사용 - O(1))
	int32 BodyIndex = -1;
	auto BodyIt = Cache.BoneToBodyMap.find(BoneIndex);
	if (BodyIt != Cache.BoneToBodyMap.end())
	{
		BodyIndex = BodyIt->second;
	}
	bool bHasBody = (BodyIndex >= 0);

	// 자식 본 가져오기 (캐시 사용 - O(1))
	const std::vector<int32>* ChildIndices = nullptr;
	auto ChildIt = Cache.ChildrenMap.find(BoneIndex);
	if (ChildIt != Cache.ChildrenMap.end())
	{
		ChildIndices = &ChildIt->second;
	}
	bool bHasChildBones = (ChildIndices != nullptr && !ChildIndices->empty());

	// 이 본에 연결된 제약조건 가져오기 (캐시 사용 - O(1))
	const std::vector<int32>* ConstraintIndices = nullptr;
	auto ConstraintIt = Cache.BoneToConstraintMap.find(BoneIndex);
	if (ConstraintIt != Cache.BoneToConstraintMap.end())
	{
		ConstraintIndices = &ConstraintIt->second;
	}
	bool bHasConstraints = (ConstraintIndices != nullptr && !ConstraintIndices->empty());

	// 트리 노드 플래그 (자식 본과 제약조건이 모두 없으면 리프)
	bool bHasChildren = bHasChildBones || bHasConstraints;
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasChildren)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (EditorState->SelectedBoneIndex == BoneIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// 확장 상태 확인
	bool bIsExpanded = EditorState->ExpandedBoneIndices.count(BoneIndex) > 0;
	if (bIsExpanded)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}

	// 노드 색상 (바디가 있으면 녹색으로 강조)
	if (bHasBody)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
	}

	// 트리 노드 렌더링 (본 이름만 표시)
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, NodeFlags, "%s", Bone.Name.c_str());

	if (bHasBody)
	{
		ImGui::PopStyleColor();
	}

	// 클릭 처리: 본 선택 시 해당 바디도 함께 선택
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		EditorState->SelectedBoneIndex = BoneIndex;
		if (bHasBody)
		{
			EditorState->SelectBody(BodyIndex);
		}
	}

	// 컨텍스트 메뉴
	if (ImGui::BeginPopupContextItem())
	{
		if (!bHasBody)
		{
			if (ImGui::MenuItem("Add Body"))
			{
				EditorState->SelectedBoneIndex = BoneIndex;
			}
		}
		else
		{
			if (ImGui::MenuItem("Select Body"))
			{
				EditorState->SelectBody(BodyIndex);
			}
			if (ImGui::MenuItem("Remove Body"))
			{
				// TODO: 바디 제거 기능
			}
		}
		ImGui::EndPopup();
	}

	if (bNodeOpen)
	{
		EditorState->ExpandedBoneIndices.insert(BoneIndex);

		// 이 본에 연결된 제약조건 먼저 표시
		if (ConstraintIndices)
		{
			for (int32 ConstraintIndex : *ConstraintIndices)
			{
				RenderConstraintNode(ConstraintIndex);
			}
		}

		// 자식 본 렌더링
		if (ChildIndices)
		{
			for (int32 ChildIndex : *ChildIndices)
			{
				RenderBoneNode(ChildIndex, Depth + 1);
			}
		}

		ImGui::TreePop();
	}
	else
	{
		EditorState->ExpandedBoneIndices.erase(BoneIndex);
	}
}

void USkeletonTreeWidget::RenderBodyNode(int32 BodyIndex)
{
	if (!EditorState || !EditorState->EditingAsset) return;
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(EditorState->EditingAsset->BodySetups.size())) return;

	UBodySetup* Body = EditorState->EditingAsset->BodySetups[BodyIndex];
	if (!Body) return;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (EditorState->bBodySelectionMode && EditorState->SelectedBodyIndex == BodyIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// Shape 개수 표시
	int32 ShapeCount = Body->AggGeom.GetElementCount();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(1000 + BodyIndex), NodeFlags, "[%d shapes]", ShapeCount);
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		EditorState->SelectBody(BodyIndex);
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void USkeletonTreeWidget::RenderConstraintNode(int32 ConstraintIndex)
{
	if (!EditorState || !EditorState->EditingAsset) return;
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(EditorState->EditingAsset->ConstraintSetups.size())) return;

	const FConstraintSetup& Constraint = EditorState->EditingAsset->ConstraintSetups[ConstraintIndex];

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!EditorState->bBodySelectionMode && EditorState->SelectedConstraintIndex == ConstraintIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// 부모-자식 바디 이름 가져오기
	FString ParentName = "?";
	FString ChildName = "?";
	if (Constraint.ParentBodyIndex >= 0 && Constraint.ParentBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
	{
		UBodySetup* ParentBody = EditorState->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
		if (ParentBody) ParentName = ParentBody->BoneName.ToString();
	}
	if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
	{
		UBodySetup* ChildBody = EditorState->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
		if (ChildBody) ChildName = ChildBody->BoneName.ToString();
	}

	// 선택 여부에 따라 색상 변경
	bool bSelected = (!EditorState->bBodySelectionMode && EditorState->SelectedConstraintIndex == ConstraintIndex);
	ImGui::PushStyleColor(ImGuiCol_Text, bSelected ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.7f, 0.4f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(2000 + ConstraintIndex), NodeFlags, "[Constraint] %s -> %s",
		ParentName.c_str(), ChildName.c_str());
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		EditorState->SelectConstraint(ConstraintIndex);
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

