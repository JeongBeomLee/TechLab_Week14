#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "ImGui/imgui.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include <filesystem>
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsTypes.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/AssetManagement/Line.h"
#include "Source/Runtime/AssetManagement/LinesBatch.h"
#include "Source/Editor/PlatformProcess.h"
#include "ResourceManager.h"

// Sub Widgets
#include "Source/Slate/Widgets/PhysicsAssetEditor/SkeletonTreeWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/BodyPropertiesWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/ConstraintPropertiesWidget.h"

// Gizmo
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/Components/BoneAnchorComponent.h"
#include "SelectionManager.h"
#include "World.h"

// Picking
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "CameraActor.h"
#include <chrono>

// ────────────────────────────────────────────────────────────────
// Shape 라인 좌표 생성 헬퍼
// ────────────────────────────────────────────────────────────────

// TODO: M_PI를 Math/MathConstants.h 등에 전역 상수로 이동
namespace
{
	constexpr float M_PI = 3.14159265358979f;
	constexpr int32 CircleSegments = 16;
	constexpr int32 CapsuleVerticalLines = 8;
}

/**
 * Sphere Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateSphereLineCoordinates(
	const FKSphereElem& Sphere,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Sphere.Center);
	float Radius = Sphere.Radius;

	// 3개 평면 (XY, XZ, YZ)에 원 그리기
	for (int32 j = 0; j < CircleSegments; ++j)
	{
		float Angle1 = (j / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Angle2 = ((j + 1) / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Cos1 = cos(Angle1), Sin1 = sin(Angle1);
		float Cos2 = cos(Angle2), Sin2 = sin(Angle2);

		// XY 평면
		OutStartPoints.Add(Center + FVector(Cos1 * Radius, Sin1 * Radius, 0));
		OutEndPoints.Add(Center + FVector(Cos2 * Radius, Sin2 * Radius, 0));
		// XZ 평면
		OutStartPoints.Add(Center + FVector(Cos1 * Radius, 0, Sin1 * Radius));
		OutEndPoints.Add(Center + FVector(Cos2 * Radius, 0, Sin2 * Radius));
		// YZ 평면
		OutStartPoints.Add(Center + FVector(0, Cos1 * Radius, Sin1 * Radius));
		OutEndPoints.Add(Center + FVector(0, Cos2 * Radius, Sin2 * Radius));
	}
}

/**
 * Box Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateBoxLineCoordinates(
	const FKBoxElem& Box,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
	FQuat Rotation = BoneTransform.Rotation * Box.Rotation;

	// Box.X, Box.Y, Box.Z는 지름이므로 반으로 나눔
	FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);

	// 8개 코너 생성 (dx/dy/dz 패턴)
	constexpr int dx[] = {-1, 1, 1, -1, -1, 1, 1, -1};
	constexpr int dy[] = {-1, -1, 1, 1, -1, -1, 1, 1};
	constexpr int dz[] = {-1, -1, -1, -1, 1, 1, 1, 1};

	FVector Corners[8];
	for (int i = 0; i < 8; ++i)
	{
		Corners[i] = Center + Rotation.RotateVector(
			FVector(HalfExtent.X * dx[i], HalfExtent.Y * dy[i], HalfExtent.Z * dz[i]));
	}

	// 12개 엣지 (pair로 정의)
	constexpr std::pair<int, int> Edges[] = {
		{0,1}, {1,2}, {2,3}, {3,0},  // 하단 면
		{4,5}, {5,6}, {6,7}, {7,4},  // 상단 면
		{0,4}, {1,5}, {2,6}, {3,7}   // 수직 엣지
	};

	for (const auto& [a, b] : Edges)
	{
		OutStartPoints.Add(Corners[a]);
		OutEndPoints.Add(Corners[b]);
	}
}

/**
 * Capsule(Sphyl) Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateCapsuleLineCoordinates(
	const FKSphylElem& Capsule,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);
	FQuat Rotation = BoneTransform.Rotation * Capsule.Rotation;

	float HalfLength = Capsule.Length * 0.5f;
	float Radius = Capsule.Radius;

	// 캡슐은 Z축 방향 (기존 엔진 규약)
	FVector Up = Rotation.RotateVector(FVector(0, 0, 1));  // Z축이 길이 방향
	FVector Top = Center + Up * HalfLength;
	FVector Bottom = Center - Up * HalfLength;

	// 수직 라인들
	for (int32 j = 0; j < CapsuleVerticalLines; ++j)
	{
		float Angle = (j / static_cast<float>(CapsuleVerticalLines)) * 2.0f * M_PI;
		FVector Dir = Rotation.RotateVector(FVector(cos(Angle), sin(Angle), 0));
		OutStartPoints.Add(Top + Dir * Radius);
		OutEndPoints.Add(Bottom + Dir * Radius);
	}

	// 상단/하단 원 (XY 평면, Z축이 길이 방향이므로)
	for (int32 j = 0; j < CircleSegments; ++j)
	{
		float Angle1 = (j / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Angle2 = ((j + 1) / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		FVector Dir1 = Rotation.RotateVector(FVector(cos(Angle1), sin(Angle1), 0));
		FVector Dir2 = Rotation.RotateVector(FVector(cos(Angle2), sin(Angle2), 0));

		OutStartPoints.Add(Top + Dir1 * Radius);
		OutEndPoints.Add(Top + Dir2 * Radius);
		OutStartPoints.Add(Bottom + Dir1 * Radius);
		OutEndPoints.Add(Bottom + Dir2 * Radius);
	}
}

/**
 * UBodySetup의 모든 Shape에 대해 와이어프레임 라인 좌표 생성
 */
static void GenerateBodyShapeLineCoordinates(
	const UBodySetup* BodySetup,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	if (!BodySetup) return;

	// Sphere shapes
	for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
	{
		GenerateSphereLineCoordinates(Sphere, BoneTransform, OutStartPoints, OutEndPoints);
	}

	// Box shapes
	for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
	{
		GenerateBoxLineCoordinates(Box, BoneTransform, OutStartPoints, OutEndPoints);
	}

	// Capsule shapes
	for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
	{
		GenerateCapsuleLineCoordinates(Capsule, BoneTransform, OutStartPoints, OutEndPoints);
	}
}

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
	WindowTitle = "Physics Asset Editor";
	bHasBottomPanel = false; // 기본적으로 하단 패널 숨김
	CreateSubWidgets();
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
	DestroySubWidgets();

	// Clean up tabs (ViewerState 정리)
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
	PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	// Context가 없거나 AssetPath가 비어있으면 새 탭 열기
	if (!Context || Context->AssetPath.empty())
	{
		char label[64];
		sprintf_s(label, "PhysicsAssetTab%d", Tabs.Num() + 1);
		OpenNewTab(label);
		return;
	}

	// 기존 탭 중 동일한 파일 경로를 가진 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		PhysicsAssetEditorState* State = static_cast<PhysicsAssetEditorState*>(Tabs[i]);
		if (State && State->CurrentFilePath == Context->AssetPath)
		{
			// 기존 탭으로 전환
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			return;
		}
	}

	// 기존 탭이 없으면 새 탭 생성
	FString AssetName;
	const size_t lastSlash = Context->AssetPath.find_last_of("/\\");
	if (lastSlash != FString::npos)
		AssetName = Context->AssetPath.substr(lastSlash + 1);
	else
		AssetName = Context->AssetPath;

	// 확장자 제거
	const size_t dotPos = AssetName.find_last_of('.');
	if (dotPos != FString::npos)
		AssetName = AssetName.substr(0, dotPos);

	ViewerState* NewState = CreateViewerState(AssetName.c_str(), Context);
	if (NewState)
	{
		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;
	}
}

