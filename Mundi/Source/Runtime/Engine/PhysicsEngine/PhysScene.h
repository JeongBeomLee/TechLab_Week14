#pragma once

#include <mutex>

#include "PhysXSupport.h"

class FPhysXSimEventCallback;

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

    void AddPendingCollisionNotify(const FCollisionNotifyInfo& NotifyInfo);

    void AddPendingCollisionNotify(FCollisionNotifyInfo&& NotifyInfo);

    // ==================================================================================
    // PhysScene Interface
    // ==================================================================================
    
    physx::PxScene* GetPxScene() const { return PhysXScene; }

    /** @note 액터의 추가는 시뮬레이션 시작 직전에 일괄적으로 이루어진다. */
    void DeferAddActor(PxActor* InActor);

    /** 대기중인 액터 추가를 일괄적으로 처리한다. */
    void FlushDeferredAdds();

    /** @note 액터의 제거는 시뮬레이션 종료 직후에 일괄적으로 이루어진다. */
    void DeferReleaseActor(PxActor* InActor);

    /** 대기중인 액터 해제를 일괄적으로 처리한다. */
    void FlushDeferredReleases();
    
    // ==================================================================================

    /** PhysX Scene 초기화 */
    void InitPhysScene();

    /** PhysX Scene 종료 */
    void TermPhysScene();

    /** 시뮬레이션 종료를 대기한다. (BodyInstance::TermBody에서 호출) */
    void WaitPhysScene();

    /** 실제 시뮬레이션 로직을 수행한다 (에디터에서 직접 호출 가능). */
    void TickPhysScene(float DeltaTime);

    /** 시뮬레이션 결과를 처리하고 동기화한다. */
    void ProcessPhysScene();

private:

    /** 컴포넌트의 트랜스폼에 시뮬레이션 결과를 동기화 */
    void SyncComponentsToBodies();

    /** 큐에 쌓인 충돌 이벤트를 메인 스레드에서 처리 */
    void DispatchPhysNotifications_AssumesLocked();

    /** PhysX Scene */
    PxScene* PhysXScene;

    /** PhysX Scene을 소유하고 있는 월드 */
    UWorld* OwningWorld;

    /** PhysX 이벤트 콜백 */
    FPhysXSimEventCallback* SimEventCallback;
    
    /** 삽입 대기 중인 PhysX 액터 목록 */
    TArray<PxActor*> DeferredAddQueue;
    
    /** 시뮬레이션 중 액터 추가 큐 접근용 뮤텍스 */
    std::mutex DeferredAddMutex;

    /** 해제 대기 중인 PhysX 액터 목록 */
    TArray<PxActor*> DeferredReleaseQueue;
    
    /** 시뮬레이션 중 액터 해제 큐 접근용 뮤텍스 */
    std::mutex DeferredReleaseMutex;

    /** 시뮬레이션이 종료되고 처리될 충돌 정보 큐 */
    TArray<FCollisionNotifyInfo> PendingCollisionNotifies;

    /** 시뮬레이션 중 충돌 정보 큐 접근용 뮤텍스 */
    std::mutex NotifyMutex;

    /** PhysX Scene 시뮬레이션 실행 여부 (실행 시점과 동기화 시점 사이) */
    bool bPhysXSceneExecuting;
};
