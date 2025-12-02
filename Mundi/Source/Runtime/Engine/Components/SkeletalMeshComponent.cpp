#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "PlatformTime.h"
#include "AnimSequence.h"
#include "FbxLoader.h"
#include "AnimNodeBase.h"
#include "AnimSingleNodeInstance.h"
#include "AnimStateMachineInstance.h"
#include "AnimBlendSpaceInstance.h"
#include "PhysicsAsset.h"
#include "BodyInstance.h"
#include "ConstraintInstance.h"
#include "PhysScene.h"
#include "FBodySetup.h"
#include "PhysXPublic.h"
#include "AggregateGeom.h"
#include "World.h"
#include "Renderer.h"
#include "PhysicsDebugUtils.h"
#include "ECollisionChannel.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 기본적으로 Kinematic 모드 (애니메이션 + 물리 충돌)
    PhysicsMode = EPhysicsMode::Kinematic;
    bSimulatePhysics = true;

    // Keep constructor lightweight for editor/viewer usage.
    // Load a simple default test mesh if available; viewer UI can override.
    SetSkeletalMesh(GDataDir + "/DancingRacer.fbx");

    // 기본 PhysicsAsset 설정
    PhysicsAsset = UResourceManager::GetInstance().Load<UPhysicsAsset>("Data/Physics/Dancing.physicsasset");
}


void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh) { return; }

    switch (PhysicsMode)
    {
    case EPhysicsMode::Animation:
        // 애니메이션만 재생 (물리 없음)
        if (bUseAnimation && AnimInstance && SkeletalMesh->GetSkeleton())
        {
            AnimInstance->NativeUpdateAnimation(DeltaTime);

            FPoseContext OutputPose;
            OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
            AnimInstance->EvaluateAnimation(OutputPose);

            BaseAnimationPose = OutputPose.LocalSpacePose;
            CurrentLocalSpacePose = OutputPose.LocalSpacePose;
            ForceRecomputePose();
        }
        break;

    case EPhysicsMode::Kinematic:
        // 애니메이션이 물리 바디를 제어
        if (bUseAnimation && AnimInstance && SkeletalMesh->GetSkeleton())
        {
            AnimInstance->NativeUpdateAnimation(DeltaTime);

            FPoseContext OutputPose;
            OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
            AnimInstance->EvaluateAnimation(OutputPose);

            BaseAnimationPose = OutputPose.LocalSpacePose;
            CurrentLocalSpacePose = OutputPose.LocalSpacePose;
            ForceRecomputePose();
        }

        // 애니메이션 결과를 물리 바디에 동기화
        if (bRagdollInitialized)
        {
            SyncPhysicsFromAnimation();
        }
        break;

    case EPhysicsMode::Ragdoll:
        // 물리가 본을 제어
        if (bRagdollInitialized)
        {
            SyncBonesFromPhysics();
        }
        break;
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix); 
        }
        RefPose = CurrentLocalSpacePose;
        ForceRecomputePose();

        // Rebind anim instance to new skeleton
        if (AnimInstance)
        {
            AnimInstance->InitializeAnimation(this);
        }
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
    }
}

void USkeletalMeshComponent::DuplicateSubObjects()
{
    USkinnedMeshComponent::DuplicateSubObjects();

    Bodies.Empty();
    Constraints.Empty();

    bRagdollInitialized = false;
    Aggregate = nullptr;
    PhysScene = nullptr;
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->InitializeAnimation(this);
    }
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* Asset, bool bLooping, float InPlayRate)
{
    UAnimSingleNodeInstance* Single = nullptr;
    if (!AnimInstance)
    {
        Single = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(Single);
    }
    else
    {
        Single = Cast<UAnimSingleNodeInstance>(AnimInstance);
        if (!Single)
        {
            // Replace with a single-node instance for simple playback
            // 기존 AnimInstance를 먼저 삭제
            ObjectFactory::DeleteObject(AnimInstance);
            AnimInstance = nullptr;

            Single = NewObject<UAnimSingleNodeInstance>();
            SetAnimInstance(Single);
        }
    }

    if (Single)
    {
        Single->SetAnimationAsset(Asset, bLooping);
        Single->SetPlayRate(InPlayRate);
        Single->Play(true);
    }
}

// ==== Lua-friendly State Machine helper: switch this component to a state machine anim instance ====
void USkeletalMeshComponent::UseStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimStateMachineInstance already exists\n");
    }
}

UAnimStateMachineInstance* USkeletalMeshComponent::GetOrCreateStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    return StateMachine;
}

// ==== Lua-friendly Blend Space helper: switch this component to a blend space 2D anim instance ====
void USkeletalMeshComponent::UseBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimBlendSpaceInstance already exists\n");
    }
}

UAnimBlendSpaceInstance* USkeletalMeshComponent::GetOrCreateBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    return BS;
}

void USkeletalMeshComponent::StopAnimation()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->Stop();
    }
}

