#pragma once

#include "PhysXSupport.h"

class FPhysScene;

/**
 * FCCTHitReport
 *
 * CCT 충돌 이벤트를 기존 FCollisionNotifyInfo 시스템에 연결하는 브릿지 클래스
 * PxUserControllerHitReport를 구현하여 CCT 충돌 시 호출됨
 */
class FCCTHitReport : public PxUserControllerHitReport
{
public:
    FCCTHitReport(FPhysScene* InOwnerScene);
    virtual ~FCCTHitReport();

    // ==================================================================================
    // PxUserControllerHitReport Interface
    // ==================================================================================

    /**
     * CCT가 Shape(RigidBody)와 충돌했을 때 호출
     * Static/Dynamic 물체와의 충돌 처리
     */
    virtual void onShapeHit(const PxControllerShapeHit& hit) override;

    /**
     * CCT가 다른 CCT와 충돌했을 때 호출
     * 캐릭터끼리의 충돌 처리
     */
    virtual void onControllerHit(const PxControllersHit& hit) override;

    /**
     * CCT가 Obstacle과 충돌했을 때 호출
     * (현재 미사용, 필요시 구현)
     */
    virtual void onObstacleHit(const PxControllerObstacleHit& hit) override;

    // ==================================================================================
    // 설정
    // ==================================================================================

    /** Dynamic 물체 밀기 활성화 여부 */
    bool bPushDynamicObjects = true;

    /** 밀기 힘 배율 */
    float PushForceScale = 0.001f;

private:
    /** 소유 물리 씬 */
    FPhysScene* OwnerScene;
};
