#include "pch.h"
#include "ConstraintInstance.h"
#include "BodyInstance.h"
#include "PhysScene.h"

FConstraintInstance::FConstraintInstance()
    : Joint(nullptr)
    , PhysScene(nullptr)
    , ParentBodyInstance(nullptr)
    , ChildBodyInstance(nullptr)
{
}

FConstraintInstance::~FConstraintInstance()
{
    TermConstraint();
}

void FConstraintInstance::InitConstraint(
    const FConstraintSetup& Setup,
    FBodyInstance* ParentBody,
    FBodyInstance* ChildBody,
    FPhysScene* InPhysScene)
{
    if (!GPhysXSDK)
    {
        UE_LOG("[PhysX Error] InitConstraint: GPhysXSDK is null");
        return;
    }

    // 기존 Joint 정리
    if (Joint)
    {
        TermConstraint();
    }

    // 최소 하나의 바디가 필요
    if (!ParentBody && !ChildBody)
    {
        UE_LOG("[PhysX Error] InitConstraint: Both bodies are null");
        return;
    }

    PhysScene = InPhysScene;
    ParentBodyInstance = ParentBody;
    ChildBodyInstance = ChildBody;

    // PhysX Actor 가져오기 (nullptr 허용 - 월드에 고정)
    PxRigidActor* ParentActor = ParentBody ? ParentBody->RigidActor : nullptr;
    PxRigidActor* ChildActor = ChildBody ? ChildBody->RigidActor : nullptr;

    if (!ParentActor && !ChildActor)
    {
        UE_LOG("[PhysX Error] InitConstraint: No valid RigidActor");
        return;
    }

    // 로컬 프레임 계산
    // 핵심: 초기 상태에서 두 프레임이 일치해야 초기 각도가 0이 됨
    PxTransform ParentLocalFrame = PxTransform(PxIdentity);
    PxTransform ChildLocalFrame = PxTransform(PxIdentity);

    if (ParentActor && ChildActor)
    {
        // 두 바디의 월드 트랜스폼 가져오기
        PxTransform ParentWorldPose = ParentActor->getGlobalPose();
        PxTransform ChildWorldPose = ChildActor->getGlobalPose();

        // 조인트 위치 = 자식 바디의 월드 위치 (본의 시작점)
        PxVec3 JointWorldPos = ChildWorldPose.p;

        // 부모 로컬 프레임: 조인트 위치를 부모 로컬로 변환, 회전은 부모 기준 Identity
        PxTransform ParentWorldInv = ParentWorldPose.getInverse();
        PxVec3 JointInParentLocal = ParentWorldInv.transform(JointWorldPos);

        // 부모와 자식의 상대 회전 계산
        PxQuat RelativeRot = ParentWorldPose.q.getConjugate() * ChildWorldPose.q;

        // 부모 프레임: 위치는 조인트 위치, 회전은 상대 회전
        ParentLocalFrame = PxTransform(JointInParentLocal, RelativeRot);

        // 자식 프레임: 원점, Identity 회전
        // 이렇게 하면 초기 상태에서 두 프레임이 월드에서 일치함 -> 초기 각도 = 0
        ChildLocalFrame = PxTransform(PxIdentity);
    }

    // PxD6Joint 생성
    Joint = PxD6JointCreate(
        *GPhysXSDK,
        ParentActor, ParentLocalFrame,
        ChildActor, ChildLocalFrame
    );

    if (!Joint)
    {
        UE_LOG("[PhysX Error] InitConstraint: Failed to create PxD6Joint");
        return;
    }

    // 제약 조건 설정 적용
    ConfigureJointMotion(Setup);
    ConfigureJointLimits(Setup);
    ConfigureJointDrive(Setup);

    // 디버그: 생성 직후 각도 확인
    float InitTwist, InitSwing1, InitSwing2;
    GetCurrentAngles(InitTwist, InitSwing1, InitSwing2);

    UE_LOG("[PhysX] Constraint '%s' created: InitAngles(Twist=%.1f, Swing1=%.1f, Swing2=%.1f)",
        Setup.JointName.ToString().c_str(), InitTwist, InitSwing1, InitSwing2);
}

