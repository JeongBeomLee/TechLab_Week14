#pragma once

#include "FConstraintSetup.h"
#include "PhysXSupport.h"

struct FBodyInstance;
class FPhysScene;

/**
 * FConstraintInstance
 *
 * 두 FBodyInstance 간의 물리 제약 조건(Joint)을 관리하는 런타임 인스턴스
 * FConstraintSetup 데이터를 기반으로 PxD6Joint를 생성/관리
 */
struct FConstraintInstance
{
public:
    FConstraintInstance();
    ~FConstraintInstance();

    // 복사 방지 (PhysX 리소스 공유 방지)
    FConstraintInstance(const FConstraintInstance&) = delete;
    FConstraintInstance& operator=(const FConstraintInstance&) = delete;

    /**
     * 제약 조건을 초기화합니다.
     * @param Setup         제약 조건 설정 데이터
     * @param ParentBody    부모 바디 인스턴스
     * @param ChildBody     자식 바디 인스턴스
     * @param InPhysScene   물리 씬
     */
    void InitConstraint(
        const FConstraintSetup& Setup,
        FBodyInstance* ParentBody,
        FBodyInstance* ChildBody,
        FPhysScene* InPhysScene
    );

    /**
     * 제약 조건을 해제합니다.
     */
    void TermConstraint();

    /**
     * 유효성 검사
     */
    bool IsValid() const { return Joint != nullptr; }

    /**
     * PhysX Joint 반환
     */
    PxD6Joint* GetJoint() const { return Joint; }

    /**
     * 제약 조건 설정 업데이트 (런타임에 제약 속성 변경 시)
     */
    void UpdateFromSetup(const FConstraintSetup& Setup);

    /**
     * 현재 조인트 각도 정보 가져오기 (degrees)
     * @param OutTwist   X축 회전 (Twist)
     * @param OutSwing1  Y축 회전 (Swing1)
     * @param OutSwing2  Z축 회전 (Swing2)
     */
    void GetCurrentAngles(float& OutTwist, float& OutSwing1, float& OutSwing2) const;

    /**
     * 디버그: 현재 조인트 각도 로그 출력
     */
    void LogCurrentAngles(const FName& JointName) const;

private:
    /**
     * PxD6Joint 모션 설정 (BallAndSocket, Hinge 등)
     */
    void ConfigureJointMotion(const FConstraintSetup& Setup);

    /**
     * 각도 제한 설정
     */
    void ConfigureJointLimits(const FConstraintSetup& Setup);

    /**
     * 드라이브(스프링) 설정
     */
    void ConfigureJointDrive(const FConstraintSetup& Setup);

private:
    /** PhysX D6 Joint */
    PxD6Joint* Joint = nullptr;

    /** 소유 물리 씬 */
    FPhysScene* PhysScene = nullptr;

    /** 연결된 바디 인스턴스 (참조용) */
    FBodyInstance* ParentBodyInstance = nullptr;
    FBodyInstance* ChildBodyInstance = nullptr;
};