void SPhysicsAssetEditorWindow::OnRender()
{
	// If window is closed, request cleanup and don't render
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
		return;
	}

	// Parent detachable window (movable, top-level) with solid background
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
	}

	// Generate a unique window title to pass to ImGui
	char UniqueTitle[256];
	FString Title = GetWindowTitle();
	if (Tabs.Num() == 1 && ActiveState)
	{
		PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(ActiveState);
		if (!PhysState->CurrentFilePath.empty())
		{
			std::filesystem::path fsPath(UTF8ToWide(PhysState->CurrentFilePath));
			FString AssetName = WideToUTF8(fsPath.filename().wstring());
			Title += " - " + AssetName;
		}
	}
	sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

	bool bViewerVisible = false;
	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		bViewerVisible = true;

		// 입력 라우팅을 위한 hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// Render tab bar and switch active state
		RenderTabsAndToolbar(EViewerType::PhysicsAsset);

		// 마지막 탭을 닫은 경우 렌더링 중단
		if (!bIsOpen)
		{
			USlateManager::GetInstance().RequestCloseDetachedWindow(this);
			ImGui::End();
			return;
		}

		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		float splitterWidth = 4.0f; // 분할선 두께

		float leftWidth = totalWidth * LeftPanelRatio;
		float rightWidth = totalWidth * RightPanelRatio;
		float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);

		// 중앙 패널이 음수가 되지 않도록 보정 (안전장치)
		if (centerWidth < 0.0f)
		{
			centerWidth = 0.0f;
		}

		// Remove spacing between panels
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// Left panel - Skeleton Tree & Bodies
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleVar();
		RenderLeftPanel(leftWidth);
		ImGui::EndChild();

		ImGui::SameLine(0, 0); // No spacing between panels

		// Left splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newLeftRatio = LeftPanelRatio + delta / totalWidth;
				float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
				LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Center panel (viewport area)
		if (centerWidth > 0.0f)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
			ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
			ImGui::PopStyleVar();

			// 뷰어 툴바 렌더링 (뷰포트 상단)
			RenderViewerToolbar();

			// 툴바 아래 뷰포트 영역
			ImVec2 viewportPos = ImGui::GetCursorScreenPos();
			float remainingWidth = ImGui::GetContentRegionAvail().x;
			float remainingHeight = ImGui::GetContentRegionAvail().y;

			// 뷰포트 영역 설정
			CenterRect.Left = viewportPos.x;
			CenterRect.Top = viewportPos.y;
			CenterRect.Right = viewportPos.x + remainingWidth;
			CenterRect.Bottom = viewportPos.y + remainingHeight;
			CenterRect.UpdateMinMax();

			// 뷰포트 렌더링 (텍스처에)
			OnRenderViewport();

			// ImGui::Image로 결과 텍스처 표시
			if (ActiveState && ActiveState->Viewport)
			{
				ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
				if (SRV)
				{
					ImGui::Image((void*)SRV, ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
				}
				else
				{
					ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(false);
				}
			}
			else
			{
				ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
			}

			ImGui::EndChild(); // CenterPanel

			ImGui::SameLine(0, 0);
		}
		else
		{
			CenterRect = FRect(0, 0, 0, 0);
			CenterRect.UpdateMinMax();
		}

		// Right splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newRightRatio = RightPanelRatio - delta / totalWidth;
				float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
				RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Right panel - Properties
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
		ImGui::PopStyleVar();
		RenderRightPanel();
		ImGui::EndChild();

		// Pop the ItemSpacing style
		ImGui::PopStyleVar();
	}
	ImGui::End();

	// If collapsed or not visible, clear the center rect
	if (!bViewerVisible)
	{
		CenterRect = FRect(0, 0, 0, 0);
		CenterRect.UpdateMinMax();
		bIsWindowHovered = false;
		bIsWindowFocused = false;
	}

	// If window was closed via X button, notify the manager to clean up
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
	}

	bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	// Sub Widget 에디터 상태 업데이트
	UpdateSubWidgetEditorState();

	// Sub Widget Update 호출
	if (SkeletonTreeWidget) SkeletonTreeWidget->Update();
	if (BodyPropertiesWidget) BodyPropertiesWidget->Update();
	if (ConstraintPropertiesWidget) ConstraintPropertiesWidget->Update();

	// Shape 프리뷰 업데이트
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (State)
	{
		// ─────────────────────────────────────────────────
		// 기즈모 연동: 선택된 바디의 트랜스폼 편집
		// ─────────────────────────────────────────────────
		if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0 && State->World)
		{
			AGizmoActor* Gizmo = State->World->GetGizmoActor();
			bool bCurrentlyDragging = Gizmo && Gizmo->GetbIsDragging();

			// 드래그 첫 프레임인지 확인 (World→Local 변환 오차 방지)
			bool bIsFirstDragFrame = bCurrentlyDragging && !State->bWasGizmoDragging;

			if (bCurrentlyDragging && !bIsFirstDragFrame)
			{
				// 첫 프레임이 아닐 때만 기즈모로부터 트랜스폼 업데이트
				UpdateBodyTransformFromGizmo();
			}
			else if (!bCurrentlyDragging)
			{
				// 드래그 중이 아니면 기즈모 앵커를 바디 위치로 이동
				RepositionAnchorToBody(State->SelectedBodyIndex);
			}

			// 드래그 상태 업데이트 (다음 프레임에서 첫 프레임 감지용)
			State->bWasGizmoDragging = bCurrentlyDragging;
		}
		else
		{
			State->bWasGizmoDragging = false;
		}

		// 라인 재구성이 필요한 경우 (바디/제약조건 추가/제거)
		if (State->bShapeLinesNeedRebuild)
		{
			RebuildShapeLines();
			State->bShapeLinesNeedRebuild = false;
			State->bSelectedBodyLinesNeedUpdate = false;  // 전체 재구성 시 개별 업데이트 불필요
			State->bSelectionColorDirty = false;  // 재구성 시 색상도 업데이트됨
			State->CachedSelectedBodyIndex = State->SelectedBodyIndex;
			State->CachedSelectedConstraintIndex = State->SelectedConstraintIndex;
		}
		// 선택된 바디의 속성만 변경된 경우 (좌표만 업데이트, new/delete 없음)
		else if (State->bSelectedBodyLinesNeedUpdate)
		{
			UpdateSelectedBodyLines();
			State->bSelectedBodyLinesNeedUpdate = false;
		}
		// 선택만 변경된 경우 (색상만 업데이트)
		else if (State->bSelectionColorDirty)
		{
			UpdateSelectionColors();
			State->bSelectionColorDirty = false;
			State->CachedSelectedBodyIndex = State->SelectedBodyIndex;
			State->CachedSelectedConstraintIndex = State->SelectedConstraintIndex;
		}
	}
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
	// 뷰포트 렌더 전 처리
}

