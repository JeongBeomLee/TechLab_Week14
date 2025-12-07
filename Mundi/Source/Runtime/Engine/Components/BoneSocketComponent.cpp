#include "pch.h"
#include "BoneSocketComponent.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "BodyInstance.h"
#include "EPhysicsMode.h"

#include <PxRigidDynamic.h>
using namespace physx;

UBoneSocketComponent::UBoneSocketComponent()
{
    // 기본 오프셋 초기화 (단위 변환)
    SocketOffset = FTransform();
}

void UBoneSocketComponent::BeginPlay()
{
    Super::BeginPlay();

    // TargetMesh가 설정되지 않았으면 부모에서 자동으로 찾기
    if (!TargetMesh)
    {
        FindAndSetTargetFromParent();
    }

    // BoneName이 설정되어 있고 BoneIndex가 -1이면 이름으로 인덱스 찾기
    if (TargetMesh && BoneIndex == -1 && !BoneName.empty())
    {
        if (USkeletalMesh* SkelMesh = TargetMesh->GetSkeletalMesh())
        {
            if (const FSkeleton* Skeleton = SkelMesh->GetSkeleton())
            {
                auto It = Skeleton->BoneNameToIndex.find(BoneName);
                if (It != Skeleton->BoneNameToIndex.end())
                {
                    BoneIndex = It->second;
                }
            }
        }
    }

    // 오프셋 프로퍼티에서 SocketOffset 계산
    UpdateSocketOffsetFromProperties();

    // 초기 동기화
    SyncWithBone();
}

void UBoneSocketComponent::SetTarget(USkeletalMeshComponent* InTargetMesh, int32 InBoneIndex)
{
    TargetMesh = InTargetMesh;
    BoneIndex = InBoneIndex;

    // 본 이름도 동기화
    if (TargetMesh && BoneIndex >= 0)
    {
        if (USkeletalMesh* SkelMesh = TargetMesh->GetSkeletalMesh())
        {
            if (const FSkeleton* Skeleton = SkelMesh->GetSkeleton())
            {
                if (BoneIndex < static_cast<int32>(Skeleton->Bones.Num()))
                {
                    BoneName = Skeleton->Bones[BoneIndex].Name;
                }
            }
        }
    }

    // 즉시 동기화
    SyncWithBone();
}

bool UBoneSocketComponent::SetTargetByName(USkeletalMeshComponent* InTargetMesh, const FString& InBoneName)
{
    if (!InTargetMesh)
    {
        return false;
    }

    USkeletalMesh* SkelMesh = InTargetMesh->GetSkeletalMesh();
    if (!SkelMesh)
    {
        return false;
    }

    const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
    if (!Skeleton)
    {
        return false;
    }

    // 본 이름으로 인덱스 찾기
    auto It = Skeleton->BoneNameToIndex.find(InBoneName);
    if (It == Skeleton->BoneNameToIndex.end())
    {
        return false;
    }

    TargetMesh = InTargetMesh;
    BoneIndex = It->second;
    BoneName = InBoneName;

    // 즉시 동기화
    SyncWithBone();
    return true;
}

USkeletalMeshComponent* UBoneSocketComponent::FindAndSetTargetFromParent()
{
    // 부모 컴포넌트 체인을 탐색하며 SkeletalMeshComponent 찾기
    USceneComponent* Parent = GetAttachParent();
    while (Parent)
    {
        // 현재 부모가 SkeletalMeshComponent인지 확인
        USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Parent);
        if (SkelMeshComp)
        {
            TargetMesh = SkelMeshComp;
            return SkelMeshComp;
        }

        // 다음 부모로 이동
        Parent = Parent->GetAttachParent();
    }

    return nullptr;
}

bool UBoneSocketComponent::SetBoneIndexAuto(int32 InBoneIndex)
{
    // 부모에서 SkeletalMeshComponent 찾기
    USkeletalMeshComponent* FoundMesh = FindAndSetTargetFromParent();
    if (!FoundMesh)
    {
        return false;
    }

    SetTarget(FoundMesh, InBoneIndex);
    return true;
}

bool UBoneSocketComponent::SetBoneNameAuto(const FString& InBoneName)
{
    // 부모에서 SkeletalMeshComponent 찾기
    USkeletalMeshComponent* FoundMesh = FindAndSetTargetFromParent();
    if (!FoundMesh)
    {
        return false;
    }

    return SetTargetByName(FoundMesh, InBoneName);
}