void USkeletalMeshComponent::SetAnimationPosition(float InSeconds)
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->SetPosition(InSeconds, false);
    }
}

bool USkeletalMeshComponent::IsPlayingAnimation() const
{
    return AnimInstance ? AnimInstance->IsPlaying() : false;
}

float USkeletalMeshComponent::GetAnimationPosition()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        return Single->GetPosition();
    }
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}


FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        FTransform Result = GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);

        // TODO: Leaf 노드 본에서 Scale3D가 0이 되는 근본 원인 분석 필요
        // Scale3D가 0인 경우 (1,1,1)로 보정 (유효하지 않은 Scale 방지)
        if (Result.Scale3D.X == 0.0f) Result.Scale3D.X = 1.0f;
        if (Result.Scale3D.Y == 0.0f) Result.Scale3D.Y = 1.0f;
        if (Result.Scale3D.Z == 0.0f) Result.Scale3D.Z = 1.0f;

        return Result;
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();

    // ComponentSpace -> Final Skinning Matrices 계산
    // UpdateFinalSkinningMatrices()에서 UpdateSkinningMatrices()를 호출하여 본 행렬 계산 시간과 함께 전달
    UpdateFinalSkinningMatrices();
    // PerformSkinning은 CollectMeshBatches에서 전역 모드에 따라 수행됨
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    // 본 행렬 계산 시간 측정 시작
    uint64 BoneMatrixCalcStart = FWindowsPlatformTime::Cycles64();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;

        // 스케일 포함하여 행렬 생성
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }

    // 본 행렬 계산 시간 측정 종료
    uint64 BoneMatrixCalcEnd = FWindowsPlatformTime::Cycles64();
    double BoneMatrixCalcTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneMatrixCalcEnd - BoneMatrixCalcStart);

    // 본 행렬 계산 시간을 부모 USkinnedMeshComponent로 전달
    // 부모에서 실제 스키닝 모드(CPU/GPU)에 따라 통계에 추가됨
    UpdateSkinningMatrices(TempFinalSkinningMatrices, BoneMatrixCalcTimeMS);
}

void USkeletalMeshComponent::ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms)
{
    if (AdditiveTransforms.IsEmpty()) return;

    // CurrentLocalSpacePose는 TickComponent에서 이미 기본 애니메이션 포즈를 포함하고 있어야 함
    for (auto const& [BoneIndex, AdditiveTransform] : AdditiveTransforms)
    {
        if (BoneIndex >= 0 && BoneIndex < CurrentLocalSpacePose.Num())
        {
            // 회전이 위치에 영향을 주지 않도록 각 성분을 개별적으로 적용
            FTransform& BasePose = CurrentLocalSpacePose[BoneIndex];

            // 위치: 단순 덧셈
            FVector FinalLocation = BasePose.Translation + AdditiveTransform.Translation;

            // 회전: 쿼터니언 곱셈
            FQuat FinalRotation = AdditiveTransform.Rotation * BasePose.Rotation;

            // 스케일: 성분별 곱셈
            FVector FinalScale = FVector(
                BasePose.Scale3D.X * AdditiveTransform.Scale3D.X,
                BasePose.Scale3D.Y * AdditiveTransform.Scale3D.Y,
                BasePose.Scale3D.Z * AdditiveTransform.Scale3D.Z
            );

            CurrentLocalSpacePose[BoneIndex] = FTransform(FinalLocation, FinalRotation, FinalScale);
        }
    }

    // 모든 additive 적용 후 최종 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToRefPose()
{
    CurrentLocalSpacePose = RefPose;
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToAnimationPose()
{
    // BaseAnimationPose가 비어있으면 RefPose 사용
    if (BaseAnimationPose.IsEmpty())
    {
        CurrentLocalSpacePose = RefPose;
    }
    else
    {
        CurrentLocalSpacePose = BaseAnimationPose;
    }
    ForceRecomputePose();
}

void USkeletalMeshComponent::TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(NotifyEvent);
    }
}

// ============================================================================
// Ragdoll Physics Implementation
// ============================================================================

void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset)
{
    if (PhysicsAsset == InPhysicsAsset)
    {
        return;
    }

    // 기존 물리 상태 해제
    OnDestroyPhysicsState();

    PhysicsAsset = InPhysicsAsset;

    // 새 PhysicsAsset으로 물리 상태 생성 (시뮬레이션이 활성화된 경우에만)
    if (PhysicsAsset && bSimulatePhysics)
    {
        OnCreatePhysicsState();
    }
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bEnable)
{
    // SetPhysicsMode로 위임
    if (bEnable)
    {
        // 기존 모드가 Animation이면 Kinematic으로 전환
        if (PhysicsMode == EPhysicsMode::Animation)
        {
            SetPhysicsMode(EPhysicsMode::Kinematic);
        }
    }
    else
    {
        SetPhysicsMode(EPhysicsMode::Animation);
    }
}