void SPhysicsAssetEditorWindow::OnSave()
{
	SavePhysicsAsset();
}

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	// 탭바만 렌더링 (Skeletal Mesh 뷰어 전환 버튼 제외 - SViewerWindow 베이스 호출 안 함)
	if (!ImGui::BeginTabBar("PhysicsAssetEditorTabs",
		ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		return;

	// 탭 렌더링
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorState* PState = static_cast<PhysicsAssetEditorState*>(State);
		bool open = true;

		// 동적 탭 이름 생성
		FString TabDisplayName;
		if (PState->CurrentFilePath.empty())
		{
			TabDisplayName = "Untitled";
		}
		else
		{
			// 파일 경로에서 파일명만 추출 (확장자 제외)
			size_t lastSlash = PState->CurrentFilePath.find_last_of("/\\");
			FString filename = (lastSlash != FString::npos)
				? PState->CurrentFilePath.substr(lastSlash + 1)
				: PState->CurrentFilePath;

			// 확장자 제거
			size_t dotPos = filename.find_last_of('.');
			if (dotPos != FString::npos)
				filename = filename.substr(0, dotPos);

			TabDisplayName = filename;
		}

		// 수정됨 표시 추가
		if (PState->bIsDirty)
		{
			TabDisplayName += "*";
		}

		// ImGui ID 충돌 방지
		TabDisplayName += "##";
		TabDisplayName += State->Name.ToString();

		if (ImGui::BeginTabItem(TabDisplayName.c_str(), &open))
		{
			ActiveTabIndex = i;
			ActiveState = State;
			ImGui::EndTabItem();
		}
		if (!open)
		{
			CloseTab(i);
			ImGui::EndTabBar();
			return;
		}
	}

	// + 버튼 (새 탭 추가)
	if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
	{
		int maxViewerNum = 0;
		for (int i = 0; i < Tabs.Num(); ++i)
		{
			const FString& tabName = Tabs[i]->Name.ToString();
			const char* prefix = "PhysicsAssetTab";
			if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
			{
				const char* numberPart = tabName.c_str() + strlen(prefix);
				int num = atoi(numberPart);
				if (num > maxViewerNum)
				{
					maxViewerNum = num;
				}
			}
		}

		char label[64];
		sprintf_s(label, "PhysicsAssetTab%d", maxViewerNum + 1);
		OpenNewTab(label);
	}
	ImGui::EndTabBar();

	// Physics Asset Editor 전용 툴바
	RenderToolbar();
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	ImGuiStyle& style = ImGui::GetStyle();

	// ─────────────────────────────────────────────────
	// ASSET BROWSER 섹션 (메시 로드)
	// ─────────────────────────────────────────────────
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
	ImGui::Text("ASSET BROWSER");
	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));

	ImGui::PushItemWidth(-1.0f);
	ImGui::InputTextWithHint("##MeshPath", "Browse for FBX file...", State->MeshPathBuffer, sizeof(State->MeshPathBuffer));
	ImGui::PopItemWidth();
	ImGui::Dummy(ImVec2(0, 4));

	float innerPadding = 8.0f;
	float availWidth = PanelWidth - innerPadding * 2.0f;
	float buttonHeight = 30.0f;
	float buttonWidth = (availWidth - 6.0f) * 0.5f;

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.25f, 0.27f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.30f, 0.33f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.19f, 0.21f, 0.23f, 1.00f));
	if (ImGui::Button("Browse...", ImVec2(buttonWidth, buttonHeight)))
	{
		auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
		if (!widePath.empty())
		{
			std::string s = WideToUTF8(widePath.wstring());
			strncpy_s(State->MeshPathBuffer, s.c_str(), sizeof(State->MeshPathBuffer) - 1);
		}
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0.0f, 4.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.44f, 0.63f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.51f, 0.72f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.36f, 0.51f, 1.00f));
	if (ImGui::Button("Load FBX", ImVec2(buttonWidth, buttonHeight)))
	{
		FString Path = State->MeshPathBuffer;
		if (!Path.empty())
		{
			USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
			if (Mesh && State->PreviewActor)
			{
				State->PreviewActor->SetSkeletalMesh(Path);
				State->CurrentMesh = Mesh;
				State->LoadedMeshPath = Path;

				// Physics Asset에 메시 경로 저장
				if (State->EditingAsset)
				{
					State->EditingAsset->SkeletalMeshPath = Path;
					State->bIsDirty = true;
				}

				OnSkeletalMeshLoaded(State, Path);
			}
		}
	}
	ImGui::PopStyleColor(3);

	ImGui::Dummy(ImVec2(0, 8));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 8));

	// ─────────────────────────────────────────────────
	// DISPLAY OPTIONS 섹션
	// ─────────────────────────────────────────────────
	ImGui::Text("DISPLAY OPTIONS");
	ImGui::Dummy(ImVec2(0, 4));

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5f, 1.5f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f));
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

	if (ImGui::Checkbox("Show Mesh", &State->bShowMesh))
	{
		if (auto* comp = State->PreviewActor->GetSkeletalMeshComponent())
			comp->SetVisibility(State->bShowMesh);
	}
	ImGui::SameLine(0.0f, 12.0f);
	if (ImGui::Checkbox("Show Bodies", &State->bShowBodies))
	{
		if (State->BodyPreviewLineComponent)
			State->BodyPreviewLineComponent->SetLineVisible(State->bShowBodies);
	}
	ImGui::SameLine(0.0f, 12.0f);
	if (ImGui::Checkbox("Show Constraints", &State->bShowConstraints))
	{
		if (State->ConstraintPreviewLineComponent)
			State->ConstraintPreviewLineComponent->SetLineVisible(State->bShowConstraints);
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	ImGui::Dummy(ImVec2(0, 8));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 8));

	// ─────────────────────────────────────────────────
	// 남은 높이 계산 (Skeleton 60%, Graph 40%)
	// ─────────────────────────────────────────────────
	float remainingHeight = ImGui::GetContentRegionAvail().y;
	float graphSectionHeight = remainingHeight * 0.4f;
	float skeletonHeight = remainingHeight * 0.6f - 40.0f;

	// ─────────────────────────────────────────────────
	// SKELETON 섹션 (본 계층 구조만 표시)
	// ─────────────────────────────────────────────────
	ImGui::Text("SKELETON");
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	if (skeletonHeight > 50.0f)
	{
		ImGui::BeginChild("SkeletonTreeScroll", ImVec2(0, skeletonHeight), false);
		if (SkeletonTreeWidget) SkeletonTreeWidget->RenderWidget();
		ImGui::EndChild();
	}

	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	// ─────────────────────────────────────────────────
	// GRAPH 섹션 (Bodies & Constraints 그래프)
	// ─────────────────────────────────────────────────
	ImGui::Text("GRAPH (%d Bodies, %d Constraints)",
		State->EditingAsset ? (int)State->EditingAsset->BodySetups.size() : 0,
		State->EditingAsset ? (int)State->EditingAsset->ConstraintSetups.size() : 0);
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	float graphHeight = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("GraphScroll", ImVec2(0, graphHeight), false);

	if (State->EditingAsset)
	{
		// Bodies 리스트
		if (ImGui::TreeNodeEx("Bodies", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
			{
				UBodySetup* Body = State->EditingAsset->BodySetups[i];
				if (!Body) continue;

				bool bSelected = (State->bBodySelectionMode && State->SelectedBodyIndex == i);

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;

				// Shape 개수 표시
				int32 ShapeCount = Body->AggGeom.GetElementCount();

				ImGui::PushStyleColor(ImGuiCol_Text, bSelected ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
				bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)(100 + i), flags, "[%d] %s", ShapeCount, Body->BoneName.ToString().c_str());
				ImGui::PopStyleColor();

				if (ImGui::IsItemClicked())
				{
					State->SelectBody(i);
				}
				if (bOpen) ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		ImGui::Spacing();

		// Constraints 리스트
		if (ImGui::TreeNodeEx("Constraints", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()); ++i)
			{
				const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];
				bool bSelected = (!State->bBodySelectionMode && State->SelectedConstraintIndex == i);

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;

				FString ParentName = "?";
				FString ChildName = "?";
				if (Constraint.ParentBodyIndex >= 0 && Constraint.ParentBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
				{
					UBodySetup* ParentBody = State->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
					if (ParentBody) ParentName = ParentBody->BoneName.ToString();
				}
				if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
				{
					UBodySetup* ChildBody = State->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
					if (ChildBody) ChildName = ChildBody->BoneName.ToString();
				}

				ImGui::PushStyleColor(ImGuiCol_Text, bSelected ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.7f, 0.4f, 1.0f));
				bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)(200 + i), flags, "%s -> %s", ParentName.c_str(), ChildName.c_str());
				ImGui::PopStyleColor();

				if (ImGui::IsItemClicked())
				{
					State->SelectConstraint(i);
				}
				if (bOpen) ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (State->EditingAsset->BodySetups.empty() && State->EditingAsset->ConstraintSetups.empty())
		{
			ImGui::TextDisabled("No bodies or constraints");
			ImGui::TextDisabled("Use 'Auto Generate' to create");
		}
	}

	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	// 패널 헤더
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
	ImGui::Text("PROPERTIES");
	ImGui::PopStyleColor();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 선택된 바디 또는 제약 조건의 속성 표시 (Sub Widget 인스턴스 사용)
	if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0)
	{
		if (BodyPropertiesWidget) BodyPropertiesWidget->RenderWidget();
	}
	else if (!State->bBodySelectionMode && State->SelectedConstraintIndex >= 0)
	{
		if (ConstraintPropertiesWidget) ConstraintPropertiesWidget->RenderWidget();
	}
	else
	{
		ImGui::TextDisabled("Select a body or constraint to edit properties");
	}
}

