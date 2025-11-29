#pragma once

#include "PhysXSupport.h"

class FPhysScene
{
public:
    FPhysScene(UWorld* InOwningWorld);
    
    ~FPhysScene();

    // ==================================================================================
    // Physics Interface
    // ==================================================================================

    /** 프레임을 시작한다 (시뮬레이션 시작). */
    void StartFrame();

    /** 프레임을 종료한다 (시뮬레이션 완료 후 동기화). */
    void EndFrame(ULineComponent* InLineComponent);
    
    void SetOwningWorld(UWorld* InOwningWorld) { OwningWorld = InOwningWorld; }
    
    UWorld* GetOwningWorld() { return OwningWorld; }

    const UWorld* GetOwningWorld() const { return OwningWorld; }

    // ==================================================================================
    // PhysScene Interface
    // ==================================================================================
    
    physx::PxScene* GetPxScene() const { return PhysXScene; }
    
    // ==================================================================================

    /** PhysX Scene 초기화 */
    void InitPhysScene();

    /** PhysX Scene 종료 */
    void TermPhysScene();

    /** 시뮬레이션 종료를 대기한다. (BodyInstance::TermBody에서 호출) */
    void WaitPhysScene();

private:
    /** 실제 시뮬레이션 로직을 수행한다 (StartFrame 내부에서 호출). */
    void TickPhysScene(float DeltaTime);

    /** 시뮬레이션 결과를 처리하고 동기화한다. */
    void ProcessPhysScene();

    /** 컴포넌트의 트랜스폼에 시뮬레이션 결과를 동기화 */
    void SyncComponentsToBodies();

    /** PhysX Scene */
    PxScene* PhysXScene;

    /** PhysX Scene을 소유하고 있는 월드 */
    UWorld* OwningWorld;

    /** PhysX Scene 시뮬레이션 실행 여부 (실행 시점과 동기화 시점 사이) */
    bool bPhysXSceneExecuting;
};
