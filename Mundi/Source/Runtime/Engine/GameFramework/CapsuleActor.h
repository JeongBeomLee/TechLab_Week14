#pragma once

#include "Actor.h"
#include "ACapsuleActor.generated.h"

class UCapsuleComponent;

UCLASS(DisplayName="캡슐 액터", Description="캡슐 형태의 물리 액터입니다")
class ACapsuleActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ACapsuleActor();

protected:
    ~ACapsuleActor() override;

public:
    UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    UCapsuleComponent* CapsuleComponent;
};