#pragma once

/**
 * @brief 스켈레탈 메시의 물리 모드
 */
UENUM()
enum class EPhysicsMode : uint8
{
    /** 물리 없음, 애니메이션만 재생 */
    Animation,

    /** 애니메이션이 물리 바디를 제어 (다른 물체와 충돌 가능) */
    Kinematic,

    /** 물리가 본을 제어 (래그돌) */
    Ragdoll
};
