#include "pch.h"
#include "PhysScene.h"

FPhysScene::FPhysScene(UWorld* InOwningWorld)
    : OwningWorld(InOwningWorld)
{
}

FPhysScene::~FPhysScene()
{
    TermPhysScene();
}

void FPhysScene::StartFrame()
{
    if (OwningWorld)
    {
        float DeltaTime = OwningWorld->GetDeltaTime(EDeltaTime::Game);
        TickPhysScene(DeltaTime);
    }
}

void FPhysScene::EndFrame(ULineComponent* InLineComponent)
{
    WaitPhysScene();
    ProcessPhysScene();

    if (InLineComponent)
    {
        // @todo 디버그 라인 렌더링
    }
}

void FPhysScene::InitPhysScene()
{
    if (PhysXScene) { return; }

    if (!GPhysXSDK)
    {
        UE_LOG("[PhysX Error] PhysX SDK가 초기화되지 않았습니다.");
        return;
    }

    PxSceneDesc SceneDesc(GPhysXSDK->getTolerancesScale());

    if (!GPhysXDispatcher)
    {
        UE_LOG("[PhysX Error] PhysX Dispatcher가 초기화되지 않았습니다.");
    }
    SceneDesc.cpuDispatcher = GPhysXDispatcher;
    SceneDesc.filterShader = PxDefaultSimulationFilterShader;

    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    SceneDesc.flags |= PxSceneFlag::eENABLE_STABILIZATION;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;

    SceneDesc.gravity = PxVec3(0, -9.81, 0);

    PhysXScene = GPhysXSDK->createScene(SceneDesc);

    if (PhysXScene)
    {
        if (PxPvdSceneClient* PVDClient = PhysXScene->getScenePvdClient())
        {
            PVDClient->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
                                        PxPvdSceneFlag::eTRANSMIT_CONTACTS    |
                                        PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES);
        }
        UE_LOG("[PhysX] PhysX Scene 생성에 성공했습니다.");
    }
    else
    {
        UE_LOG("[PhysX Error] PhysX Scene 생성에 실패했습니다.");
    }
}

void FPhysScene::TermPhysScene()
{
    if (PhysXScene)
    {
        WaitPhysScene();

        PhysXScene->release();
        PhysXScene = nullptr;
    }
}

void FPhysScene::TickPhysScene(float DeltaTime)
{
    if (!PhysXScene)          { return; }

    if (bPhysXSceneExecuting) { return; }

    if (DeltaTime <= 0.0f)    { return; }

    PhysXScene->simulate(DeltaTime);
    bPhysXSceneExecuting = true;
}

void FPhysScene::WaitPhysScene()
{
    if (PhysXScene) { return; }

    if (bPhysXSceneExecuting)
    {
        PhysXScene->fetchResults(true);
        bPhysXSceneExecuting = false;
    }
}

void FPhysScene::ProcessPhysScene()
{
    if (!PhysXScene) { return; }

    // @todo 결과 동기화
}
