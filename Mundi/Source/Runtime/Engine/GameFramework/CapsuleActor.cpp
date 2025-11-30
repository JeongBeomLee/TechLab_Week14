#include "pch.h"
#include "CapsuleActor.h"
#include "CapsuleComponent.h"

ACapsuleActor::ACapsuleActor()
{
    ObjectName = "Capsule Actor";
    
    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
    
    CapsuleComponent->SetSimulatePhysics(true);

    RootComponent = CapsuleComponent;
}

ACapsuleActor::~ACapsuleActor()
{
}

void ACapsuleActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
        {
            CapsuleComponent = Capsule;
            break;
        }
    }
}

void ACapsuleActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        CapsuleComponent = Cast<UCapsuleComponent>(RootComponent);
    }
}