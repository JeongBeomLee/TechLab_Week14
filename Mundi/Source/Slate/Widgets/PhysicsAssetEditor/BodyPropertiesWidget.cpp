#include "pch.h"
#include "BodyPropertiesWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"

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

	UBodySetup* Body = EditorState->EditingAsset->BodySetups[EditorState->SelectedBodyIndex];
	if (!Body) return;

	bWasModified = false;

	ImGui::Text("Body: %s", Body->BoneName.ToString().c_str());
	ImGui::Separator();

	// Shape 속성
	if (ImGui::CollapsingHeader("Shapes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bWasModified |= RenderShapeProperties(Body);
	}
}

bool UBodyPropertiesWidget::RenderShapeProperties(UBodySetup* Body)
{
	bool bChanged = false;

	// 현재 Shape 개수 표시
	int32 SphereCount = Body->AggGeom.SphereElems.Num();
	int32 BoxCount = Body->AggGeom.BoxElems.Num();
	int32 SphylCount = Body->AggGeom.SphylElems.Num();
	int32 TotalShapes = SphereCount + BoxCount + SphylCount;

	ImGui::Text("Total Shapes: %d", TotalShapes);

	// Shape 추가 버튼
	if (ImGui::Button("Add Sphere"))
	{
		FKSphereElem NewSphere;
		NewSphere.Radius = 0.15f;
		Body->AggGeom.SphereElems.Add(NewSphere);
		EditorState->bIsDirty = true;
		EditorState->RequestLinesRebuild();
		bChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Box"))
	{
		FKBoxElem NewBox(0.3f, 0.3f, 0.3f);
		Body->AggGeom.BoxElems.Add(NewBox);
		EditorState->bIsDirty = true;
		EditorState->RequestLinesRebuild();
		bChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Capsule"))
	{
		FKSphylElem NewCapsule(0.15f, 0.5f);
		Body->AggGeom.SphylElems.Add(NewCapsule);
		EditorState->bIsDirty = true;
		EditorState->RequestLinesRebuild();
		bChanged = true;
	}

	ImGui::Separator();

	// Sphere 형태 편집
	for (int32 i = 0; i < SphereCount; ++i)
	{
		ImGui::PushID(i);
		if (ImGui::TreeNode("Sphere", "Sphere [%d]", i))
		{
			bChanged |= RenderSphereShape(Body, i);

			if (ImGui::Button("Remove"))
			{
				Body->AggGeom.SphereElems.RemoveAt(i);
				EditorState->bIsDirty = true;
				EditorState->RequestLinesRebuild();
				bChanged = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;  // 배열이 변경되었으므로 루프 종료
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Box 형태 편집
	for (int32 i = 0; i < BoxCount; ++i)
	{
		ImGui::PushID(SphereCount + i);
		if (ImGui::TreeNode("Box", "Box [%d]", i))
		{
			bChanged |= RenderBoxShape(Body, i);

			if (ImGui::Button("Remove"))
			{
				Body->AggGeom.BoxElems.RemoveAt(i);
				EditorState->bIsDirty = true;
				EditorState->RequestLinesRebuild();
				bChanged = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Capsule 형태 편집
	for (int32 i = 0; i < SphylCount; ++i)
	{
		ImGui::PushID(SphereCount + BoxCount + i);
		if (ImGui::TreeNode("Capsule", "Capsule [%d]", i))
		{
			bChanged |= RenderSphylShape(Body, i);

			if (ImGui::Button("Remove"))
			{
				Body->AggGeom.SphylElems.RemoveAt(i);
				EditorState->bIsDirty = true;
				EditorState->RequestLinesRebuild();
				bChanged = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	return bChanged;
}

bool UBodyPropertiesWidget::RenderSphereShape(UBodySetup* Body, int32 ShapeIndex)
{
	if (ShapeIndex < 0 || ShapeIndex >= Body->AggGeom.SphereElems.Num()) return false;

	bool bChanged = false;
	FKSphereElem& Sphere = Body->AggGeom.SphereElems[ShapeIndex];

	// Center
	float Center[3] = { Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z };
	if (ImGui::DragFloat3("Center", Center, 0.01f))
	{
		Sphere.Center = FVector(Center[0], Center[1], Center[2]);
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Radius
	if (ImGui::DragFloat("Radius", &Sphere.Radius, 0.01f, 0.01f, 100.0f))
	{
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	return bChanged;
}

bool UBodyPropertiesWidget::RenderBoxShape(UBodySetup* Body, int32 ShapeIndex)
{
	if (ShapeIndex < 0 || ShapeIndex >= Body->AggGeom.BoxElems.Num()) return false;

	bool bChanged = false;
	FKBoxElem& Box = Body->AggGeom.BoxElems[ShapeIndex];

	// Center
	float Center[3] = { Box.Center.X, Box.Center.Y, Box.Center.Z };
	if (ImGui::DragFloat3("Center", Center, 0.01f))
	{
		Box.Center = FVector(Center[0], Center[1], Center[2]);
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Rotation (Euler)
	FVector RotEuler = Box.Rotation.ToEulerZYXDeg();
	float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
	if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
	{
		Box.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Extents (X, Y, Z are diameters)
	float Extents[3] = { Box.X, Box.Y, Box.Z };
	if (ImGui::DragFloat3("Size (XYZ)", Extents, 0.01f, 0.01f, 100.0f))
	{
		Box.X = Extents[0];
		Box.Y = Extents[1];
		Box.Z = Extents[2];
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	return bChanged;
}

bool UBodyPropertiesWidget::RenderSphylShape(UBodySetup* Body, int32 ShapeIndex)
{
	if (ShapeIndex < 0 || ShapeIndex >= Body->AggGeom.SphylElems.Num()) return false;

	bool bChanged = false;
	FKSphylElem& Capsule = Body->AggGeom.SphylElems[ShapeIndex];

	// Center
	float Center[3] = { Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z };
	if (ImGui::DragFloat3("Center", Center, 0.01f))
	{
		Capsule.Center = FVector(Center[0], Center[1], Center[2]);
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Rotation (Euler)
	FVector RotEuler = Capsule.Rotation.ToEulerZYXDeg();
	float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
	if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
	{
		Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Radius
	if (ImGui::DragFloat("Radius", &Capsule.Radius, 0.01f, 0.01f, 100.0f))
	{
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	// Length
	if (ImGui::DragFloat("Length", &Capsule.Length, 0.01f, 0.01f, 100.0f))
	{
		EditorState->bIsDirty = true;
		EditorState->RequestSelectedBodyLinesUpdate();
		bChanged = true;
	}

	return bChanged;
}
