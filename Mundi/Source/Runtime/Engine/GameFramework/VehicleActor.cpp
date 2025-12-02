#include "pch.h"
#include "VehicleActor.h"
#include "VehicleComponent.h"
#include "StaticMeshComponent.h"

AVehicleActor::AVehicleActor()
{
    ObjectName = "자동차 액터";
    VehicleComponent = CreateDefaultSubobject<UVehicleComponent>("VehicleComponent");

    // 루트 교체
    RootComponent = VehicleComponent;

    UStaticMeshComponent* StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
    StaticMeshComponent->SetStaticMesh(GDataDir + "/Model/car.obj");
    StaticMeshComponent->SetupAttachment(RootComponent);
    StaticMeshComponent->BodyInstance.SetSimulatePhysics(false);
    StaticMeshComponent->BodyInstance.SetEnableGravity(false);
    StaticMeshComponent->BodyInstance.SetCollisionEnabled(false);
    StaticMeshComponent->BodyInstance.SetStartAwake(false);
}

void AVehicleActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

AVehicleActor::~AVehicleActor()
{

}

FAABB AVehicleActor::GetBounds() const
{
    // 멤버 변수를 직접 사용하지 않고, 현재의 RootComponent를 확인하도록 수정
    // 기본 컴포넌트가 제거되는 도중에 어떤 로직에 의해 GetBounds() 함수가 호출
    // 이 시점에 AVehicleActor의 멤버변수인 UVehicleComponent는 아직 새로운 컴포넌트
    // ID 751로 업데이트되기 전. 제거된 기본 컴포넌트를 여전히 가리키고 있음. 
    // 유효하지 않은 staticmeshcomponent 포인터의 getworldaabb 함수를 호출 시도.

    UVehicleComponent* CurrentSMC = Cast<UVehicleComponent>(RootComponent);
    if (CurrentSMC)
    {
        return CurrentSMC->GetWorldAABB();
    }

    return FAABB();
}

void AVehicleActor::SetVehicleComponent(UVehicleComponent* InVehicleComponent)
{
    VehicleComponent = InVehicleComponent;
}

void AVehicleActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    for (UActorComponent* Component : OwnedComponents)
    {
        if (UVehicleComponent* VehicleComp = Cast<UVehicleComponent>(Component))
        {
            VehicleComponent = VehicleComp;
            break;
        }
    }
}

void AVehicleActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        VehicleComponent = Cast<UVehicleComponent>(RootComponent);
    }
}
