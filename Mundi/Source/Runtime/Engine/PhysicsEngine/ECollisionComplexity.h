#pragma once

/**
 * ECollisionComplexity
 *
 * 충돌체 복잡도 설정
 * - UseSimple: 단순화된 충돌체 사용 (Convex, Box, Sphere 등)
 * - UseComplexAsSimple: 렌더링 메시를 그대로 충돌에 사용 (TriangleMesh, Static만 가능)
 */
UENUM()
enum class ECollisionComplexity : uint8
{
    UseSimple,           // 단순화된 충돌체 (Convex 자동 생성)
    UseComplexAsSimple   // 렌더링 메시 그대로 사용 (TriangleMesh, Static 전용)
};
