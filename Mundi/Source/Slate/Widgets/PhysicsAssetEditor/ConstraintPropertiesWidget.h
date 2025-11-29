#pragma once

#include "Source/Slate/Widgets/Widget.h"

struct PhysicsAssetEditorState;
struct FConstraintSetup;

/**
 * UConstraintPropertiesWidget
 *
 * Physics Asset Editor의 우측 패널 (제약 조건 선택 시)
 * 제약 조건 타입 및 각도 제한 편집 UI
 *
 * UWidget 기반 인스턴스로 에디터 상태와 연결
 */
class UConstraintPropertiesWidget : public UWidget
{
public:
	DECLARE_CLASS(UConstraintPropertiesWidget, UWidget)

	UConstraintPropertiesWidget();
	virtual ~UConstraintPropertiesWidget() = default;

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
	bool RenderLimitProperties(FConstraintSetup& Constraint);
};