void SPhysicsAssetEditorWindow::RenderBottomPanel()
{
	// 더 이상 사용하지 않음 - Graph가 좌측 패널로 이동됨
}

void SPhysicsAssetEditorWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
	PhysicsAssetEditorState* PhysicsState = static_cast<PhysicsAssetEditorState*>(State);
	if (!PhysicsState || !PhysicsState->EditingAsset) return;

	// Physics Asset에 스켈레탈 메시 경로 저장
	PhysicsState->EditingAsset->SkeletalMeshPath = Path;
	PhysicsState->bIsDirty = true;
	PhysicsState->RequestLinesRebuild();

	UE_LOG("[SPhysicsAssetEditorWindow] Skeletal mesh loaded: %s", Path.c_str());
}

// ────────────────────────────────────────────────────────────────
// 툴바
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RenderToolbar()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 저장 버튼
	if (ImGui::Button("Save"))
	{
		SavePhysicsAsset();
	}
	ImGui::SameLine();

	if (ImGui::Button("Save As"))
	{
		SavePhysicsAssetAs();
	}
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 자동 생성 버튼
	bool bCanAutoGenerate = State->CurrentMesh != nullptr;
	if (!bCanAutoGenerate) ImGui::BeginDisabled();
	if (ImGui::Button("Auto-Generate"))
	{
		AutoGenerateBodies();
	}
	if (!bCanAutoGenerate) ImGui::EndDisabled();
	ImGui::SameLine();

	// 바디 추가/제거 버튼
	bool bCanAddBody = State->SelectedBoneIndex >= 0;
	if (!bCanAddBody) ImGui::BeginDisabled();
	if (ImGui::Button("Add Body"))
	{
		AddBodyToBone(State->SelectedBoneIndex);
	}
	if (!bCanAddBody) ImGui::EndDisabled();
	ImGui::SameLine();

	bool bCanRemoveBody = State->bBodySelectionMode && State->SelectedBodyIndex >= 0;
	if (!bCanRemoveBody) ImGui::BeginDisabled();
	if (ImGui::Button("Remove Body"))
	{
		RemoveSelectedBody();
	}
	if (!bCanRemoveBody) ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 제약 조건 제거 버튼
	bool bCanRemoveConstraint = !State->bBodySelectionMode && State->SelectedConstraintIndex >= 0;
	if (!bCanRemoveConstraint) ImGui::BeginDisabled();
	if (ImGui::Button("Remove Constraint"))
	{
		RemoveSelectedConstraint();
	}
	if (!bCanRemoveConstraint) ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 표시 옵션
	if (ImGui::Checkbox("Bodies", &State->bShowBodies))
	{
		if (State->BodyPreviewLineComponent)
			State->BodyPreviewLineComponent->SetLineVisible(State->bShowBodies);
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Constraints", &State->bShowConstraints))
	{
		if (State->ConstraintPreviewLineComponent)
			State->ConstraintPreviewLineComponent->SetLineVisible(State->bShowConstraints);
	}
}

