#include "pch.h"
#include "BlendSpaceEditorBootstrap.h"
#include "FViewport.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"
#include "Source/Runtime/Renderer/BlendSpaceEditorViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Animation/AnimBlendSpaceInstance.h"
#include "JsonSerializer.h"
#include "PathUtils.h"

ViewerState* BlendSpaceEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    ViewerState* State = new ViewerState();
    State->Name = Name ? Name : "BlendSpace2D";

    // Preview world
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    // Viewport
    State->Viewport = new FViewport();
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);
    State->Viewport->SetUseRenderTarget(true);  // Use ImGui::Image method for viewer

    // Client
    auto* Client = new FBlendSpaceEditorViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    State->Client = Client;
    State->Viewport->SetViewportClient(Client);
    State->World->SetEditorCameraActor(Client->GetCamera());

    // Preview actor for skeletal mesh
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;
        State->CurrentMesh = Preview && Preview->GetSkeletalMeshComponent()
            ? Preview->GetSkeletalMeshComponent()->GetSkeletalMesh() : nullptr;
    }

    return State;
}

void BlendSpaceEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;
    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}

bool BlendSpaceEditorBootstrap::SaveBlendSpace(UAnimBlendSpaceInstance* BlendInst, const FString& FilePath, const FString& SkeletalMeshPath)
{
    if (!BlendInst)
    {
        UE_LOG("[BlendSpaceEditorBootstrap] SaveBlendSpace: BlendInst가 nullptr입니다");
        return false;
    }

    if (FilePath.empty())
    {
        UE_LOG("[BlendSpaceEditorBootstrap] SaveBlendSpace: FilePath가 비어있습니다");
        return false;
    }

    // Create JSON object
    JSON JsonData = JSON::Make(JSON::Class::Object);

    // Save metadata
    JsonData["Version"] = 1;
    JsonData["SkeletalMeshPath"] = SkeletalMeshPath;

    // Serialize BlendSpace data
    JSON BlendSpaceJson = JSON::Make(JSON::Class::Object);
    BlendInst->SerializeBlendSpace(false, BlendSpaceJson);
    JsonData["BlendSpace"] = BlendSpaceJson;

    // Save to file
    FWideString WidePath = UTF8ToWide(FilePath);
    if (!FJsonSerializer::SaveJsonToFile(JsonData, WidePath))
    {
        UE_LOG("[BlendSpaceEditorBootstrap] SaveBlendSpace: 파일 저장 실패: %s", FilePath.c_str());
        return false;
    }

    UE_LOG("[BlendSpaceEditorBootstrap] SaveBlendSpace: 저장 성공: %s", FilePath.c_str());
    return true;
}

bool BlendSpaceEditorBootstrap::LoadBlendSpace(UAnimBlendSpaceInstance* BlendInst, const FString& FilePath, FString& OutSkeletalMeshPath)
{
    if (!BlendInst)
    {
        UE_LOG("[BlendSpaceEditorBootstrap] LoadBlendSpace: BlendInst가 nullptr입니다");
        return false;
    }

    if (FilePath.empty())
    {
        UE_LOG("[BlendSpaceEditorBootstrap] LoadBlendSpace: FilePath가 비어있습니다");
        return false;
    }

    // Normalize path
    FString NormalizedPath = ResolveAssetRelativePath(NormalizePath(FilePath), "");

    // Load JSON from file
    FWideString WidePath = UTF8ToWide(NormalizedPath);
    JSON JsonData;
    if (!FJsonSerializer::LoadJsonFromFile(JsonData, WidePath))
    {
        UE_LOG("[BlendSpaceEditorBootstrap] LoadBlendSpace: 파일 로드 실패: %s", NormalizedPath.c_str());
        return false;
    }

    // Read version (for future compatibility)
    int32 Version = 1;
    FJsonSerializer::ReadInt32(JsonData, "Version", Version, 1, false);

    // Read skeletal mesh path
    FJsonSerializer::ReadString(JsonData, "SkeletalMeshPath", OutSkeletalMeshPath, "", false);

    // Deserialize BlendSpace data
    JSON BlendSpaceJson;
    if (FJsonSerializer::ReadObject(JsonData, "BlendSpace", BlendSpaceJson, nullptr, false))
    {
        BlendInst->SerializeBlendSpace(true, BlendSpaceJson);
    }

    UE_LOG("[BlendSpaceEditorBootstrap] LoadBlendSpace: 로드 성공: %s", NormalizedPath.c_str());
    return true;
}

