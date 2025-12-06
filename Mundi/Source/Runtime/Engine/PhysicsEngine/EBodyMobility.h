#pragma once

/**
 * EComponentMobility
 *
 * 컴포넌트의 이동성을 정의 (렌더링과 물리 모두에 영향)
 * - Static: 절대 움직이지 않음
 *   - 렌더링: 미리 굽는 라이팅/그림자
 *   - 물리: PxRigidStatic, TriangleMesh 허용
 * - Movable: 이동 가능
 *   - 렌더링: 실시간 라이팅/그림자
 *   - 물리: PxRigidDynamic, bSimulatePhysics에 따라 Dynamic/Kinematic 동작
 */
UENUM()
enum class EComponentMobility : uint8
{
    Static,   // 움직이지 않음
    Movable   // 이동 가능
};