// ────────────────────────────────────────────────────────────────
// Shape 라인 관리
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RebuildShapeLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	ULineComponent* BodyLineComp = State->BodyPreviewLineComponent;
	ULineComponent* ConstraintLineComp = State->ConstraintPreviewLineComponent;

	// ─────────────────────────────────────────────────
	// 바디 라인 재구성 (FLinesBatch 기반 - DOD)
	// ─────────────────────────────────────────────────
	State->BodyLinesBatch.Clear();
	State->BodyLineRanges.Empty();
	State->BodyLineRanges.SetNum(static_cast<int32>(State->EditingAsset->BodySetups.size()));

	// 예상 라인 수 예약 (대략 바디당 40개 라인)
	State->BodyLinesBatch.Reserve(static_cast<int32>(State->EditingAsset->BodySetups.size()) * 40);

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();

	for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
	{
		UBodySetup* Body = State->EditingAsset->BodySetups[i];
		if (!Body) continue;

		// 바디 라인 범위 시작
		PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[i];
		Range.StartIndex = State->BodyLinesBatch.Num();

		// 선택된 바디는 다른 색상
		FVector4 Color = (State->bBodySelectionMode && State->SelectedBodyIndex == i)
			? FVector4(0.0f, 1.0f, 0.0f, 1.0f)  // 선택: 녹색
			: FVector4(0.0f, 0.5f, 1.0f, 1.0f); // 기본: 파랑

		// 본 트랜스폼 가져오기
		FTransform BoneTransform;
		if (MeshComp && Body->BoneIndex >= 0)
		{
			BoneTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
		}

		// 헬퍼 함수로 좌표 생성 (AggGeom의 모든 Shape 처리)
		TArray<FVector> StartPoints, EndPoints;
		GenerateBodyShapeLineCoordinates(Body, BoneTransform, StartPoints, EndPoints);

		// FLinesBatch에 직접 추가 (NewObject 없음!)
		for (int32 j = 0; j < StartPoints.Num(); ++j)
		{
			State->BodyLinesBatch.Add(StartPoints[j], EndPoints[j], Color);
		}

		// 바디 라인 범위 종료
		Range.Count = State->BodyLinesBatch.Num() - Range.StartIndex;
	}

	// ULineComponent에 배치 데이터 설정
	if (BodyLineComp)
	{
		BodyLineComp->ClearLines();  // 기존 ULine 정리
		BodyLineComp->SetDirectBatch(State->BodyLinesBatch);
		BodyLineComp->SetLineVisible(State->bShowBodies);
	}

	// ─────────────────────────────────────────────────
	// 제약 조건 라인 재구성 (FLinesBatch 기반 - DOD)
	// ─────────────────────────────────────────────────
	State->ConstraintLinesBatch.Clear();
	State->ConstraintLinesBatch.Reserve(static_cast<int32>(State->EditingAsset->ConstraintSetups.size()));

	for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()); ++i)
	{
		const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];
		if (Constraint.ParentBodyIndex < 0 || Constraint.ChildBodyIndex < 0) continue;
		if (Constraint.ParentBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;
		if (Constraint.ChildBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;

		FVector4 Color = (!State->bBodySelectionMode && State->SelectedConstraintIndex == i)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f) : FVector4(1.0f, 0.5f, 0.0f, 1.0f);

		FVector ParentPos, ChildPos;
		UBodySetup* ParentBody = State->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
		UBodySetup* ChildBody = State->EditingAsset->BodySetups[Constraint.ChildBodyIndex];

		if (MeshComp && ParentBody && ChildBody)
		{
			ParentPos = MeshComp->GetBoneWorldTransform(ParentBody->BoneIndex).Translation;
			ChildPos = MeshComp->GetBoneWorldTransform(ChildBody->BoneIndex).Translation;
		}

		State->ConstraintLinesBatch.Add(ParentPos, ChildPos, Color);
	}

	// ULineComponent에 배치 데이터 설정
	if (ConstraintLineComp)
	{
		ConstraintLineComp->ClearLines();  // 기존 ULine 정리
		ConstraintLineComp->SetDirectBatch(State->ConstraintLinesBatch);
		ConstraintLineComp->SetLineVisible(State->bShowConstraints);
	}
}

void SPhysicsAssetEditorWindow::UpdateSelectedBodyLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedBodyIndex < 0) return;
	if (State->SelectedBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) return;
	if (State->SelectedBodyIndex >= State->BodyLineRanges.Num()) return;

	UBodySetup* Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
	if (!Body) return;
	const PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[State->SelectedBodyIndex];

	// 본 트랜스폼 가져오기
	FTransform BoneTransform;
	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	if (MeshComp && Body->BoneIndex >= 0)
	{
		BoneTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}

	// 헬퍼 함수로 좌표 생성 (AggGeom의 모든 Shape 처리)
	TArray<FVector> StartPoints, EndPoints;
	GenerateBodyShapeLineCoordinates(Body, BoneTransform, StartPoints, EndPoints);

	// FLinesBatch 직접 업데이트 (범위 내 라인 좌표만 변경)
	for (int32 i = 0; i < Range.Count && i < StartPoints.Num(); ++i)
	{
		State->BodyLinesBatch.SetLine(Range.StartIndex + i, StartPoints[i], EndPoints[i]);
	}

	// ULineComponent에 갱신된 배치 데이터 재설정
	if (ULineComponent* BodyLineComp = State->BodyPreviewLineComponent)
	{
		BodyLineComp->SetDirectBatch(State->BodyLinesBatch);
	}
}

void SPhysicsAssetEditorWindow::UpdateSelectionColors()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	const FVector4 SelectedColor(0.0f, 1.0f, 0.0f, 1.0f);   // 녹색
	const FVector4 NormalColor(0.0f, 0.5f, 1.0f, 1.0f);     // 파랑
	const FVector4 SelectedConstraintColor(1.0f, 1.0f, 0.0f, 1.0f);  // 노랑
	const FVector4 NormalConstraintColor(1.0f, 0.5f, 0.0f, 1.0f);    // 주황

	bool bBodyBatchDirty = false;
	bool bConstraintBatchDirty = false;

	// 헬퍼 람다: 특정 바디의 라인 색상 업데이트 (FLinesBatch 기반)
	auto UpdateBodyColor = [&](int32 BodyIndex, const FVector4& Color)
	{
		if (BodyIndex >= 0 && BodyIndex < State->BodyLineRanges.Num())
		{
			const PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[BodyIndex];
			State->BodyLinesBatch.SetColorRange(Range.StartIndex, Range.Count, Color);
			bBodyBatchDirty = true;
		}
	};

	// 헬퍼 람다: 특정 제약조건의 라인 색상 업데이트 (FLinesBatch 기반)
	auto UpdateConstraintColor = [&](int32 ConstraintIndex, const FVector4& Color)
	{
		if (ConstraintIndex >= 0 && ConstraintIndex < State->ConstraintLinesBatch.Num())
		{
			State->ConstraintLinesBatch.SetColor(ConstraintIndex, Color);
			bConstraintBatchDirty = true;
		}
	};

	// 바디 선택 변경 처리 (이전 선택 → 비선택, 새 선택 → 선택)
	if (State->CachedSelectedBodyIndex != State->SelectedBodyIndex)
	{
		// 이전에 선택된 바디를 일반 색상으로
		UpdateBodyColor(State->CachedSelectedBodyIndex, NormalColor);

		// 새로 선택된 바디를 선택 색상으로 (바디 선택 모드일 때만)
		if (State->bBodySelectionMode)
		{
			UpdateBodyColor(State->SelectedBodyIndex, SelectedColor);
		}
	}

	// 제약조건 선택 변경 처리
	if (State->CachedSelectedConstraintIndex != State->SelectedConstraintIndex)
	{
		// 이전에 선택된 제약조건을 일반 색상으로
		UpdateConstraintColor(State->CachedSelectedConstraintIndex, NormalConstraintColor);

		// 새로 선택된 제약조건을 선택 색상으로 (제약조건 선택 모드일 때만)
		if (!State->bBodySelectionMode)
		{
			UpdateConstraintColor(State->SelectedConstraintIndex, SelectedConstraintColor);
		}
	}

	// ULineComponent에 갱신된 배치 데이터 재설정
	if (bBodyBatchDirty && State->BodyPreviewLineComponent)
	{
		State->BodyPreviewLineComponent->SetDirectBatch(State->BodyLinesBatch);
	}
	if (bConstraintBatchDirty && State->ConstraintPreviewLineComponent)
	{
		State->ConstraintPreviewLineComponent->SetDirectBatch(State->ConstraintLinesBatch);
	}
}

