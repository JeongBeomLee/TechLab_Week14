#pragma once

/**
 * ETriangleMeshSource
 *
 * TriangleMesh 정점 데이터의 소스를 지정
 */
UENUM()
enum class ETriangleMeshSource : uint8
{
    Mesh,    // 렌더링 메시 전체 사용 (자동 생성)
    Custom   // 사용자 정의 정점/인덱스
};
