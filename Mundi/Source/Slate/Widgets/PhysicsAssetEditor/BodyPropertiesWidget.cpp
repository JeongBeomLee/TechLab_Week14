#include "pch.h"
#include "BodyPropertiesWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/FBodySetup.h"

IMPLEMENT_CLASS(UBodyPropertiesWidget);

UBodyPropertiesWidget::UBodyPropertiesWidget()
	: UWidget("BodyPropertiesWidget")
{
}

void UBodyPropertiesWidget::Initialize()
{
	bWasModified = false;
}

void UBodyPropertiesWidget::Update()
{
	// 필요시 업데이트 로직 추가
}

void UBodyPropertiesWidget::RenderWidget()
{
	if (!EditorState || !EditorState->EditingAsset) return;

	// 선택된 바디가 없으면 렌더링하지 않음
	if (!EditorState->bBodySelectionMode || EditorState->SelectedBodyIndex < 0) return;
	if (EditorState->SelectedBodyIndex >= static_cast<int32>(EditorState->EditingAsset->BodySetups.size())) return;

	FBodySetup& Body = EditorState->EditingAsset->BodySetups[EditorState->SelectedBodyIndex];
	bWasModified = false;

	ImGui::Text("Body: %s", Body.BoneName.ToString().c_str());
	ImGui::Separator();

	// Shape 속성
	if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bWasModified |= RenderShapeProperties(Body);
	}

	// 물리 속성
	if (ImGui::CollapsingHeader("Physics Properties"))
	{
		bWasModified |= RenderPhysicsProperties(Body);
	}
}

bool UBodyPropertiesWidget::RenderShapeProperties(FBodySetup& Body)
{
	bool bChanged = false;

	// Shape 타입 선택 (타입 변경 시 라인 수가 달라지므로 전체 재구성)
	const char* ShapeTypes[] = { "Sphere", "Capsule", "Box" };
	int32 CurrentType = static_cast<int32>(Body.ShapeType);
	EPhysicsShapeType OldType = Body.ShapeType;

	if (ImGui::Combo("Shape Type", &CurrentType, ShapeTypes, IM_ARRAYSIZE(ShapeTypes)))
	{
		EPhysicsShapeType NewType = static_cast<EPhysicsShapeType>(CurrentType);

		// 타입 변환 시 파라미터 보존
		if (OldType != NewType)
		{
			switch (OldType)
			{
			case EPhysicsShapeType::Sphere:
				if (NewType == EPhysicsShapeType::Capsule)
				{
					// Sphere → Capsule: Radius 유지, HalfHeight = Radius
					Body.HalfHeight = Body.Radius;
				}
				else if (NewType == EPhysicsShapeType::Box)
				{
					// Sphere → Box: Extent = (Radius, Radius, Radius)
					Body.Extent = FVector(Body.Radius, Body.Radius, Body.Radius);
				}
				break;

			case EPhysicsShapeType::Capsule:
				if (NewType == EPhysicsShapeType::Sphere)
				{
					// Capsule → Sphere: Radius 유지
				}
				else if (NewType == EPhysicsShapeType::Box)
				{
					// Capsule → Box: Extent = (Radius, Radius, HalfHeight)
					Body.Extent = FVector(Body.Radius, Body.Radius, Body.HalfHeight);
				}
				break;

			case EPhysicsShapeType::Box:
				if (NewType == EPhysicsShapeType::Sphere)
				{
					// Box → Sphere: Radius = 평균 크기
					Body.Radius = (Body.Extent.X + Body.Extent.Y + Body.Extent.Z) / 3.0f;
				}
				else if (NewType == EPhysicsShapeType::Capsule)
				{
					// Box → Capsule: Radius = XY 평균, HalfHeight = Z
					Body.Radius = (Body.Extent.X + Body.Extent.Y) / 2.0f;
					Body.HalfHeight = Body.Extent.Z;
				}
				break;
			}
		}

		Body.ShapeType = NewType;
		EditorState->bIsDirty = true;
		EditorState->RequestLinesRebuild();  // 타입 변경 시에만 전체 재구성
		bChanged = true;
	}

	// Shape 파라미터 (타입에 따라) - 좌표만 업데이트
	switch (Body.ShapeType)
	{
	case EPhysicsShapeType::Sphere:
		if (ImGui::DragFloat("Radius", &Body.Radius, 0.1f, 0.1f, 1000.0f))
		{
			EditorState->bIsDirty = true;
			EditorState->RequestSelectedBodyLinesUpdate();
			bChanged = true;
		}
		break;

	case EPhysicsShapeType::Capsule:
		if (ImGui::DragFloat("Radius", &Body.Radius, 0.1f, 0.1f, 1000.0f))
		{
			EditorState->bIsDirty = true;
			EditorState->RequestSelectedBodyLinesUpdate();
			bChanged = true;
		}
		if (ImGui::DragFloat("Half Height", &Body.HalfHeight, 0.1f, 0.1f, 1000.0f))
		{
			EditorState->bIsDirty = true;
			EditorState->RequestSelectedBodyLinesUpdate();
			bChanged = true;
		}
		break;

	case EPhysicsShapeType::Box:
		float Extent[3] = { Body.Extent.X, Body.Extent.Y, Body.Extent.Z };
		if (ImGui::DragFloat3("Extent", Extent, 0.1f, 0.1f, 1000.0f))
		{
			Body.Extent = FVector(Extent[0], Extent[1], Extent[2]);
			EditorState->bIsDirty = true;
			EditorState->RequestSelectedBodyLinesUpdate();
			bChanged = true;
		}
		break;
	}

	// 로컬 트랜스폼 - 좌표만 업데이트
	ImGui::Separator();
	ImGui::Text("Local Transform");

	FVector Location = Body.LocalTransform.Translation;
	float Loc[3] = { Location.X, Location.Y, Location.Z };
	if (ImGui::DragFloat3("Location", Loc, 0.1f))
	{
		Body.LocalTransform.Translation = FVector(Loc[0], Loc[1], Loc[2]);
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	FVector RotEuler = Body.LocalTransform.Rotation.ToEulerZYXDeg();
	float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
	if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
	{
		Body.LocalTransform.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	return bChanged;
}

bool UBodyPropertiesWidget::RenderPhysicsProperties(FBodySetup& Body)
{
	bool bChanged = false;

	if (ImGui::DragFloat("Mass", &Body.Mass, 0.1f, 0.01f, 10000.0f))
	{
		EditorState->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::DragFloat("Linear Damping", &Body.LinearDamping, 0.01f, 0.0f, 100.0f))
	{
		EditorState->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::DragFloat("Angular Damping", &Body.AngularDamping, 0.01f, 0.0f, 100.0f))
	{
		EditorState->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::Checkbox("Enable Gravity", &Body.bEnableGravity))
	{
		EditorState->bIsDirty = true;
		bChanged = true;
	}

	return bChanged;
}
