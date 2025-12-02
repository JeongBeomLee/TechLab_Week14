#pragma once

#include "Actor.h"
#include "Enums.h"
#include "AVehicleActor.generated.h"

class UVehicleComponent;

UCLASS(DisplayName = "자동차", Description = "자동차 액터입니다")
class AVehicleActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AVehicleActor();

	virtual void Tick(float DeltaTime) override;
protected:
	~AVehicleActor() override;

public:
	virtual FAABB GetBounds() const override;
	UVehicleComponent* GetVehicleComponent() const { return VehicleComponent; }
	void SetVehicleComponent(UVehicleComponent* InVehicleComponent);

	// 복제/직렬화
	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UVehicleComponent* VehicleComponent;
};

