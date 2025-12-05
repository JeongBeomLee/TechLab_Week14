#pragma once

#include "AggregateGeom.h"
#include "ECollisionComplexity.h"
#include "UBodySetup.generated.h"

class UPhysicalMaterial;
class UStaticMesh;
struct FBodyInstance;

UCLASS()
class UBodySetup : public UObject
{
	GENERATED_REFLECTION_BODY()
public:
	UBodySetup();

	virtual ~UBodySetup();

	/** 이 BodySetup을 소유하는 StaticMesh (CollisionComplexity 변경 시 필요) */
	UStaticMesh* OwningMesh = nullptr;

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

	/** 충돌 복잡도 설정 (UseSimple: Convex, UseComplexAsSimple: TriangleMesh) */
	UPROPERTY(EditAnywhere, Category="Collision")
	ECollisionComplexity CollisionComplexity = ECollisionComplexity::UseSimple;

    /** 밀도, 마찰 등과 관련된 정보를 포함하는 물리 재질 */
	UPROPERTY(EditAnywhere, Category="Physics")
	UPhysicalMaterial* PhysicalMaterial;

	/** 물리 재질 경로 (에셋 저장용) */
	FString PhysMaterialPath;

	/** PhysicalMaterial 별명 (Week13 호환) */
	UPhysicalMaterial* PhysMaterial = nullptr;

	// ====================================================================
	// Physics Properties (Week13 호환)
	// ====================================================================

	/** 질량 (kg) */
	UPROPERTY(EditAnywhere, Category="Physics")
	float MassInKg = 1.0f;

	/** 선형 감쇠 */
	UPROPERTY(EditAnywhere, Category="Physics")
	float LinearDamping = 0.01f;

	/** 각도 감쇠 */
	UPROPERTY(EditAnywhere, Category="Physics")
	float AngularDamping = 0.05f;

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

	/** 속성 변경 시 호출 (CollisionComplexity 변경 감지) */
	virtual void OnPropertyChanged(const FProperty& Prop) override;

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
