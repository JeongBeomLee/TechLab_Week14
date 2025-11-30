#include "pch.h"
#include "SphereActor.h"
#include "SphereComponent.h"

ASphereActor::ASphereActor()
{
    ObjectName = "Sphere Actor";
    
    SphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComponent");
    
    SphereComponent->SetSimulatePhysics(true);

    RootComponent = SphereComponent;
}

ASphereActor::~ASphereActor()
{
}

void ASphereActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
        {
            SphereComponent = Sphere;
            break;
        }
    }
}

void ASphereActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SphereComponent = Cast<USphereComponent>(RootComponent);
    }
}