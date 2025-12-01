#pragma once
#include "UEContainer.h"

class ViewerState;
class UWorld;
class UAnimBlendSpaceInstance;
struct ID3D11Device;

// Bootstrap helpers to construct/destroy per-tab viewer state for the BlendSpace editor
class BlendSpaceEditorBootstrap
{
public:
    static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyViewerState(ViewerState*& State);

    // Save BlendSpace to .blendspace file (JSON format)
    static bool SaveBlendSpace(UAnimBlendSpaceInstance* BlendInst, const FString& FilePath, const FString& SkeletalMeshPath);

    // Load BlendSpace from .blendspace file
    static bool LoadBlendSpace(UAnimBlendSpaceInstance* BlendInst, const FString& FilePath, FString& OutSkeletalMeshPath);
};