void FConstraintInstance::TermConstraint()
{
    if (Joint)
    {
        Joint->release();
        Joint = nullptr;
    }

    PhysScene = nullptr;
    ParentBodyInstance = nullptr;
    ChildBodyInstance = nullptr;
}

void FConstraintInstance::UpdateFromSetup(const FConstraintSetup& Setup)
{
    if (!Joint)
    {
        return;
    }

    ConfigureJointMotion(Setup);
    ConfigureJointLimits(Setup);
    ConfigureJointDrive(Setup);
}

void FConstraintInstance::ConfigureJointMotion(const FConstraintSetup& Setup)
{
    if (!Joint)
    {
        return;
    }

    // 기본: 모든 선형 축 잠금 (위치 고정)
    Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    // 모든 회전축 제한 (BallAndSocket)
    Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
    Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
    Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

    UE_LOG("[Joint] ConfigureJointMotion: Linear=LOCKED, Angular=LIMITED");
}

void FConstraintInstance::ConfigureJointLimits(const FConstraintSetup& Setup)
{
    if (!Joint)
    {
        return;
    }

    // Setup에서 각도 가져오기
    float Swing1Rad = DegreesToRadians(Setup.Swing1Limit);
    float Swing2Rad = DegreesToRadians(Setup.Swing2Limit);
    float TwistMinRad = DegreesToRadians(Setup.TwistLimitMin);
    float TwistMaxRad = DegreesToRadians(Setup.TwistLimitMax);

    // Swing 제한 (Cone Limit)
    PxJointLimitCone SwingLimit(Swing1Rad, Swing2Rad, 0.05f);
    SwingLimit.stiffness = 0.0f;
    SwingLimit.damping = 0.0f;
    SwingLimit.restitution = 0.0f;
    Joint->setSwingLimit(SwingLimit);

    // Twist 제한 (Angular Pair Limit)
    PxJointAngularLimitPair TwistLimit(TwistMinRad, TwistMaxRad, 0.05f);
    TwistLimit.stiffness = 0.0f;
    TwistLimit.damping = 0.0f;
    TwistLimit.restitution = 0.0f;
    Joint->setTwistLimit(TwistLimit);

    // 프로젝션 활성화 - 제한 위반 시 강제 보정
    Joint->setProjectionLinearTolerance(0.1f);
    Joint->setProjectionAngularTolerance(DegreesToRadians(5.0f));
    Joint->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);
}

void FConstraintInstance::ConfigureJointDrive(const FConstraintSetup& Setup)
{
    if (!Joint)
    {
        return;
    }

    // 스프링 드라이브 설정 (Stiffness > 0 인 경우만)
    if (Setup.Stiffness > 0.0f || Setup.Damping > 0.0f)
    {
        PxD6JointDrive Drive(Setup.Stiffness, Setup.Damping, PX_MAX_F32, false);

        // 각 회전축에 드라이브 적용
        Joint->setDrive(PxD6Drive::eSWING, Drive);
        Joint->setDrive(PxD6Drive::eTWIST, Drive);
    }
}

void FConstraintInstance::GetCurrentAngles(float& OutTwist, float& OutSwing1, float& OutSwing2) const
{
    OutTwist = 0.0f;
    OutSwing1 = 0.0f;
    OutSwing2 = 0.0f;

    if (!Joint)
    {
        return;
    }

    // PhysX D6Joint에서 직접 각도 가져오기
    OutTwist = RadiansToDegrees(Joint->getTwistAngle());
    OutSwing1 = RadiansToDegrees(Joint->getSwingYAngle());
    OutSwing2 = RadiansToDegrees(Joint->getSwingZAngle());
}

void FConstraintInstance::LogCurrentAngles(const FName& JointName) const
{
    float Twist, Swing1, Swing2;
    GetCurrentAngles(Twist, Swing1, Swing2);

    UE_LOG("[Joint] %s: Twist=%.1f, Swing1=%.1f, Swing2=%.1f (deg)",
        JointName.ToString().c_str(), Twist, Swing1, Swing2);
}
