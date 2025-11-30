#pragma once

#include "Actor.h"
#include "ABoxActor.generated.h"

class UBoxComponent;

UCLASS(DisplayName="박스 액터", Description="박스 형태의 물리 액터입니다")
class ABoxActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ABoxActor();

protected:
    ~ABoxActor() override;

public:
    UBoxComponent* GetBoxComponent() const { return BoxComponent; }

    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    UBoxComponent* BoxComponent;
};