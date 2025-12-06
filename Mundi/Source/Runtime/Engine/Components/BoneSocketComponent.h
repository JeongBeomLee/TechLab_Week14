#pragma once

#include "SceneComponent.h"
#include "UBoneSocketComponent.generated.h"

class USkeletalMeshComponent;

/**
 * @brief 스켈레탈 메시의 특정 본(Bone)에 부착되어 따라가는 컴포넌트
 *
 * 사용법:
 * 1. 이 컴포넌트를 액터에 추가
 * 2. SetTarget()으로 대상 스켈레탈 메시와 본 설정
 * 3. 다른 컴포넌트들을 이 소켓 컴포넌트의 자식으로 부착
 *
 * 매 틱마다 대상 본의 월드 트랜스폼을 따라갑니다.
 */
UCLASS(DisplayName="본 소켓 컴포넌트", Description="스켈레탈 메시의 특정 본에 부착되어 따라가는 컴포넌트입니다")
class UBoneSocketComponent : public USceneComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UBoneSocketComponent();

    // ──────────────────────────────
    // 타겟 설정
    // ──────────────────────────────

    /**
     * @brief 대상 스켈레탈 메시와 본 인덱스 설정
     * @param InTargetMesh 대상 스켈레탈 메시 컴포넌트
     * @param InBoneIndex 부착할 본 인덱스
     */
    UFUNCTION(LuaBind, DisplayName="SetTarget")
    void SetTarget(USkeletalMeshComponent* InTargetMesh, int32 InBoneIndex);

    /**
     * @brief 대상 스켈레탈 메시와 본 이름으로 설정
     * @param InTargetMesh 대상 스켈레탈 메시 컴포넌트
     * @param InBoneName 부착할 본 이름
     * @return 성공 여부 (본 이름이 없으면 false)
     */
    UFUNCTION(LuaBind, DisplayName="SetTargetByName")
    bool SetTargetByName(USkeletalMeshComponent* InTargetMesh, const FString& InBoneName);

    /**
     * @brief 본 인덱스만 설정 (TargetMesh는 자동으로 부모에서 찾음)
     * @param InBoneIndex 부착할 본 인덱스
     * @return 성공 여부 (부모에 SkeletalMeshComponent가 없으면 false)
     */
    UFUNCTION(LuaBind, DisplayName="SetBoneIndex")
    bool SetBoneIndexAuto(int32 InBoneIndex);

    /**
     * @brief 본 이름만 설정 (TargetMesh는 자동으로 부모에서 찾음)
     * @param InBoneName 부착할 본 이름
     * @return 성공 여부 (부모에 SkeletalMeshComponent가 없거나 본 이름이 없으면 false)
     */
    UFUNCTION(LuaBind, DisplayName="SetBoneName")
    bool SetBoneNameAuto(const FString& InBoneName);

    /**
     * @brief 부모 컴포넌트 체인에서 SkeletalMeshComponent를 찾아서 타겟으로 설정
     * @return 찾은 SkeletalMeshComponent (없으면 nullptr)
     */
    UFUNCTION(LuaBind, DisplayName="FindParentSkeletalMesh")
    USkeletalMeshComponent* FindAndSetTargetFromParent();

    // Getters
    USkeletalMeshComponent* GetTargetMesh() const { return TargetMesh; }
    int32 GetBoneIndex() const { return BoneIndex; }

    // ──────────────────────────────
    // 소켓 오프셋
    // ──────────────────────────────

    /**
     * @brief 본 기준 오프셋 설정
     * @param InOffset 본의 로컬 공간 기준 오프셋 트랜스폼
     */
    void SetSocketOffset(const FTransform& InOffset);
    FTransform GetSocketOffset() const { return SocketOffset; }

    /**
     * @brief 위치 오프셋만 설정 (회전/스케일은 유지)
     */
    void SetSocketOffsetLocation(const FVector& InLocation);

    /**
     * @brief 회전 오프셋만 설정 (위치/스케일은 유지)
     */
    void SetSocketOffsetRotation(const FQuat& InRotation);

    // ──────────────────────────────
    // 업데이트
    // ──────────────────────────────

    /**
     * @brief 초기화 시 자동으로 부모에서 타겟 찾기
     */
    void BeginPlay() override;

    /**
     * @brief 매 프레임 본 트랜스폼과 동기화
     */
    void TickComponent(float DeltaTime) override;

protected:
    /**
     * @brief 부모 트랜스폼 변경 시 호출 - 본 소켓은 자체적으로 트랜스폼을 관리하므로 무시
     */
    void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

    /**
     * @brief 수동으로 본 트랜스폼과 동기화 (틱 비활성화 시 사용)
     */
    void SyncWithBone();

    /**
     * @brief 소켓의 현재 월드 트랜스폼 계산 (본 + 오프셋)
     * @return 소켓의 월드 트랜스폼
     */
    FTransform GetSocketWorldTransform() const;

    // ──────────────────────────────
    // 활성화/비활성화
    // ──────────────────────────────

    /**
     * @brief 자동 동기화 활성화/비활성화
     */
    void SetAutoSync(bool bEnable) { bAutoSync = bEnable; }
    bool IsAutoSyncEnabled() const { return bAutoSync; }

public:
    /** 부착할 본 인덱스 */
    UPROPERTY(EditAnywhere, Category="소켓")
    int32 BoneIndex = -1;

    /** 부착할 본 이름 (에디터 편의용) */
    UPROPERTY(EditAnywhere, Category="소켓")
    FString BoneName;

    /** 본 기준 위치 오프셋 */
    UPROPERTY(EditAnywhere, Category="소켓")
    FVector SocketOffsetLocation = FVector(0, 0, 0);

    /** 본 기준 회전 오프셋 (Euler, Degrees) */
    UPROPERTY(EditAnywhere, Category="소켓")
    FVector SocketOffsetRotationEuler = FVector(0, 0, 0);

    /** 본 기준 스케일 오프셋 */
    UPROPERTY(EditAnywhere, Category="소켓")
    FVector SocketOffsetScale = FVector(1, 1, 1);

    /** 자동 동기화 활성화 여부 */
    UPROPERTY(EditAnywhere, Category="소켓")
    bool bAutoSync = true;

private:
    /** 대상 스켈레탈 메시 컴포넌트 (리플렉션 미지원) */
    USkeletalMeshComponent* TargetMesh = nullptr;

    /** 내부용 오프셋 트랜스폼 (위 값들로부터 계산) */
    FTransform SocketOffset;

    /** 오프셋 프로퍼티 변경 시 SocketOffset 재계산 */
    void UpdateSocketOffsetFromProperties();

    /** SyncWithBone 재귀 호출 방지 플래그 */
    bool bSyncing = false;
};
