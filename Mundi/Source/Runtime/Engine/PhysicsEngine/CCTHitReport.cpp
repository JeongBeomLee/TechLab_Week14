#include "pch.h"
#include "CCTHitReport.h"

#include "PhysScene.h"
#include "BodyInstance.h"
#include "PrimitiveComponent.h"
#include "ControllerInstance.h"
#include "CapsuleComponent.h"

FCCTHitReport::FCCTHitReport(FPhysScene* InOwnerScene)
    : bPushDynamicObjects(true)
    , PushForceScale(5.0f)
    , OwnerScene(InOwnerScene)
{
}

FCCTHitReport::~FCCTHitReport()
{
}

void FCCTHitReport::onShapeHit(const PxControllerShapeHit& hit)
{
    if (!OwnerScene || !hit.controller || !hit.actor)
    {
        return;
    }

    // CCT 정보 추출
    FControllerInstance* CtrlInstance = static_cast<FControllerInstance*>(hit.controller->getUserData());
    if (!CtrlInstance || !CtrlInstance->OwnerComponent)
    {
        return;
    }

    // 충돌 대상 정보 추출
    FBodyInstance* OtherBody = static_cast<FBodyInstance*>(hit.actor->userData);
    if (!OtherBody || !OtherBody->OwnerComponent)
    {
        return;
    }

    // Dynamic 물체 밀기
    if (bPushDynamicObjects && hit.actor->is<PxRigidDynamic>())
    {
        PxRigidDynamic* Dynamic = hit.actor->is<PxRigidDynamic>();
        if (Dynamic && !(Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
        {
            // 밀기 힘 계산 (이동 방향 * 스케일)
            PxVec3 PushDir = hit.dir;
            PxVec3 Force = PushDir * PushForceScale;
            Dynamic->addForce(Force, PxForceMode::eIMPULSE);
        }
    }

    // FCollisionNotifyInfo 생성
    FCollisionNotifyInfo NotifyInfo;
    NotifyInfo.Type = ECollisionNotifyType::Hit;

    // CCT 측 정보 (Info0)
    NotifyInfo.Info0.Actor = CtrlInstance->OwnerComponent->GetOwner();
    NotifyInfo.Info0.Component = CtrlInstance->OwnerComponent;
    NotifyInfo.Info0.BodyIndex = 0;

    // 충돌 대상 정보 (Info1)
    NotifyInfo.Info1.Actor = OtherBody->OwnerComponent->GetOwner();
    NotifyInfo.Info1.Component = OtherBody->OwnerComponent;
    NotifyInfo.Info1.BodyIndex = 0;

    // 충돌 위치/노멀 (프로젝트가 cm 단위 직접 사용)
    NotifyInfo.ImpactPoint = FVector(
        static_cast<float>(hit.worldPos.x),
        static_cast<float>(hit.worldPos.y),
        static_cast<float>(hit.worldPos.z)
    );
    NotifyInfo.ImpactNormal = FVector(hit.worldNormal.x, hit.worldNormal.y, hit.worldNormal.z);
    NotifyInfo.TotalImpulse = hit.length;  // 근사치

    NotifyInfo.bCallEvent0 = true;
    NotifyInfo.bCallEvent1 = true;

    OwnerScene->AddPendingCollisionNotify(std::move(NotifyInfo));
}

void FCCTHitReport::onControllerHit(const PxControllersHit& hit)
{
    if (!OwnerScene || !hit.controller || !hit.other)
    {
        return;
    }

    // 두 CCT 정보 추출
    FControllerInstance* Ctrl0 = static_cast<FControllerInstance*>(hit.controller->getUserData());
    FControllerInstance* Ctrl1 = static_cast<FControllerInstance*>(hit.other->getUserData());

    if (!Ctrl0 || !Ctrl0->OwnerComponent || !Ctrl1 || !Ctrl1->OwnerComponent)
    {
        return;
    }

    // FCollisionNotifyInfo 생성
    FCollisionNotifyInfo NotifyInfo;
    NotifyInfo.Type = ECollisionNotifyType::Hit;

    // 첫 번째 CCT (Info0)
    NotifyInfo.Info0.Actor = Ctrl0->OwnerComponent->GetOwner();
    NotifyInfo.Info0.Component = Ctrl0->OwnerComponent;
    NotifyInfo.Info0.BodyIndex = 0;

    // 두 번째 CCT (Info1)
    NotifyInfo.Info1.Actor = Ctrl1->OwnerComponent->GetOwner();
    NotifyInfo.Info1.Component = Ctrl1->OwnerComponent;
    NotifyInfo.Info1.BodyIndex = 0;

    // 충돌 위치 (두 CCT 중간점, 프로젝트가 cm 단위 직접 사용)
    PxExtendedVec3 Pos0 = hit.controller->getPosition();
    PxExtendedVec3 Pos1 = hit.other->getPosition();
    FVector MidPoint = FVector(
        static_cast<float>((Pos0.x + Pos1.x) * 0.5),
        static_cast<float>((Pos0.y + Pos1.y) * 0.5),
        static_cast<float>((Pos0.z + Pos1.z) * 0.5)
    );
    NotifyInfo.ImpactPoint = MidPoint;

    // 충돌 노멀 (Ctrl0 -> Ctrl1 방향)
    FVector Dir = FVector(
        static_cast<float>(Pos1.x - Pos0.x),
        static_cast<float>(Pos1.y - Pos0.y),
        static_cast<float>(Pos1.z - Pos0.z)
    );
    NotifyInfo.ImpactNormal = Dir.GetNormalized();

    NotifyInfo.bCallEvent0 = true;
    NotifyInfo.bCallEvent1 = true;

    OwnerScene->AddPendingCollisionNotify(std::move(NotifyInfo));
}

void FCCTHitReport::onObstacleHit(const PxControllerObstacleHit& hit)
{
    // Obstacle은 현재 미사용
    // 필요시 구현
}