// ────────────────────────────────────────────────────────────────
// 파일 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	if (State->CurrentFilePath.empty())
	{
		SavePhysicsAssetAs();
		return;
	}

	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, State->CurrentFilePath))
	{
		State->bIsDirty = false;
	}
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
	// 파일 다이얼로그 (추후 구현)
	// 현재는 기본 경로로 저장
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	FString DefaultPath = "Data/PhysicsAssets/NewPhysicsAsset.json";
	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, DefaultPath))
	{
		State->CurrentFilePath = DefaultPath;
		State->bIsDirty = false;
	}
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
	// 파일 다이얼로그 (추후 구현)
}

// ────────────────────────────────────────────────────────────────
// 바디/제약 조건 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::AutoGenerateBodies()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->CurrentMesh) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return;

	UPhysicsAsset* Asset = State->EditingAsset;

	// 기존 상태 완전 초기화
	Asset->ClearAll();
	State->BodyLinesBatch.Clear();
	State->BodyLineRanges.Empty();
	State->ConstraintLinesBatch.Clear();
	State->CachedSelectedBodyIndex = -1;
	State->CachedSelectedConstraintIndex = -1;

	// 본 길이 계산을 위한 자식 본 맵 구축
	TArray<TArray<int32>> ChildrenMap;
	ChildrenMap.SetNum(static_cast<int32>(Skeleton->Bones.size()));
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentIdx >= 0 && ParentIdx < static_cast<int32>(Skeleton->Bones.size()))
		{
			ChildrenMap[ParentIdx].Add(i);
		}
	}

	// 모든 본에 대해 바디 생성
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		const FBone& Bone = Skeleton->Bones[i];

		// 본 위치 (BindPose 행렬의 Translation 부분: M[3][0..2])
		FVector BonePos(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);

		// 본 길이 계산 (자식 본까지의 거리 또는 기본값)
		float BoneLength = 0.05f; // 기본값 (리프 본용)
		if (!ChildrenMap[i].IsEmpty())
		{
			// 첫 번째 자식까지의 거리 계산
			int32 ChildIdx = ChildrenMap[i][0];
			const FBone& ChildBone = Skeleton->Bones[ChildIdx];
			FVector ChildPos(ChildBone.BindPose.M[3][0], ChildBone.BindPose.M[3][1], ChildBone.BindPose.M[3][2]);
			BoneLength = (ChildPos - BonePos).Size();
		}

		// 최소/최대 크기 제한 (더 보수적으로)
		BoneLength = std::max(0.02f, std::min(BoneLength, 0.5f));

		// 바디 생성
		int32 BodyIndex = Asset->AddBody(Bone.Name, i);
		if (BodyIndex >= 0)
		{
			UBodySetup* Body = Asset->BodySetups[BodyIndex];
			if (Body)
			{
				// AggGeom에 캡슐 Shape 추가
				FKSphylElem NewCapsule;
				NewCapsule.Radius = BoneLength * 0.1f;       // 본 길이의 10%를 반지름으로
				NewCapsule.Length = BoneLength * 0.7f;       // 본 길이의 70%를 전체 길이로
				Body->AggGeom.SphylElems.Add(NewCapsule);
			}
		}
	}

	// 부모-자식 본 사이에 제약 조건 생성
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentBoneIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentBoneIdx < 0) continue;

		int32 ChildBodyIdx = Asset->FindBodyIndexByBone(i);
		int32 ParentBodyIdx = Asset->FindBodyIndexByBone(ParentBoneIdx);

		if (ChildBodyIdx >= 0 && ParentBodyIdx >= 0)
		{
			char JointName[128];
			sprintf_s(JointName, "Joint_%s", Skeleton->Bones[i].Name.c_str());
			Asset->AddConstraint(FName(JointName), ParentBodyIdx, ChildBodyIdx);
		}
	}

	State->bIsDirty = true;
	State->RequestLinesRebuild();
	State->ClearSelection();
	State->HideGizmo();

	UE_LOG("[SPhysicsAssetEditorWindow] Auto-generated %d bodies and %d constraints",
		(int)Asset->BodySetups.size(), (int)Asset->ConstraintSetups.size());
}

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->CurrentMesh) return;
	if (BoneIndex < 0) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	// 이미 바디가 있는지 확인
	if (State->EditingAsset->FindBodyIndexByBone(BoneIndex) >= 0)
	{
		UE_LOG("[SPhysicsAssetEditorWindow] Bone already has a body");
		return;
	}

	FName BoneName = Skeleton->Bones[BoneIndex].Name;
	int32 NewBodyIndex = State->EditingAsset->AddBody(BoneName, BoneIndex);

	if (NewBodyIndex >= 0)
	{
		// 부모 본의 바디 찾기
		int32 ParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		if (ParentBoneIndex >= 0)
		{
			int32 ParentBodyIndex = State->EditingAsset->FindBodyIndexByBone(ParentBoneIndex);
			if (ParentBodyIndex >= 0)
			{
				// 자동으로 제약 조건 생성
				char JointNameBuf[128];
				sprintf_s(JointNameBuf, "Joint_%s", BoneName.ToString().c_str());
				State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, NewBodyIndex);
			}
		}

		State->SelectBody(NewBodyIndex);
		State->bIsDirty = true;
		State->RequestLinesRebuild();
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedBody()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedBodyIndex < 0) return;

	if (State->EditingAsset->RemoveBody(State->SelectedBodyIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->RequestLinesRebuild();
		State->HideGizmo();
	}
}