void USkeletalMeshComponent::InitRagdoll(FPhysScene* InPhysScene)
{
    if (!PhysicsAsset || !SkeletalMesh || !InPhysScene)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: missing PhysicsAsset, SkeletalMesh, or PhysScene");
        return;
    }

    if (bRagdollInitialized)
    {
        TermRagdoll();
    }

    PhysScene = InPhysScene;
    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: no skeleton");
        return;
    }

    const int32 NumBones = Skeleton->Bones.Num();
    const int32 NumBodySetups = PhysicsAsset->BodySetups.Num();

    // Bodies 배열 초기화 (본 개수만큼, nullptr로)
    Bodies.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        Bodies[i] = nullptr;
    }

    // PxAggregate 생성 (자체 충돌 활성화: selfCollision = true)
    // 초기에 겹치는 바디만 나중에 충돌 무시 설정
    if (GPhysXSDK)
    {
        Aggregate = GPhysXSDK->createAggregate(NumBodySetups, true);  // true = 자체 충돌 활성화
    }

    // PhysicsAsset의 각 BodySetup에 대해 FBodyInstance 생성
    for (int32 BodyIdx = 0; BodyIdx < NumBodySetups; ++BodyIdx)
    {
        UBodySetup* BodySetup = PhysicsAsset->BodySetups[BodyIdx];
        if (!BodySetup)
        {
            continue;
        }

        // BodySetup에 저장된 BoneIndex 사용
        int32 BoneIndex = BodySetup->BoneIndex;
        if (BoneIndex < 0 || BoneIndex >= NumBones)
        {
            UE_LOG("[Ragdoll] BodySetup %d has invalid BoneIndex %d", BodyIdx, BoneIndex);
            continue;
        }

        // 본의 월드 트랜스폼 계산 (컴포넌트 스케일 적용)
        FTransform BoneWorldTransform = GetBoneWorldTransform(BoneIndex);
        BoneWorldTransform.Scale3D = GetWorldScale();  // 컴포넌트의 월드 스케일 적용

        // FBodyInstance 생성
        FBodyInstance* NewBody = new FBodyInstance();
        // Ragdoll 바디는 항상 Dynamic이어야 함 (Joint 연결을 위해)
        NewBody->bSimulatePhysics = true;
        NewBody->LinearDamping = 0.1f;     // 기본 감쇠
        NewBody->AngularDamping = 0.05f;
        NewBody->bIsRagdollBody = true;    // 랙돌 바디 표시

        // Kinematic 모드 설정 (애니메이션이 물리를 제어)
        NewBody->bKinematic = (PhysicsMode == EPhysicsMode::Kinematic);

        // 래그돌 바디는 PhysicsBody 채널 사용
        // PhysicsBody는 Pawn(캡슐)과 충돌하지 않음
        NewBody->ObjectType = ECollisionChannel::PhysicsBody;
        NewBody->CollisionMask = CollisionMasks::DefaultPhysicsBody;

        // 바디 초기화 (Aggregate에 추가)
        NewBody->InitBody(BodySetup, BoneWorldTransform, this, PhysScene, Aggregate);

        Bodies[BoneIndex] = NewBody;
    }

    // Aggregate를 Scene에 추가
    if (Aggregate && PhysScene && PhysScene->GetPxScene())
    {
        PhysScene->GetPxScene()->addAggregate(*Aggregate);
    }

    // 초기 포즈에서 겹치는 바디 쌍 검출 및 충돌 무시 설정
    SetupInitialOverlapFilters();

    // Constraints 생성
    for (int32 ConstraintIdx = 0; ConstraintIdx < PhysicsAsset->ConstraintSetups.Num(); ++ConstraintIdx)
    {
        const FConstraintSetup& Setup = PhysicsAsset->ConstraintSetups[ConstraintIdx];

        // BodySetup 인덱스 → BoneIndex 변환하여 FBodyInstance 찾기
        // Setup.ParentBodyIndex/ChildBodyIndex는 BodySetups 배열의 인덱스이고,
        // Bodies 배열은 BoneIndex로 인덱싱됨
        FBodyInstance* ParentBody = nullptr;
        FBodyInstance* ChildBody = nullptr;

        if (Setup.ParentBodyIndex >= 0 && Setup.ParentBodyIndex < PhysicsAsset->BodySetups.Num())
        {
            UBodySetup* ParentSetup = PhysicsAsset->BodySetups[Setup.ParentBodyIndex];
            if (ParentSetup && ParentSetup->BoneIndex >= 0 && ParentSetup->BoneIndex < Bodies.Num())
            {
                ParentBody = Bodies[ParentSetup->BoneIndex];
            }
        }
        if (Setup.ChildBodyIndex >= 0 && Setup.ChildBodyIndex < PhysicsAsset->BodySetups.Num())
        {
            UBodySetup* ChildSetup = PhysicsAsset->BodySetups[Setup.ChildBodyIndex];
            if (ChildSetup && ChildSetup->BoneIndex >= 0 && ChildSetup->BoneIndex < Bodies.Num())
            {
                ChildBody = Bodies[ChildSetup->BoneIndex];
            }
        }

        // 둘 다 없으면 스킵
        if (!ParentBody && !ChildBody)
        {
            continue;
        }

        // 둘 다 유효한 RigidActor가 있는지 확인
        bool bParentValid = ParentBody && ParentBody->RigidActor;
        bool bChildValid = ChildBody && ChildBody->RigidActor;

        if (!bParentValid && !bChildValid)
        {
            UE_LOG("[Ragdoll] Skipping constraint %d: no valid RigidActor", ConstraintIdx);
            continue;
        }

        FConstraintInstance* NewConstraint = new FConstraintInstance();
        NewConstraint->InitConstraint(Setup, ParentBody, ChildBody, PhysScene);

        // Joint가 성공적으로 생성된 경우에만 추가
        if (NewConstraint->IsValid())
        {
            Constraints.Add(NewConstraint);
        }
        else
        {
            UE_LOG("[Ragdoll] Constraint %d failed to create joint", ConstraintIdx);
            delete NewConstraint;
        }
    }

    bRagdollInitialized = true;
    UE_LOG("[Ragdoll] Initialized with %d bodies and %d constraints",
           PhysicsAsset->BodySetups.Num(), Constraints.Num());
}

