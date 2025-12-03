#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; class UAnimSequence;
class UParticleSystem; class UParticleSystemComponent; class AActor; class UParticleModule;
class UPhysicsAsset;
class FPhysicsScene;
struct FBodyInstance;
namespace physx 
{
    class PxJoint; 
    class PxRigidStatic;
}

struct FAnimNotifyEvent
{
    float TriggerTime;
    float Duration;
    FName NotifyName;
    FString SoundPath;
    FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

struct FNotifyTrack
{
    FString Name;
    TArray<FAnimNotifyEvent> Notifies;

    FNotifyTrack() = default;
    FNotifyTrack(const FString& InName) : Name(InName) {}
};

struct FSelectedNotify
{
    int32 TrackIndex = -1;
    int32 NotifyIndex = -1;

    bool IsValid() const { return TrackIndex != -1 && NotifyIndex != -1; }
    void Invalidate() { TrackIndex = -1; NotifyIndex = -1; }
};

class ViewerState
{
public:
    // 가상 소멸자 - 파생 클래스 포인터를 기본 클래스 포인터로 delete할 때 필요
    virtual ~ViewerState() = default;

    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
    
    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;
    
    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;
    bool bRequestScrollToBone = false;

    // Animation State
    UAnimSequence* CurrentAnimation = nullptr;
    bool bIsPlaying = false;
    bool bIsLooping = true;
    bool bReversePlay = false;
    float PlaybackSpeed = 1.0f;
    float CurrentTime = 0.0f;
    float TotalTime = 0.0f;

    bool bIsScrubbing = false;

    float PreviousTime = 0.0f;  // The animation time from the previous frame to detect notify triggers

    TArray<UAnimSequence*> CompatibleAnimations;
    bool bShowOnlyCompatible = false;
    
    TArray<FNotifyTrack> NotifyTracks;

    bool bFoldNotifies = false;
    bool bFoldCurves = false;
    bool bFoldAttributes = false;

    FSelectedNotify SelectedNotify;

    bool bIsDirty = false;

    // Additive bone transforms applied on top of animation
    TMap<int32, FTransform> BoneAdditiveTransforms;

    // 기즈모 드래그 첫 프레임 감지용 (부동소수점 오차로 인한 불필요한 업데이트 방지)
    bool bWasGizmoDragging = false;
};

// 파티클 에디터 상태
struct ParticleEditorState : public ViewerState
{
    // 파티클 시스템
    UParticleSystem* EditingTemplate = nullptr;
    UParticleSystemComponent* PreviewComponent = nullptr;
    AActor* PreviewActor = nullptr;

    // 원점축 라인 컴포넌트 (탭별로 소유)
    class ULineComponent* OriginAxisLineComponent = nullptr;

    // Bounds 와이어프레임 라인 컴포넌트
    class ULineComponent* BoundsLineComponent = nullptr;

    // Bounds 캐시 (최대 크기 추적용 - 언리얼 방식)
    FVector CachedBoundsMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector CachedBoundsMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool bBoundsNeedRebuild = true;  // 라인 재생성 필요 플래그

    // Bounds 리셋 (파티클 시스템 재시작 시 호출)
    void ResetBounds()
    {
        CachedBoundsMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        CachedBoundsMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bBoundsNeedRebuild = true;
    }

    // 선택 상태
    int32 SelectedEmitterIndex = -1;
    int32 SelectedModuleIndex = -1;
    UParticleModule* SelectedModule = nullptr;

    // 파일 경로
    FString CurrentFilePath;
    bool bIsDirty = false;

    // 시뮬레이션 제어
    bool bIsSimulating = true;
    float SimulationSpeed = 1.0f;

    // 뷰포트 표시 옵션
    bool bShowBounds = false;
    bool bShowOriginAxis = false;
    FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // LOD 제어
    int32 CurrentLODLevel = 0;

    // LOD 레벨 변경 (모듈 선택 초기화 포함)
    void SetLODLevel(int32 NewLevel)
    {
        if (CurrentLODLevel != NewLevel)
        {
            CurrentLODLevel = NewLevel;
            // LOD 변경 시 모듈 선택 초기화 (다른 LOD의 모듈을 편집하는 버그 방지)
            SelectedModule = nullptr;
            SelectedModuleIndex = -1;
        }
    }
};

// 전방 선언
class FPrimitiveDrawInterface;

// Physics Asset Editor 상태
struct PhysicsAssetEditorState : public ViewerState
{
    // 편집 중인 Physics Asset
    UPhysicsAsset* EditingPhysicsAsset = nullptr;