void UBoneSocketComponent::SetSocketOffset(const FTransform& InOffset)
{
    SocketOffset = InOffset;

    // UPROPERTY들도 동기화
    SocketOffsetLocation = InOffset.Translation;
    SocketOffsetRotationEuler = InOffset.Rotation.ToEulerZYXDeg();
    SocketOffsetScale = InOffset.Scale3D;
}

void UBoneSocketComponent::SetSocketOffsetLocation(const FVector& InLocation)
{
    SocketOffsetLocation = InLocation;
    UpdateSocketOffsetFromProperties();
}

void UBoneSocketComponent::SetSocketOffsetRotation(const FQuat& InRotation)
{
    SocketOffsetRotationEuler = InRotation.ToEulerZYXDeg();
    UpdateSocketOffsetFromProperties();
}

void UBoneSocketComponent::UpdateSocketOffsetFromProperties()
{
    FQuat Rotation = FQuat::MakeFromEulerZYX(SocketOffsetRotationEuler);
    SocketOffset = FTransform(SocketOffsetLocation, Rotation, SocketOffsetScale);
}

void UBoneSocketComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (bAutoSync)
    {
        SyncWithBone();
    }
}

void UBoneSocketComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
    // 부모 트랜스폼이 변경되면 즉시 본과 재동기화
    // SyncWithBone 내부에서 bSyncing 플래그로 재귀 방지
    if (bAutoSync && TargetMesh)
    {
        SyncWithBone();
    }

    // 자식들에게 트랜스폼 변경 전파 (기본 클래스 구현 사용)
    Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
}

void UBoneSocketComponent::SyncWithBone()
{
    if (!TargetMesh || BoneIndex < 0 || bSyncing)
    {
        return;
    }

    bSyncing = true;

    FTransform BoneWorldTransform;

    // 래그돌 모드일 때는 직접 물리 바디에서 트랜스폼을 가져옴 (틱 순서 문제 해결)
    if (TargetMesh->GetPhysicsMode() == EPhysicsMode::Ragdoll)
    {
        FBodyInstance* Body = TargetMesh->GetBodyInstance(BoneIndex);
        if (Body && Body->IsValidBodyInstance())
        {
            BoneWorldTransform = Body->GetUnrealWorldTransform();
            // 스케일은 컴포넌트에서 가져오기 (물리 바디는 스케일 정보가 없을 수 있음)
            BoneWorldTransform.Scale3D = TargetMesh->GetWorldScale();
        }
        else
        {
            // 바디가 없는 본은 기존 방식 사용
            BoneWorldTransform = TargetMesh->GetBoneWorldTransform(BoneIndex);
        }
    }
    else
    {
        // 애니메이션/키네마틱 모드: 기존 방식
        BoneWorldTransform = TargetMesh->GetBoneWorldTransform(BoneIndex);
    }

    // 오프셋 적용: BoneWorld * SocketOffset
    FTransform FinalTransform = BoneWorldTransform.GetWorldTransform(SocketOffset);

    // 이 컴포넌트의 월드 트랜스폼 설정
    SetWorldTransform(FinalTransform, EUpdateTransformFlags::SkipPhysicsUpdate);

    // 부착된 래그돌 또는 자식 래그돌 동기화
    SyncAttachedRagdoll();

    bSyncing = false;
}

FTransform UBoneSocketComponent::GetSocketWorldTransform() const
{
    if (!TargetMesh || BoneIndex < 0)
    {
        return GetWorldTransform();
    }

    FTransform BoneWorldTransform;

    // 래그돌 모드일 때는 직접 물리 바디에서 트랜스폼을 가져옴
    if (TargetMesh->GetPhysicsMode() == EPhysicsMode::Ragdoll)
    {
        FBodyInstance* Body = TargetMesh->GetBodyInstance(BoneIndex);
        if (Body && Body->IsValidBodyInstance())
        {
            BoneWorldTransform = Body->GetUnrealWorldTransform();
            BoneWorldTransform.Scale3D = TargetMesh->GetWorldScale();
        }
        else
        {
            BoneWorldTransform = const_cast<USkeletalMeshComponent*>(TargetMesh)->GetBoneWorldTransform(BoneIndex);
        }
    }
    else
    {
        BoneWorldTransform = const_cast<USkeletalMeshComponent*>(TargetMesh)->GetBoneWorldTransform(BoneIndex);
    }

    return BoneWorldTransform.GetWorldTransform(SocketOffset);
}