void SPhysicsAssetEditorWindow::AddConstraintBetweenBodies(int32 ParentBodyIndex, int32 ChildBodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	char JointNameBuf[64];
	sprintf_s(JointNameBuf, "Constraint_%d_%d", ParentBodyIndex, ChildBodyIndex);
	int32 NewIndex = State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, ChildBodyIndex);

	if (NewIndex >= 0)
	{
		State->SelectConstraint(NewIndex);
		State->bIsDirty = true;
		State->RequestLinesRebuild();
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedConstraint()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedConstraintIndex < 0) return;

	if (State->EditingAsset->RemoveConstraint(State->SelectedConstraintIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->RequestLinesRebuild();
		State->HideGizmo();
	}
}

// ────────────────────────────────────────────────────────────────
// 마우스 입력 (베이스 클래스의 bone picking 제거)
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport) return;

	// 현재 뷰포트가 호버되어 있어야 마우스 다운 처리 (Z-order 고려)
	if (!ActiveState->Viewport->IsViewportHovered()) return;

	if (CenterRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

		// ─────────────────────────────────────────────
		// 타이밍 측정 시작
		// ─────────────────────────────────────────────
		auto T0 = std::chrono::high_resolution_clock::now();

		// 기즈모 피킹 수행
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

		auto T1 = std::chrono::high_resolution_clock::now();

		// 좌클릭: 바디 피킹
		if (Button == 0)
		{
			PhysicsAssetEditorState* State = GetActivePhysicsState();
			if (State && State->EditingAsset && State->PreviewActor && State->Client && State->World)
			{
				// 기즈모가 선택되었는지 확인
				UActorComponent* SelectedComp = State->World->GetSelectionManager()->GetSelectedComponent();
				bool bGizmoSelected = SelectedComp && Cast<UBoneAnchorComponent>(SelectedComp);

				// 기즈모가 선택되지 않은 경우에만 바디 피킹
				if (!bGizmoSelected)
				{
					ACameraActor* Camera = State->Client->GetCamera();
					if (Camera)
					{
						// 카메라 벡터
						FVector CameraPos = Camera->GetActorLocation();
						FVector CameraRight = Camera->GetRight();
						FVector CameraUp = Camera->GetUp();
						FVector CameraForward = Camera->GetForward();

						// 뷰포트 마우스 위치
						FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
						FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

						// 레이 생성
						FRay Ray = MakeRayFromViewport(
							Camera->GetViewMatrix(),
							Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), State->Viewport),
							CameraPos,
							CameraRight,
							CameraUp,
							CameraForward,
							ViewportMousePos,
							ViewportSize
						);

						auto T2 = std::chrono::high_resolution_clock::now();

						// 모든 바디에 대해 레이캐스트
						int32 ClosestBodyIndex = -1;
						float ClosestT = FLT_MAX;

						USkeletalMeshComponent* SkelComp = State->GetPreviewMeshComponent();

						if (SkelComp)
						{
							for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
							{
								UBodySetup* Body = State->EditingAsset->BodySetups[i];
								if (!Body || Body->BoneIndex < 0) continue;

								// 본 월드 트랜스폼
								FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(Body->BoneIndex);

								float HitT;
								if (IntersectRayBody(Ray, Body, BoneWorldTransform, HitT))
								{
									if (HitT < ClosestT)
									{
										ClosestT = HitT;
										ClosestBodyIndex = i;
									}
								}
							}
						}

						auto T3 = std::chrono::high_resolution_clock::now();

						if (ClosestBodyIndex >= 0)
						{
							// 바디 선택 (색상만 업데이트 - 전체 rebuild 불필요)
							State->bBodySelectionMode = true;
							State->SelectedBodyIndex = ClosestBodyIndex;
							State->SelectedConstraintIndex = -1;
							State->bSelectionColorDirty = true;

							// 기즈모 위치 업데이트
							RepositionAnchorToBody(ClosestBodyIndex);
						}
						else
						{
							// 빈 공간 클릭: 선택 해제 및 기즈모 숨기기
							State->ClearSelection();
							State->HideGizmo();
						}

						auto T4 = std::chrono::high_resolution_clock::now();

						// ─────────────────────────────────────────────
						// 타이밍 출력
						// ─────────────────────────────────────────────
						double Ms1 = std::chrono::duration<double, std::milli>(T1 - T0).count();
						double Ms2 = std::chrono::duration<double, std::milli>(T2 - T1).count();
						double Ms3 = std::chrono::duration<double, std::milli>(T3 - T2).count();
						double Ms4 = std::chrono::duration<double, std::milli>(T4 - T3).count();
						double MsTotal = std::chrono::duration<double, std::milli>(T4 - T0).count();

						char buf[256];
						sprintf_s(buf, "[PhysicsAssetEditor Pick] ProcessMouseButtonDown: %.2f ms | RaySetup: %.2f ms | BodyPicking: %.2f ms | Selection: %.2f ms | Total: %.2f ms",
							Ms1, Ms2, Ms3, Ms4, MsTotal);
						UE_LOG(buf);
					}
				}
			}

			bLeftMousePressed = true;
		}

		// 우클릭: 카메라 조작 시작
		if (Button == 1)
		{
			UInputManager::GetInstance().SetCursorVisible(false);
			UInputManager::GetInstance().LockCursor();
			bRightMousePressed = true;
		}
	}
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport) return;

	FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
	ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

	// 좌클릭 해제
	if (Button == 0)
	{
		bLeftMousePressed = false;
	}

	// 우클릭 해제: 카메라 조작 종료
	if (Button == 1)
	{
		UInputManager::GetInstance().SetCursorVisible(true);
		UInputManager::GetInstance().ReleaseCursor();
		bRightMousePressed = false;
	}
}

// ────────────────────────────────────────────────────────────────
// Sub Widget 관리
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::CreateSubWidgets()
{
	SkeletonTreeWidget = NewObject<USkeletonTreeWidget>();
	SkeletonTreeWidget->Initialize();

	BodyPropertiesWidget = NewObject<UBodyPropertiesWidget>();
	BodyPropertiesWidget->Initialize();

	ConstraintPropertiesWidget = NewObject<UConstraintPropertiesWidget>();
	ConstraintPropertiesWidget->Initialize();
}

void SPhysicsAssetEditorWindow::DestroySubWidgets()
{
	if (SkeletonTreeWidget)
	{
		DeleteObject(SkeletonTreeWidget);
		SkeletonTreeWidget = nullptr;
	}
	if (BodyPropertiesWidget)
	{
		DeleteObject(BodyPropertiesWidget);
		BodyPropertiesWidget = nullptr;
	}
	if (ConstraintPropertiesWidget)
	{
		DeleteObject(ConstraintPropertiesWidget);
		ConstraintPropertiesWidget = nullptr;
	}
}

void SPhysicsAssetEditorWindow::UpdateSubWidgetEditorState()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (SkeletonTreeWidget)
	{
		SkeletonTreeWidget->SetEditorState(State);
	}
	if (BodyPropertiesWidget)
	{
		BodyPropertiesWidget->SetEditorState(State);
	}
	if (ConstraintPropertiesWidget)
	{
		ConstraintPropertiesWidget->SetEditorState(State);
	}
}