void USkeletalMeshComponent::TermRagdoll()
{
    // Constraints 해제
    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint)
        {
            Constraint->TermConstraint();
            delete Constraint;
        }
    }
    Constraints.Empty();

    // Bodies 해제
    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody();
            delete Body;
        }
    }
    Bodies.Empty();

    // Aggregate 해제
    if (Aggregate)
    {
        if (PhysScene && PhysScene->GetPxScene())
        {
            PhysScene->GetPxScene()->removeAggregate(*Aggregate);
        }
        Aggregate->release();
        Aggregate = nullptr;
    }

    bRagdollInitialized = false;
    PhysScene = nullptr;

    UE_LOG("[Ragdoll] Terminated");
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(int32 BoneIndex) const
{
    if (BoneIndex >= 0 && BoneIndex < Bodies.Num())
    {
        return Bodies[BoneIndex];
    }
    return nullptr;
}

void USkeletalMeshComponent::AddImpulse(const FVector& Impulse, const FName& BoneName)
{
    if (!bRagdollInitialized)
    {
        return;
    }

    if (BoneName.ToString().empty())
    {
        // 모든 바디에 충격 적용
        for (FBodyInstance* Body : Bodies)
        {
            if (Body && Body->IsDynamic())
            {
                Body->AddForce(Impulse, true);  // bAccelChange = true for impulse
            }
        }
    }
    else
    {
        // 특정 본에만 적용
        const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
        if (Skeleton)
        {
            for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
            {
                if (Skeleton->Bones[i].Name == BoneName)
                {
                    FBodyInstance* Body = GetBodyInstance(i);
                    if (Body && Body->IsDynamic())
                    {
                        Body->AddForce(Impulse, true);
                    }
                    break;
                }
            }
        }
    }
}

void USkeletalMeshComponent::AddForce(const FVector& Force, const FName& BoneName)
{
    if (!bRagdollInitialized)
    {
        return;
    }

    if (BoneName.ToString().empty())
    {
        // 모든 바디에 힘 적용
        for (FBodyInstance* Body : Bodies)
        {
            if (Body && Body->IsDynamic())
            {
                Body->AddForce(Force, false);
            }
        }
    }
    else
    {
        // 특정 본에만 적용
        const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
        if (Skeleton)
        {
            for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
            {
                if (Skeleton->Bones[i].Name == BoneName)
                {
                    FBodyInstance* Body = GetBodyInstance(i);
                    if (Body && Body->IsDynamic())
                    {
                        Body->AddForce(Force, false);
                    }
                    break;
                }
            }
        }
    }
}

void USkeletalMeshComponent::SyncBonesFromPhysics()
{
    if (!bRagdollInitialized || !bSimulatePhysics)
    {
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        return;
    }

    // 1단계: 모든 물리 바디의 월드 트랜스폼을 먼저 수집
    TArray<FTransform> BodyWorldTransforms;
    BodyWorldTransforms.SetNum(Bodies.Num());

    for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
    {
        FBodyInstance* Body = Bodies[BoneIndex];
        if (Body && Body->IsValidBodyInstance())
        {
            BodyWorldTransforms[BoneIndex] = Body->GetUnrealWorldTransform();
            BodyWorldTransforms[BoneIndex].Scale3D *= 0.01;
        }
        else
        {
            // Body가 없는 본은 기존 본 트랜스폼 사용
            BodyWorldTransforms[BoneIndex] = GetBoneWorldTransform(BoneIndex);
        }
    }

    // 2단계: 월드 → 로컬 변환 (물리 월드 트랜스폼 기준으로)
    FTransform ComponentWorldTransform = GetWorldTransform();

    for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
    {
        FBodyInstance* Body = Bodies[BoneIndex];
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        const int32 ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            // 루트 본: 컴포넌트 기준 로컬
            CurrentLocalSpacePose[BoneIndex] = ComponentWorldTransform.GetRelativeTransform(BodyWorldTransforms[BoneIndex]);
        }
        else
        {
            // 자식 본: 부모의 물리 월드 트랜스폼 기준
            CurrentLocalSpacePose[BoneIndex] = BodyWorldTransforms[ParentIndex].GetRelativeTransform(BodyWorldTransforms[BoneIndex]);
        }
    }

    // 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::SyncPhysicsFromBones()
{
    if (!bRagdollInitialized)
    {
        return;
    }

    // 각 본의 현재 월드 트랜스폼을 물리 바디에 적용
    for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
    {
        FBodyInstance* Body = Bodies[BoneIndex];
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        FTransform BoneWorldTransform = GetBoneWorldTransform(BoneIndex);

        // PhysX 액터의 글로벌 포즈 설정
        if (Body->RigidActor)
        {
            PxTransform PxPose = U2PTransform(BoneWorldTransform);
            Body->RigidActor->setGlobalPose(PxPose);
        }
    }
}

