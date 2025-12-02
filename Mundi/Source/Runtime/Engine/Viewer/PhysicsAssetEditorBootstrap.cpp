#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "ViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "PhysicsAsset.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshActor.h"
#include "Grid/GridActor.h"
#include "JsonSerializer.h"
#include "EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "ResourceManager.h"

ViewerState* PhysicsAssetEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld,
	ID3D11Device* InDevice, UEditorAssetPreviewContext* Context)
{
	if (!InDevice) return nullptr;

	// PhysicsAssetEditorState 생성
	PhysicsAssetEditorState* State = new PhysicsAssetEditorState();
	State->Name = Name ? Name : "Physics Asset Editor";

	// Preview world 생성
	State->World = NewObject<UWorld>();
	State->World->SetWorldType(EWorldType::PreviewMinimal);
	State->World->Initialize();
	State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

	// Viewport 생성
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0.f, 0.f, 1.f, 1.f, InDevice);
	State->Viewport->SetUseRenderTarget(true);

	// ViewportClient 생성
	FViewportClient* Client = new FViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);

	// 카메라 설정
	Client->GetCamera()->SetActorLocation(FVector(5.f, 5.f, 3.f));
	Client->GetCamera()->SetRotationFromEulerAngles(FVector(0.f, 20.f, -135.f));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());

	// 프리뷰 액터 생성 (SkeletalMesh를 담을 액터)
	if (State->World)
	{
		ASkeletalMeshActor* PreviewActor = State->World->SpawnActor<ASkeletalMeshActor>();
		if (PreviewActor)
		{
			State->PreviewActor = PreviewActor;

			// Context에 AssetPath가 있으면 파일에서 로드, 없으면 기본 생성
			UPhysicsAsset* PhysAsset = nullptr;
			if (Context && !Context->AssetPath.empty())
			{
				// 파일에서 PhysicsAsset 로드
				PhysAsset = LoadPhysicsAsset(Context->AssetPath);
				if (PhysAsset)
				{
					State->CurrentFilePath = Context->AssetPath;
					State->bIsDirty = false;
					UE_LOG("[PhysicsAssetEditorBootstrap] PhysicsAsset 로드: %s", Context->AssetPath.c_str());
				}
			}

			// 로드 실패하거나 AssetPath가 없으면 기본 생성
			if (!PhysAsset)
			{
				PhysAsset = CreateDefaultPhysicsAsset();
				UE_LOG("[PhysicsAssetEditorBootstrap] 기본 PhysicsAsset 생성");
			}

			State->EditingPhysicsAsset = PhysAsset;

			// 바디 Shape 시각화용 LineComponent 생성
			ULineComponent* BodyLineComp = NewObject<ULineComponent>();
			BodyLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(BodyLineComp);
			BodyLineComp->RegisterComponent(State->World);
			BodyLineComp->SetLineVisible(true);
			State->BodyShapeLineComponent = BodyLineComp;

			// Constraint 시각화용 LineComponent 생성
			ULineComponent* ConstraintLineComp = NewObject<ULineComponent>();
			ConstraintLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(ConstraintLineComp);
			ConstraintLineComp->RegisterComponent(State->World);
			ConstraintLineComp->SetLineVisible(true);
			State->ConstraintLineComponent = ConstraintLineComp;
		}
	}

	return State;
}

void PhysicsAssetEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
	if (!State) return;

	PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(State);

	// Viewport/Client 정리
	if (PhysState->Viewport) 
	{ 
		delete PhysState->Viewport; 
		PhysState->Viewport = nullptr; 
	}

	if (PhysState->Client) 
	{ 
		delete PhysState->Client; 
		PhysState->Client = nullptr; 
	}

	// World 정리 (PreviewActor와 LineComponent들은 World가 소유하므로 자동 정리)
	if (PhysState->World) 
	{ 
		ObjectFactory::DeleteObject(PhysState->World); 
		PhysState->World = nullptr; 
	}

	delete PhysState;
	State = nullptr;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::CreateDefaultPhysicsAsset()
{
	UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>();
	// 기본적으로 빈 PhysicsAsset 생성
	return PhysAsset;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath)
{
	if (!Asset || FilePath.empty()) return false;

	Asset->Save(FilePath);
	return true;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath)
{
	if (FilePath.empty()) return nullptr;

	UPhysicsAsset* PhysAsset = UResourceManager::GetInstance().Load<UPhysicsAsset>(FilePath.c_str());
	return PhysAsset;
}
