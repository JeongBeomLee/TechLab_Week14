#pragma once

#include "Source/Slate/Widgets/Widget.h"

struct PhysicsAssetEditorState;
struct FBodySetup;

/**
 * UBodyPropertiesWidget
 *
 * Physics Asset Editor의 우측 패널 (바디 선택 시)
 * 바디 Shape 및 물리 속성 편집 UI
 *
 * UWidget 기반 인스턴스로 에디터 상태와 연결
 */
class UBodyPropertiesWidget : public UWidget
{
public:
	DECLARE_CLASS(UBodyPropertiesWidget, UWidget)

	UBodyPropertiesWidget();
	virtual ~UBodyPropertiesWidget() = default;

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	// 외부 상태 연결
	void SetEditorState(PhysicsAssetEditorState* InState) { EditorState = InState; }
	PhysicsAssetEditorState* GetEditorState() const { return EditorState; }

	// 속성이 변경되었는지 확인
	bool WasModified() const { return bWasModified; }
	void ClearModifiedFlag() { bWasModified = false; }

private:
	PhysicsAssetEditorState* EditorState = nullptr;
	bool bWasModified = false;

	// 렌더링 헬퍼
	bool RenderShapeProperties(FBodySetup& Body);
	bool RenderPhysicsProperties(FBodySetup& Body);
};