void USkeletalMeshComponent::SyncPhysicsFromAnimation()
{
    if (!bRagdollInitialized || PhysicsMode != EPhysicsMode::Kinematic)
    {
        return;
    }

    // 각 본의 현재 월드 트랜스폼을 Kinematic 타겟으로 설정
    for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
    {
        FBodyInstance* Body = Bodies[BoneIndex];
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        FTransform BoneWorldTransform = GetBoneWorldTransform(BoneIndex);
        Body->SetKinematicTarget(BoneWorldTransform);
    }
}

void USkeletalMeshComponent::SetPhysicsMode(EPhysicsMode NewMode)
{
    if (PhysicsMode == NewMode)
    {
        return;
    }

    EPhysicsMode OldMode = PhysicsMode;
    PhysicsMode = NewMode;

    switch (NewMode)
    {
    case EPhysicsMode::Animation:
        // 물리 상태 해제
        if (bRagdollInitialized)
        {
            TermRagdoll();
        }
        bSimulatePhysics = false;
        break;

    case EPhysicsMode::Kinematic:
        // 물리 상태 생성 (아직 안 되어있으면)
        bSimulatePhysics = true;
        if (!bRagdollInitialized)
        {
            OnCreatePhysicsState();
        }
        // 기존 바디들을 Kinematic으로 설정
        for (FBodyInstance* Body : Bodies)
        {
            if (Body)
            {
                Body->SetKinematic(true);
            }
        }
        // 현재 애니메이션 포즈로 물리 바디 동기화
        SyncPhysicsFromAnimation();
        break;

    case EPhysicsMode::Ragdoll:
        // 물리 상태 생성 (아직 안 되어있으면)
        bSimulatePhysics = true;
        if (!bRagdollInitialized)
        {
            OnCreatePhysicsState();
        }
        // 기존 바디들을 Dynamic으로 설정
        for (FBodyInstance* Body : Bodies)
        {
            if (Body)
            {
                Body->SetKinematic(false);
            }
        }
        // 현재 본 위치를 물리 바디에 동기화
        SyncPhysicsFromBones();
        break;
    }
}

// ============================================================================
// Test Ragdoll Implementation (Hardcoded - No PhysicsAsset)
// ============================================================================

