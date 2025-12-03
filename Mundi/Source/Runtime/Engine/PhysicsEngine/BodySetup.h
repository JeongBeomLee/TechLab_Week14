#pragma once

#include "AggregateGeom.h"
#include "UBodySetup.generated.h"

class UPhysicalMaterial;
struct FBodyInstance;

UCLASS()
class UBodySetup : public UObject
{
	GENERATED_REFLECTION_BODY()
public:
	UBodySetup();

	virtual ~UBodySetup();

    // ====================================================================
	// 본 바인딩 정보 (발제 기준 UBodySetupCore)
	// ====================================================================

	/** 연결된 본 이름 */
	UPROPERTY()
	FName BoneName;

	/** 연결된 본 인덱스 (캐시용) */
	UPROPERTY()
	int32 BoneIndex = -1;

    // ====================================================================
	// 충돌 형상
	// ====================================================================

	/** 단순화된 충돌 표현 (Shape 편집은 별도 UI에서 처리) */
	UPROPERTY()
	FKAggregateGeom AggGeom;

    /** 밀도, 마찰 등과 관련된 정보를 포함하는 물리 재질 */
	UPROPERTY(EditAnywhere, Category="Physics")
	UPhysicalMaterial* PhysicalMaterial;

    void AddShapesToRigidActor_AssumesLocked(
        FBodyInstance* OwningInstance,
        const FVector& Scale3D,
        PxRigidActor* PDestActor,
        UPhysicalMaterial* InPhysicalMaterial);

    /** 테스트용 함수 (박스 추가) */
    void AddBoxTest(const FVector& Extent);

	/** 테스트용 함수 (구 추가) */
	void AddSphereTest(float Radius);

	/** 테스트용 함수 (캡슐 추가) */
	void AddCapsuleTest(float Radius, float Length);

	// ====================================================================
	// 직렬화
	// ====================================================================

	/** 직렬화 - UPROPERTY는 자동, AggGeom은 수동 처리 */
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	/** 서브 객체 복제 */
	virtual void DuplicateSubObjects() override;

	// ====================================================================
	// UI 렌더링 헬퍼
	// ====================================================================

	/**
	 * Shape UI 렌더링 (ImGui)
	 * TODO: PropertyRenderer에 USTRUCT 리플렉션 렌더링 지원 후 제거
	 * @param BodySetup 렌더링할 BodySetup
	 * @param OutSaveRequested 저장이 필요하면 true로 설정됨
	 * @return 값이 변경되었으면 true
	 */
	static bool RenderShapesUI(UBodySetup* BodySetup, bool& OutSaveRequested);
};