// ────────────────────────────────────────────────────────────────
// 기즈모 연동
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RepositionAnchorToBody(int32 BodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->PreviewActor || !State->World)
		return;

	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size()))
		return;

	ASkeletalMeshActor* SkelActor = State->GetPreviewSkeletalActor();
	if (!SkelActor) return;

	// BoneAnchor 컴포넌트가 없으면 생성
	SkelActor->EnsureViewerComponents();

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	USceneComponent* Anchor = SkelActor->GetBoneGizmoAnchor();

	if (!MeshComp || !Anchor)
		return;

	UBodySetup* Body = State->EditingAsset->BodySetups[BodyIndex];
	if (!Body) return;

	// 바디의 월드 트랜스폼 계산: 본 월드 트랜스폼 기준
	FTransform BoneWorldTransform;
	if (Body->BoneIndex >= 0)
	{
		BoneWorldTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}
	else
	{
		BoneWorldTransform = MeshComp->GetWorldTransform();
	}

	// AggGeom의 첫 번째 Shape 중심을 기즈모 위치로 사용
	FVector ShapeCenter = FVector(0.0f, 0.0f, 0.0f);
	FQuat ShapeRotation = FQuat::Identity();
	if (!Body->AggGeom.SphereElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.SphereElems[0].Center;
	}
	else if (!Body->AggGeom.BoxElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.BoxElems[0].Center;
		ShapeRotation = Body->AggGeom.BoxElems[0].Rotation;
	}
	else if (!Body->AggGeom.SphylElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.SphylElems[0].Center;
		ShapeRotation = Body->AggGeom.SphylElems[0].Rotation;
	}

	// Shape의 로컬 트랜스폼을 본의 월드 트랜스폼으로 변환 (스케일 무시 - Shape Center는 스케일 독립적 오프셋)
	FVector WorldCenter = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeCenter);
	FQuat WorldRotation = BoneWorldTransform.Rotation * ShapeRotation;

	// 앵커를 바디의 월드 위치로 이동
	Anchor->SetWorldLocation(WorldCenter);
	Anchor->SetWorldRotation(WorldRotation);

	// 앵커 편집 가능 상태로 설정
	if (UBoneAnchorComponent* BoneAnchor = Cast<UBoneAnchorComponent>(Anchor))
	{
		BoneAnchor->SetSuppressWriteback(true);  // 본 수정 방지 (바디만 수정)
		BoneAnchor->SetEditability(true);
		BoneAnchor->SetVisibility(true);
	}

	// SelectionManager를 통해 앵커 선택 (기즈모가 앵커에 연결됨)
	USelectionManager* SelectionManager = State->World->GetSelectionManager();
	if (SelectionManager)
	{
		SelectionManager->SelectActor(State->PreviewActor);
		SelectionManager->SelectComponent(Anchor);
	}
}

void SPhysicsAssetEditorWindow::UpdateBodyTransformFromGizmo()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->PreviewActor || !State->World)
		return;

	if (State->SelectedBodyIndex < 0 ||
		State->SelectedBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size()))
		return;

	ASkeletalMeshActor* SkelActor = State->GetPreviewSkeletalActor();
	if (!SkelActor) return;

	// BoneAnchor 컴포넌트가 없으면 생성
	SkelActor->EnsureViewerComponents();

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	USceneComponent* Anchor = SkelActor->GetBoneGizmoAnchor();
	AGizmoActor* Gizmo = State->World->GetGizmoActor();

	if (!MeshComp || !Anchor || !Gizmo)
		return;

	UBodySetup* Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
	if (!Body) return;

	// 기즈모 모드 확인
	EGizmoMode CurrentGizmoMode = Gizmo->GetGizmoMode();

	// 앵커의 현재 월드 트랜스폼 (기즈모로 이동됨)
	const FTransform& AnchorWorldTransform = Anchor->GetWorldTransform();

	// 본의 월드 트랜스폼 가져오기 (바디의 부모)
	FTransform BoneWorldTransform;
	if (Body->BoneIndex >= 0)
	{
		BoneWorldTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}
	else
	{
		BoneWorldTransform = MeshComp->GetWorldTransform();
	}

	// 원하는 로컬 트랜스폼 계산: 본 월드 기준 앵커의 상대 트랜스폼 (스케일 무시 - Shape Center는 스케일 독립적 오프셋)
	FQuat InvBoneRotation = BoneWorldTransform.Rotation.Inverse();
	FVector DesiredLocalCenter = InvBoneRotation.RotateVector(AnchorWorldTransform.Translation - BoneWorldTransform.Translation);
	FQuat DesiredLocalRotation = InvBoneRotation * AnchorWorldTransform.Rotation;

	// 벡터 비교 헬퍼 람다
	auto VectorEquals = [](const FVector& A, const FVector& B, float Tolerance) -> bool
	{
		return std::abs(A.X - B.X) <= Tolerance &&
		       std::abs(A.Y - B.Y) <= Tolerance &&
		       std::abs(A.Z - B.Z) <= Tolerance;
	};

	bool bChanged = false;

	// AggGeom의 첫 번째 Shape의 Center/Rotation 업데이트
	// (기즈모는 첫 번째 Shape를 기준으로 위치함)
	switch (CurrentGizmoMode)
	{
	case EGizmoMode::Translate:
		{
			FVector NewCenter = DesiredLocalCenter;
			if (!Body->AggGeom.SphereElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.SphereElems[0].Center, 0.001f))
				{
					Body->AggGeom.SphereElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.BoxElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.BoxElems[0].Center, 0.001f))
				{
					Body->AggGeom.BoxElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.SphylElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.SphylElems[0].Center, 0.001f))
				{
					Body->AggGeom.SphylElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
		}
		break;

	case EGizmoMode::Rotate:
		{
			FQuat NewRotation = DesiredLocalRotation;
			// Box와 Capsule만 회전을 가짐 (Sphere는 회전 무의미)
			if (!Body->AggGeom.BoxElems.IsEmpty())
			{
				float Dot = std::abs(FQuat::Dot(Body->AggGeom.BoxElems[0].Rotation, NewRotation));
				if (Dot < 0.9999f)
				{
					Body->AggGeom.BoxElems[0].Rotation = NewRotation;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.SphylElems.IsEmpty())
			{
				float Dot = std::abs(FQuat::Dot(Body->AggGeom.SphylElems[0].Rotation, NewRotation));
				if (Dot < 0.9999f)
				{
					Body->AggGeom.SphylElems[0].Rotation = NewRotation;
					bChanged = true;
				}
			}
		}
		break;

	case EGizmoMode::Scale:
		// Scale 모드: Shape 크기 조절 (향후 구현 가능)
		break;

	default:
		// Select 모드 등에서는 아무것도 하지 않음
		return;
	}

	if (bChanged)
	{
		State->bIsDirty = true;
		State->RequestSelectedBodyLinesUpdate();  // 라인 좌표 업데이트 (색상/개수 유지)
	}
}