void USkeletalMeshComponent::InitTestRagdoll(FPhysScene* InPhysScene)
{
    if (!SkeletalMesh || !InPhysScene)
    {
        UE_LOG("[TestRagdoll] InitTestRagdoll failed: missing SkeletalMesh or PhysScene");
        return;
    }

    if (bRagdollInitialized)
    {
        TermRagdoll();
    }

    PhysScene = InPhysScene;
    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG("[TestRagdoll] InitTestRagdoll failed: no skeleton");
        return;
    }

    const int32 NumBones = Skeleton->Bones.Num();
    UE_LOG("[TestRagdoll] Creating ragdoll for %d bones", NumBones);

    // Bodies 배열 초기화
    Bodies.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        Bodies[i] = nullptr;
    }

    // 각 본에 대해 UBodySetup을 동적으로 생성하고 FBodyInstance 생성
    TArray<UBodySetup*> TempBodySetups;
    TempBodySetups.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FBone& Bone = Skeleton->Bones[BoneIndex];

        // 본의 월드 트랜스폼
        FTransform BoneWorldTransform = GetBoneWorldTransform(BoneIndex);

        // 본 길이 계산 (자식 본까지의 거리 또는 기본값)
        float BoneLength = 0.1f;  // 기본 길이

        // 자식 본 찾아서 길이 계산
        for (int32 ChildIndex = 0; ChildIndex < NumBones; ++ChildIndex)
        {
            if (Skeleton->Bones[ChildIndex].ParentIndex == BoneIndex)
            {
                FTransform ChildWorldTransform = GetBoneWorldTransform(ChildIndex);
                FVector Diff = ChildWorldTransform.Translation - BoneWorldTransform.Translation;
                float ChildDistance = Diff.Size();
                if (ChildDistance > BoneLength)
                {
                    BoneLength = ChildDistance;
                }
            }
        }

        // UBodySetup 생성
        UBodySetup* BodySetup = NewObject<UBodySetup>();
        BodySetup->BoneName = Bone.Name;
        BodySetup->BoneIndex = BoneIndex;

        // 캡슐 shape 추가 (본 길이 기반)
        float CapsuleRadius = FMath::Max(0.02f, BoneLength * 0.15f);
        float CapsuleLength = FMath::Max(0.06f, BoneLength * 0.8f);  // 전체 길이

        FKSphylElem Capsule;
        Capsule.Radius = CapsuleRadius;
        Capsule.Length = CapsuleLength;
        // 캡슐을 본 방향(보통 X축)으로 정렬
        Capsule.Center = FVector(CapsuleLength * 0.5f, 0.0f, 0.0f);
        Capsule.Rotation = FQuat::Identity();
        BodySetup->AggGeom.SphylElems.Add(Capsule);

        TempBodySetups[BoneIndex] = BodySetup;

        // FBodyInstance 생성
        FBodyInstance* NewBody = new FBodyInstance();
        NewBody->bSimulatePhysics = true;
        NewBody->LinearDamping = 0.1f;
        NewBody->AngularDamping = 0.05f;

        // 바디 초기화
        NewBody->InitBody(BodySetup, BoneWorldTransform, this, PhysScene);

        Bodies[BoneIndex] = NewBody;

        UE_LOG("[TestRagdoll] Created body for bone[%d] '%s' (len=%.3f, r=%.3f, capsuleLen=%.3f)",
               BoneIndex, Bone.Name.c_str(), BoneLength, CapsuleRadius, CapsuleLength);
    }

    // TODO: Constraints 생성 - Local Frame 설정 필요
    // 현재는 바디만 테스트 (조인트 없이 각 본이 독립적으로 떨어짐)
    #if 0  // 조인트 비활성화 - Local Frame 문제 해결 후 활성화
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FBone& Bone = Skeleton->Bones[BoneIndex];
        int32 ParentIndex = Bone.ParentIndex;

        if (ParentIndex < 0)
        {
            // 루트 본은 조인트 없음
            continue;
        }

        FBodyInstance* ParentBody = Bodies[ParentIndex];
        FBodyInstance* ChildBody = Bodies[BoneIndex];

        if (!ParentBody || !ChildBody)
        {
            continue;
        }

        // FConstraintSetup 생성
        FConstraintSetup Setup;
        Setup.JointName = FName(Bone.Name + "_Joint");
        Setup.ParentBodyIndex = ParentIndex;
        Setup.ChildBodyIndex = BoneIndex;

        // 기본 각도 제한 설정 (Motion 타입은 기본값 Limited 사용)
        Setup.Swing1LimitDegrees = 30.0f;
        Setup.Swing2LimitDegrees = 30.0f;
        Setup.TwistLimitDegrees = 20.0f;
        Setup.Stiffness = 100.0f;
        Setup.Damping = 10.0f;

        FConstraintInstance* NewConstraint = new FConstraintInstance();
        NewConstraint->InitConstraint(Setup, ParentBody, ChildBody, PhysScene);

        Constraints.Add(NewConstraint);

        UE_LOG("[TestRagdoll] Created constraint '%s' (parent=%d, child=%d)",
               Setup.JointName.ToString().c_str(), ParentIndex, BoneIndex);
    }
    #endif

    bRagdollInitialized = true;
    UE_LOG("[TestRagdoll] Initialized with %d bodies and %d constraints", Bodies.Num(), Constraints.Num());
}

void USkeletalMeshComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (!Renderer || !PhysicsAsset) return;
    if (!GetOwner()) return;

    UWorld* World = GetOwner()->GetWorld();
    if (!World) return;

    // PIE 모드에서는 물리 디버그 안 그림
    if (World->bPie) return;

    // 본 트랜스폼 수집
    USkeletalMesh* SkelMesh = GetSkeletalMesh();
    if (!SkelMesh) return;

    const FSkeletalMeshData* MeshData = SkelMesh->GetSkeletalMeshData();
    if (!MeshData) return;

    int32 BoneCount = static_cast<int32>(MeshData->Skeleton.Bones.size());
    TArray<FTransform> BoneTransforms;
    BoneTransforms.SetNum(BoneCount);

    FMatrix ComponentToWorld = GetWorldMatrix();

    for (int32 i = 0; i < BoneCount; ++i)
    {
        // 컴포넌트 스페이스 트랜스폼을 월드 트랜스폼으로 변환
        FTransform ComponentSpaceTransform;
        if (i < CurrentComponentSpacePose.Num())
        {
            ComponentSpaceTransform = CurrentComponentSpacePose[i];
        }
        else
        {
            // CurrentComponentSpacePose가 없으면 바인드 포즈 사용
            ComponentSpaceTransform = MeshData->Skeleton.Bones[i].BindPose;
        }

        // 컴포넌트 월드 행렬 적용
        FMatrix BoneWorldMatrix = ComponentSpaceTransform.ToMatrix() * ComponentToWorld;
        BoneTransforms[i] = FTransform(BoneWorldMatrix);
    }

    // Body 삼각형 배치 생성
    FTrianglesBatch BodyTriBatch;
    FVector4 DebugColor(0.0f, 1.0f, 0.0f, 0.3f); // 반투명 녹색
    FPhysicsDebugUtils::GeneratePhysicsAssetDebugMesh(PhysicsAsset, BoneTransforms, DebugColor, BodyTriBatch);

    // Constraint 삼각형/라인 배치 생성
    FTrianglesBatch ConstraintTriBatch;
    FLinesBatch ConstraintLineBatch;
    FPhysicsDebugUtils::GenerateConstraintsDebugMesh(PhysicsAsset, BoneTransforms, -1, ConstraintTriBatch, ConstraintLineBatch);

    // Body 삼각형 렌더링
    if (BodyTriBatch.Vertices.Num() > 0)
    {
        Renderer->BeginTriangleBatch();
        Renderer->AddTriangles(BodyTriBatch.Vertices, BodyTriBatch.Indices, BodyTriBatch.Colors);
        Renderer->EndTriangleBatchAlwaysOnTop(FMatrix::Identity());
    }

    // Constraint 삼각형 렌더링
    if (ConstraintTriBatch.Vertices.Num() > 0)
    {
        Renderer->BeginTriangleBatch();
        Renderer->AddTriangles(ConstraintTriBatch.Vertices, ConstraintTriBatch.Indices, ConstraintTriBatch.Colors);
        Renderer->EndTriangleBatchAlwaysOnTop(FMatrix::Identity());
    }

    // Constraint 라인 렌더링
    if (ConstraintLineBatch.Num() > 0)
    {
        Renderer->BeginLineBatch();
        for (int32 i = 0; i < ConstraintLineBatch.Num(); ++i)
        {
            Renderer->AddLine(ConstraintLineBatch.StartPoints[i], ConstraintLineBatch.EndPoints[i], ConstraintLineBatch.Colors[i]);
        }
        Renderer->EndLineBatchAlwaysOnTop(FMatrix::Identity());
    }
}

void USkeletalMeshComponent::OnCreatePhysicsState()
{
    // NoCollision이면 물리 상태 생성하지 않음
    if (CollisionEnabled == ECollisionEnabled::NoCollision)
    {
        return;
    }

    // PhysicsAsset이 없으면 물리 생성 안함
    if (!PhysicsAsset)
    {
        return;
    }

    // 이미 초기화되어 있으면 스킵
    if (bRagdollInitialized)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FPhysScene* Scene = World->GetPhysicsScene();
    if (!Scene)
    {
        return;
    }

    // 랙돌 초기화
    InitRagdoll(Scene);
}

void USkeletalMeshComponent::OnDestroyPhysicsState()
{
    // 랙돌 해제
    if (bRagdollInitialized)
    {
        TermRagdoll();
    }
}

void USkeletalMeshComponent::OnPropertyChanged(const FProperty& Prop)
{
    USkinnedMeshComponent::OnPropertyChanged(Prop);

    if (Prop.Name == "bSimulatePhysics" || Prop.Name == "PhysicsAsset")
    {
        // 물리 상태 재생성
        OnDestroyPhysicsState();
        OnCreatePhysicsState();
    }
    else if (Prop.Name == "PhysicsMode")
    {
        // PhysicsMode 변경 시 바디 상태 업데이트
        if (bRagdollInitialized)
        {
            bool bKinematic = (PhysicsMode == EPhysicsMode::Kinematic);
            for (FBodyInstance* Body : Bodies)
            {
                if (Body)
                {
                    Body->SetKinematic(bKinematic);
                }
            }

            // 모드에 따라 동기화
            if (PhysicsMode == EPhysicsMode::Kinematic)
            {
                SyncPhysicsFromAnimation();
            }
            else if (PhysicsMode == EPhysicsMode::Ragdoll)
            {
                SyncPhysicsFromBones();
            }
        }
    }
}

void USkeletalMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
    USkinnedMeshComponent::OnUpdateTransform(UpdateTransformFlags, Teleport);

    // 물리 업데이트 스킵 플래그가 있으면 리턴
    if ((int32)UpdateTransformFlags & (int32)EUpdateTransformFlags::SkipPhysicsUpdate)
    {
        return;
    }

    // 랙돌이 초기화되어 있고, 물리 시뮬레이션 중이 아닐 때만 바디 트랜스폼 업데이트
    // (시뮬레이션 중이면 물리가 트랜스폼을 제어하므로 건드리면 안됨)
    if (bRagdollInitialized && !bSimulatePhysics)
    {
        bool bIsTeleport = Teleport != ETeleportType::None;

        // 모든 물리 바디의 트랜스폼 업데이트
        for (int32 BoneIndex = 0; BoneIndex < Bodies.Num(); ++BoneIndex)
        {
            FBodyInstance* Body = Bodies[BoneIndex];
            if (Body && Body->IsValidBodyInstance())
            {
                FTransform BoneWorldTransform = GetBoneWorldTransform(BoneIndex);
                Body->SetBodyTransform(BoneWorldTransform, bIsTeleport);
            }
        }
    }
}

