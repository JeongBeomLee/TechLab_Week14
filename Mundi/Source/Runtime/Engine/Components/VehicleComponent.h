#pragma once
#include "PrimitiveComponent.h"
#include "UVehicleComponent.generated.h"

struct FVehicleData;

UCLASS(DisplayName = "자동차 물리 컴포넌트", Description = "자동차 물리 처리 컴포넌트입니다")
class UVehicleComponent : public UPrimitiveComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UVehicleComponent();

    virtual ~UVehicleComponent();
    void EndPlay() override;
    void DuplicateSubObjects() override;

    virtual bool CanSimulatingPhysics() const  override { return true; }

    // 매 프레임 업데이트 (입력 처리 -> 물리 업데이트 -> 렌더링 동기화)
    // void TickComponent(float DeltaTime) override;

protected:
    void CreatePhysicsState() override;

private:
    PxVehicleDrive4W* CreateVehicle4W(FVehicleData VehicleData, physx::PxPhysics* Physics, PxCooking* cooking, const FTransform& WorldTransform);

    // PhysX 차량 인스턴스
    physx::PxVehicleDrive4W* PhysXVehicle = nullptr;
    UPhysicalMaterial* ChassisMaterial = nullptr;
    UPhysicalMaterial* WheelMaterial = nullptr;
};