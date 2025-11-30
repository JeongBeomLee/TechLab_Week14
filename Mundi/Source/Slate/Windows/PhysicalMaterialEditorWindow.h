#pragma once
#include "SViewerWindow.h"

class UPhysicalMaterial;

// 에디터 상태를 관리하는 구조체 (탭마다 하나씩 생성됨)
struct PhysicalMaterialEditorState : public ViewerState
{
    UPhysicalMaterial* EditingAsset = nullptr;
    FString CurrentFilePath;
    bool bIsDirty = false;

    virtual ~PhysicalMaterialEditorState() {}
};

/**
 * 물리 재질(Physical Material) 에디터 윈도우
 * 언리얼 엔진의 디테일 패널 스타일을 모방하여 구현
 */
class SPhysicalMaterialEditorWindow : public SViewerWindow
{
public:
    SPhysicalMaterialEditorWindow();
    virtual ~SPhysicalMaterialEditorWindow();

    virtual void OnRender() override;
    virtual void OnSave() override;

    // 파일 열기/포커싱 (외부에서 호출)
    void OpenOrFocusAsset(const FString& FilePath);

protected:
    // 뷰어 상태 관리 (SViewerWindow 가상 함수)
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Physical Material Editor"; }
    virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

    // UI 렌더링 파트
    void RenderToolbar();
    void RenderDetailsPanel();

    // 파일 작업
    void SaveAsset();
    void SaveAssetAs();
    
    // 헬퍼
    PhysicalMaterialEditorState* GetActiveState() const 
    { 
        return static_cast<PhysicalMaterialEditorState*>(ActiveState); 
    }
};