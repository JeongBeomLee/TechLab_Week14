#include "pch.h"
#include "BoxActor.h"
#include "BoxComponent.h"

ABoxActor::ABoxActor()
{
    ObjectName = "Box Actor";
    
    BoxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");

    BoxComponent->SetSimulatePhysics(false);

    RootComponent = BoxComponent;
}

ABoxActor::~ABoxActor()
{
}

void ABoxActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (UBoxComponent* Box = Cast<UBoxComponent>(Component))
        {
            BoxComponent = Box;
            break;
        }
    }
}

void ABoxActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        BoxComponent = Cast<UBoxComponent>(RootComponent);
    }
}