void USkeletalMeshComponent::SetupInitialOverlapFilters()
{
    if (!PhysScene || !PhysScene->GetPxScene() || !PhysicsAsset)
    {
        return;
    }

    PxScene* Scene = PhysScene->GetPxScene();

    // 유효한 바디 인스턴스 목록 수집
    TArray<FBodyInstance*> ValidBodies;
    TArray<int32> ValidBodyIndices;
    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        if (Bodies[i] && Bodies[i]->RigidActor)
        {
            ValidBodies.Add(Bodies[i]);
            ValidBodyIndices.Add(i);
        }
    }

    // 먼저 각 바디의 Shape에 자신의 인덱스를 word3에 설정
    for (int32 i = 0; i < ValidBodies.Num(); ++i)
    {
        PxRigidActor* Actor = ValidBodies[i]->RigidActor;
        PxU32 NumShapes = Actor->getNbShapes();
        TArray<PxShape*> Shapes(NumShapes);
        Actor->getShapes(Shapes.GetData(), NumShapes);

        for (PxU32 s = 0; s < NumShapes; ++s)
        {
            PxFilterData FilterData = Shapes[s]->getSimulationFilterData();
            FilterData.word3 = i;  // 자신의 바디 인덱스 저장
            Shapes[s]->setSimulationFilterData(FilterData);
        }
    }

    int32 OverlapCount = 0;

    // 모든 바디 쌍에 대해 겹침 검사
    for (int32 i = 0; i < ValidBodies.Num(); ++i)
    {
        for (int32 j = i + 1; j < ValidBodies.Num(); ++j)
        {
            FBodyInstance* BodyA = ValidBodies[i];
            FBodyInstance* BodyB = ValidBodies[j];

            PxRigidActor* ActorA = BodyA->RigidActor;
            PxRigidActor* ActorB = BodyB->RigidActor;

            // 각 Shape 쌍에 대해 겹침 검사
            bool bOverlapping = false;

            PxU32 NumShapesA = ActorA->getNbShapes();
            PxU32 NumShapesB = ActorB->getNbShapes();

            TArray<PxShape*> ShapesA(NumShapesA);
            TArray<PxShape*> ShapesB(NumShapesB);

            ActorA->getShapes(ShapesA.GetData(), NumShapesA);
            ActorB->getShapes(ShapesB.GetData(), NumShapesB);

            for (PxU32 sa = 0; sa < NumShapesA && !bOverlapping; ++sa)
            {
                for (PxU32 sb = 0; sb < NumShapesB && !bOverlapping; ++sb)
                {
                    PxShape* ShapeA = ShapesA[sa];
                    PxShape* ShapeB = ShapesB[sb];

                    PxTransform PoseA = ActorA->getGlobalPose() * ShapeA->getLocalPose();
                    PxTransform PoseB = ActorB->getGlobalPose() * ShapeB->getLocalPose();

                    // PxGeometryQuery::overlap으로 겹침 검사
                    PxGeometryHolder GeomA = ShapeA->getGeometry();
                    PxGeometryHolder GeomB = ShapeB->getGeometry();

                    if (PxGeometryQuery::overlap(GeomA.any(), PoseA, GeomB.any(), PoseB))
                    {
                        bOverlapping = true;
                    }
                }
            }

            // 겹치는 경우 충돌 무시 설정
            if (bOverlapping)
            {
                OverlapCount++;

                // FilterData.word2에 충돌 무시할 바디 인덱스 비트 설정
                for (PxU32 sa = 0; sa < NumShapesA; ++sa)
                {
                    PxShape* Shape = ShapesA[sa];
                    PxFilterData FilterData = Shape->getSimulationFilterData();
                    FilterData.word2 |= (1 << j);  // j번 바디와 충돌 무시
                    Shape->setSimulationFilterData(FilterData);
                }
                for (PxU32 sb = 0; sb < NumShapesB; ++sb)
                {
                    PxShape* Shape = ShapesB[sb];
                    PxFilterData FilterData = Shape->getSimulationFilterData();
                    FilterData.word2 |= (1 << i);  // i번 바디와 충돌 무시
                    Shape->setSimulationFilterData(FilterData);
                }

                // 필터링 재설정
                Scene->resetFiltering(*ActorA);
                Scene->resetFiltering(*ActorB);
            }
        }
    }

    UE_LOG("[Ragdoll] SetupInitialOverlapFilters: Found %d overlapping body pairs", OverlapCount);
}