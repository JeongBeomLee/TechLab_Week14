#pragma once
#include <PxPhysicsAPI.h>

using namespace physx;
class FPhysicsSystem;
class FPhysXSimEventCallback;
class FBodyInstance;
class UVehicleComponent;

class FPhysicsScene
{
    using PhysicsCommand = std::function<void()>;
public:
    FPhysicsScene();
    ~FPhysicsScene();

    // 초기화: 시스템에서 Physics랑 Dispatcher를 빌려옴
    void Initialize(FPhysicsSystem* System);

    // 명령 추가 (메인 스레드에서 물리 관련 작업할 때 여기에 추가)
    void EnqueueCommand(PhysicsCommand Command);
    
    // Tick 시작 시 쌓인 명령들 수행
    void ProcessCommandQueue();
    
    // 명령 수행 후 시뮬레이션 진행
    void Simulation(float DeltaTime);

    void FetchAndSync();
    
    // 정리
    void Shutdown();

    // 액터 관리 (이제 Scene이 액터를 가짐)
    void AddActor(FBodyInstance* Body);
    void RemoveActor(FBodyInstance* Body);

    // 차량 관리
    void AddVehicle(UVehicleComponent* Vehicle);
    void RemoveVehicle(UVehicleComponent* Vehicle);

    PxScene* GetPxScene() const { return mScene; }
    
private:
    PxScene* mScene = nullptr;
    FPhysXSimEventCallback* mSimulationEventCallback = nullptr;
    TArray<PhysicsCommand> CommandQueue;

    TArray<UVehicleComponent*> Vehicles;

    const float FixedDeltaTime = 1.0f / 60.0f;
    float LeftoverTime = 0.0f;
    bool bIsSimulated = false;  // 이번 Tick에 Simulate 을 수행했는지

// Stat Getter
public:
    uint32 GetTotalActorCount() const;
    uint32 GetActiveActorCount() const;
    uint32 GetSleepingActorCount() const;
};