#pragma once

#include "Actor.h"
#include "ASphereActor.generated.h"

class USphereComponent;

UCLASS(DisplayName="구체 액터", Description="구체 형태의 물리 액터입니다")
class ASphereActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ASphereActor();

protected:
    ~ASphereActor() override;

public:
    USphereComponent* GetSphereComponent() const { return SphereComponent; }

    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    USphereComponent* SphereComponent;
};