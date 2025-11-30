#include "pch.h"
#include "PhysicalMaterialEditorWindow.h"
#include "PhysicalMaterial.h"
#include "ResourceManager.h"
#include "SlateManager.h"
#include "ImGui/imgui.h"
#include "Source/Editor/PlatformProcess.h" // 파일 다이얼로그용
#include "Source/Slate/Widgets/ModalDialog.h" // 저장 확인창용
#include <filesystem>

#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Widgets/PropertyRenderer.h"

SPhysicalMaterialEditorWindow::SPhysicalMaterialEditorWindow()
{
    WindowTitle = "Physical Material Editor";
}

SPhysicalMaterialEditorWindow::~SPhysicalMaterialEditorWindow()
{
    // 열려있는 탭(State) 정리
    for (ViewerState* State : Tabs)
    {
        DestroyViewerState(State);
    }
    Tabs.Empty();
}

ViewerState* SPhysicalMaterialEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    PhysicalMaterialEditorState* NewState = new PhysicalMaterialEditorState();
    NewState->Name = Name;
    
    // Context에 경로가 있다면 로드
    if (Context && !Context->AssetPath.empty())
    {
        NewState->CurrentFilePath = Context->AssetPath;
        NewState->EditingAsset = UResourceManager::GetInstance().Load<UPhysicalMaterial>(Context->AssetPath);
    }
    else
    {
        // 새 에셋 생성
        NewState->EditingAsset = NewObject<UPhysicalMaterial>();
        NewState->CurrentFilePath = ""; // 아직 저장 안됨
    }

    return NewState;
}

void SPhysicalMaterialEditorWindow::DestroyViewerState(ViewerState*& State)
{
    if (State)
    {
        delete State;
        State = nullptr;
    }
}

void SPhysicalMaterialEditorWindow::OpenOrFocusAsset(const FString& FilePath)
{
    // 이미 열려있는지 확인
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        PhysicalMaterialEditorState* State = static_cast<PhysicalMaterialEditorState*>(Tabs[i]);
        if (State->CurrentFilePath == FilePath)
        {
            ActiveTabIndex = i;
            ActiveState = State;
            return;
        }
    }

    // 없으면 새 탭 생성
    UEditorAssetPreviewContext Context;
    Context.AssetPath = FilePath;
    
    std::filesystem::path fsPath(UTF8ToWide(FilePath));
    FString FileName = WideToUTF8(fsPath.filename().wstring());

    ViewerState* NewState = CreateViewerState(FileName.c_str(), &Context);
    if (NewState)
    {
        Tabs.Add(NewState);
        ActiveTabIndex = Tabs.Num() - 1;
        ActiveState = NewState;
    }
}

void SPhysicalMaterialEditorWindow::OnRender()
{
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    // 윈도우 설정
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver); // 디테일 패널이라 적당한 폭

    char UniqueTitle[256];
    sprintf_s(UniqueTitle, "%s###PhyMatEd_%p", WindowTitle.c_str(), this);

    if (ImGui::Begin(UniqueTitle, &bIsOpen, ImGuiWindowFlags_MenuBar))
    {
        // 1. 탭 바 & 툴바 렌더링
        RenderTabsAndToolbar(EViewerType::PhysicalMaterial);

        // 2. 디테일 패널 렌더링 (메인 영역)
        if (GetActiveState() && GetActiveState()->EditingAsset)
        {
            RenderDetailsPanel();
        }
        else
        {
            ImGui::TextDisabled("No asset loaded.");
        }
    }
    ImGui::End();
}

void SPhysicalMaterialEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
    // 탭 바
    if (ImGui::BeginTabBar("PhysicalMaterialTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
    {
        for (int i = 0; i < Tabs.Num(); ++i)
        {
            PhysicalMaterialEditorState* State = static_cast<PhysicalMaterialEditorState*>(Tabs[i]);
            bool open = true;
            
            FString DisplayName = State->Name.ToString();
            if (State->bIsDirty) DisplayName += "*";

            if (ImGui::BeginTabItem((DisplayName + "###Tab" + std::to_string(i)).c_str(), &open))
            {
                if (ActiveState != State)
                {
                    ActiveTabIndex = i;
                    ActiveState = State;
                }
                ImGui::EndTabItem();
            }

            if (!open)
            {
                // 닫기 전 저장 확인 로직이 필요하다면 여기에 추가
                CloseTab(i);
                ImGui::EndTabBar();
                return;
            }
        }
        ImGui::EndTabBar();
    }

    // 툴바 (저장 버튼 등)
    RenderToolbar();
}

void SPhysicalMaterialEditorWindow::RenderToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    // 아이콘이 있다면 ImageButton을 사용하겠지만, 텍스트로 대체합니다.
    // 언리얼 스타일: [플로피 디스크 아이콘] 저장
    if (ImGui::Button("Save"))
    {
        SaveAsset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        SaveAssetAs();
    }

    // 파일 경로 표시 (디버그/확인용)
    if (PhysicalMaterialEditorState* State = GetActiveState())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("|  %s", State->CurrentFilePath.empty() ? "Untitled" : State->CurrentFilePath.c_str());
    }

    ImGui::Separator();
    ImGui::PopStyleVar();
}

void SPhysicalMaterialEditorWindow::RenderDetailsPanel()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    // 언리얼 스타일의 "검색" 바 구현
    // PropertyRenderer에는 현재 필터링 기능이 내장되어 있지 않으므로 UI만 유사하게 배치합니다.
    // (필요하다면 PropertyRenderer에 TextFilter 기능을 추가해야 함)
    static char SearchBuf[128] = "";
    
    ImGui::Spacing();
    
    // 검색창 스타일링 (약간 어둡게)
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f); // 둥근 검색창
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    
    // 돋보기 아이콘 대신 텍스트로 힌트 표시
    ImGui::InputTextWithHint("##Search", "검색", SearchBuf, IM_ARRAYSIZE(SearchBuf));
    ImGui::PopStyleVar();
    
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::BeginChild("DetailsScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (UPropertyRenderer::RenderAllPropertiesWithInheritance(State->EditingAsset))
    {
        State->bIsDirty = true;
        
        State->EditingAsset->UpdatePhysicsMaterial();
    }

    ImGui::EndChild();
}

void SPhysicalMaterialEditorWindow::OnSave()
{
    SaveAsset();
}

void SPhysicalMaterialEditorWindow::SaveAsset()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    // 경로가 없으면 '다른 이름으로 저장'
    if (State->CurrentFilePath.empty())
    {
        SaveAssetAs();
        return;
    }

    // JSON 직렬화하여 저장
    JSON JsonHandle;
    State->EditingAsset->Serialize(false, JsonHandle); // false = Save
    
    FWideString WidePath(State->CurrentFilePath.begin(), State->CurrentFilePath.end());
    if (FJsonSerializer::SaveJsonToFile(JsonHandle, WidePath))
    {
        State->bIsDirty = false;
        UE_LOG("[PhysicalMaterialEditor] Saved: %s", State->CurrentFilePath.c_str());
    }
    else
    {
        FModalDialog::Get().Show("Error", "Failed to save file.", EModalType::OK);
    }
}

void SPhysicalMaterialEditorWindow::SaveAssetAs()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    // 파일 저장 다이얼로그 호출
    std::wstring widePath = FPlatformProcess::OpenSaveFileDialog(
        UTF8ToWide(GDataDir),
        L"physicalmaterial", // 확장자
        L"Physical Material Files"
    );

    if (widePath.empty()) return;

    State->CurrentFilePath = WideToUTF8(widePath);
    
    // 파일명으로 탭 이름 업데이트
    std::filesystem::path fsPath(widePath);
    State->Name = WideToUTF8(fsPath.filename().wstring()).c_str();

    SaveAsset();
}