    // 선택 상태
    int32 SelectedBodyIndex = -1;        // 그래프 내 하이라이트된 바디
    int32 SelectedConstraintIndex = -1;  // 그래프 내 하이라이트된 컨스트레인트
    int32 GraphRootBodyIndex = -1;       // 그래프의 중심 바디 (스켈레톤 트리에서만 변경)

    // === 시뮬레이션 상태 ===
    bool bIsSimulating = false;
    FPhysicsScene* SimulationScene = nullptr;
    TArray<FBodyInstance*> SimulatedBodies;
    TArray<physx::PxJoint*> SimulatedJoints;
    physx::PxRigidStatic* GroundPlane = nullptr;  // 바닥 충돌용 평면
    TArray<FTransform> OriginalBoneTransforms;  // 리셋용 원본 포즈
    float SimulationLeftoverTime = 0.0f;        // 고정 시간 스텝용

    // 표시 옵션
    bool bShowBodies = true;
    bool bShowConstraints = true;

    // 파일 경로
    FString SkeletalMeshPath;   // 스켈레탈 메시 경로 (프리뷰용)
    FString CurrentFilePath;    // Physics Asset 경로 (저장/불러오기용)

    // 시각화 라인 컴포넌트
    class ULineComponent* BodyShapeLineComponent = nullptr;           // 비선택 바디용
    class ULineComponent* SelectedBodyLineComponent = nullptr;        // 선택 바디용
    class ULineComponent* ConstraintLineComponent = nullptr;          // 비선택 Constraint용
    class ULineComponent* SelectedConstraintLineComponent = nullptr;  // 선택 Constraint용

    // 디버그 렌더링 인터페이스
    FPrimitiveDrawInterface* PDI = nullptr;                    // 비선택 바디용
    FPrimitiveDrawInterface* SelectedPDI = nullptr;            // 선택 바디용
    FPrimitiveDrawInterface* ConstraintPDI = nullptr;          // 비선택 Constraint용
    FPrimitiveDrawInterface* SelectedConstraintPDI = nullptr;  // 선택 Constraint용

    // BoneTM 캐시 (본 인덱스 -> 월드 트랜스폼)
    TMap<int32, FTransform> CachedBoneTM;
    bool bBoneTMCacheDirty = true;      // 캐시 무효화 플래그

    // Shape 라인 재생성 플래그 (세분화)
    bool bAllBodyLinesDirty = true;           // 전체 바디 라인 재생성
    bool bSelectedBodyLineDirty = true;       // 선택 바디 라인만 재생성
    bool bAllConstraintLinesDirty = true;     // 전체 Constraint 라인 재생성
    bool bSelectedConstraintLineDirty = true; // 선택 Constraint 라인만 재생성
    int32 LastSelectedBodyIndex = -1;         // 이전 선택 바디 (변경 감지용)
    int32 LastSelectedConstraintIndex = -1;   // 이전 선택 컨스트레인트 (변경 감지용)

    // 패널 비율 (좌측: Hierarchy/Graph, 우측: Details/Tool)
    float LeftTopRatio = 0.6f;
    float RightTopRatio = 0.6f;

    // Graph 패널 줌/팬
    float GraphZoomLevel = 0.6f;        // 기본 줌 레벨
    ImVec2 GraphPanOffset = ImVec2(0, 0);  // 팬 오프셋

    // 더티 플래그
    bool bIsDirty = false;

    // 경고/에러 팝업용
    FString PendingWarningMessage;

    // === Tool 패널 파라미터 ===
    // 바디 생성 옵션
    int32 ToolGeomType = 2;                 // 0: Sphere, 1: Box, 2: Capsule (기본값)
    float ToolBodySizeScale = 1.0f;         // 바디 크기 비율 (0.5~1.0)
    bool bToolBodyForAll = false;           // 모든 본에 바디 생성
    float ToolMinBoneSize = 0.25f;          // 이 크기 미만 본은 무시 (미터)

    // 컨스트레인트 생성 옵션
    bool bToolCreateConstraints = true;     // 컨스트레인트 자동 생성
    int32 ToolAngularMode = 1;              // 0: Free, 1: Limited (기본값), 2: Locked
    float ToolSwingLimit = 45.0f;           // Swing 제한 각도 (도)
    float ToolTwistLimit = 45.0f;           // Twist 제한 각도 (도)
};