// ──────────────────────────────
// 래그돌 부착 (시체 메기 등)
// ──────────────────────────────

void UBoneSocketComponent::AttachRagdoll(USkeletalMeshComponent* RagdollMesh, int32 AttachBoneIndex)
{
    if (!RagdollMesh)
    {
        return;
    }

    // 이전 래그돌이 있으면 해제
    if (AttachedRagdoll)
    {
        DetachRagdoll();
    }

    AttachedRagdoll = RagdollMesh;
    AttachedRagdollBoneIndex = AttachBoneIndex;

    // 래그돌 모드가 아니면 래그돌 모드로 전환
    if (RagdollMesh->GetPhysicsMode() != EPhysicsMode::Ragdoll)
    {
        RagdollMesh->SetPhysicsMode(EPhysicsMode::Ragdoll);
    }

    // 고정할 본의 바디를 키네마틱으로 설정
    FBodyInstance* AttachBody = RagdollMesh->GetBodyInstance(AttachBoneIndex);
    if (AttachBody && AttachBody->IsValidBodyInstance())
    {
        AttachBody->SetKinematic(true); // 키네마틱으로 전환
    }

    // 즉시 동기화
    SyncAttachedRagdoll();
}

void UBoneSocketComponent::DetachRagdoll()
{
    if (!AttachedRagdoll)
    {
        return;
    }

    // 고정했던 본의 바디를 다시 시뮬레이션 모드로
    FBodyInstance* AttachBody = AttachedRagdoll->GetBodyInstance(AttachedRagdollBoneIndex);
    if (AttachBody && AttachBody->IsValidBodyInstance())
    {
        AttachBody->SetKinematic(false); // 다시 시뮬레이션 (다이나믹)
    }

    AttachedRagdoll = nullptr;
    AttachedRagdollBoneIndex = -1;
}

void UBoneSocketComponent::SyncAttachedRagdoll()
{
    FTransform SocketWorld = GetWorldTransform();

    // 명시적으로 AttachRagdoll로 부착된 경우
    if (AttachedRagdoll && AttachedRagdollBoneIndex >= 0)
    {
        FBodyInstance* AttachBody = AttachedRagdoll->GetBodyInstance(AttachedRagdollBoneIndex);
        if (AttachBody && AttachBody->IsValidBodyInstance())
        {
            // 매 프레임 키네마틱으로 강제 설정 후 위치 적용
            AttachBody->SetKinematic(true);
            AttachBody->SetKinematicTarget(SocketWorld); // 부드러운 이동
        }
        return;
    }

    // 자식 컴포넌트 중 래그돌 SkeletalMeshComponent 자동 감지 및 동기화
    for (USceneComponent* Child : GetAttachChildren())
    {
        USkeletalMeshComponent* SkelMeshChild = Cast<USkeletalMeshComponent>(Child);
        if (!SkelMeshChild)
        {
            continue;
        }

        // Ragdoll 또는 Kinematic 모드에서만 동기화
        EPhysicsMode ChildMode = SkelMeshChild->GetPhysicsMode();
        if (ChildMode != EPhysicsMode::Ragdoll && ChildMode != EPhysicsMode::Kinematic)
        {
            continue;
        }

        // 첫 번째 유효한 바디를 찾아서 루트로 사용
        const TArray<FBodyInstance*>& Bodies = SkelMeshChild->GetBodies();
        FBodyInstance* RootBody = nullptr;

        for (FBodyInstance* Body : Bodies)
        {
            if (Body && Body->IsValidBodyInstance())
            {
                RootBody = Body;
                break;
            }
        }

        if (RootBody)
        {
            // 매 프레임 키네마틱으로 강제 설정 후 위치 적용
            RootBody->SetKinematic(true);
            RootBody->SetKinematicTarget(SocketWorld); // 부드러운 이동

            // 다른 바디들에 높은 damping 적용하여 회전 억제
            for (FBodyInstance* Body : Bodies)
            {
                if (Body && Body != RootBody && Body->IsValidBodyInstance() && Body->RigidActor)
                {
                    PxRigidDynamic* DynActor = Body->RigidActor->is<PxRigidDynamic>();
                    if (DynActor)
                    {
                        DynActor->setLinearDamping(5.0f);
                        DynActor->setAngularDamping(5.0f);
                    }
                }
            }
        }
    }
}
