#pragma once

/**
 * EConvexSource
 *
 * Convex 정점 데이터의 소스를 지정
 */
UENUM()
enum class EConvexSource : uint8
{
    Mesh,    // 메시 전체 정점 사용 (VertexData 불필요, 자동 생성)
    Custom   // 사용자 정의 정점 (VertexData 필요)
};
