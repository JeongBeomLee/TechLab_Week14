#pragma once

/**
 * ECollisionChannel
 *
 * 충돌 채널 정의입니다.
 * 각 채널은 비트 플래그로 표현되어 충돌 마스크에서 사용됩니다.
 */
UENUM()
enum class ECollisionChannel : uint8
{
    /** 기본 월드 스태틱 오브젝트 (지형, 건물 등) */
    WorldStatic = 0,

    /** 월드 다이나믹 오브젝트 (움직이는 물체) */
    WorldDynamic = 1,

    /** Pawn 충돌 채널 (캐릭터 캡슐 등) */
    Pawn = 2,

    /** 물리 바디 채널 (래그돌 바디 등) */
    PhysicsBody = 3,

    /** 프로젝타일 채널 */
    Projectile = 4,

    /** 카메라 채널 */
    Camera = 5,

    /** 사용자 정의 채널 */
    Custom1 = 6,
    Custom2 = 7,

    /** 최대 채널 수 (8개) */
    MAX = 8
};

/**
 * 충돌 채널을 비트 마스크로 변환합니다.
 */
inline uint32 ChannelToBit(ECollisionChannel Channel)
{
    return 1u << static_cast<uint8>(Channel);
}

/**
 * 기본 충돌 마스크 정의
 */
namespace CollisionMasks
{
    /** 모든 채널과 충돌 */
    constexpr uint32 All = 0xFFFFFFFF;

    /** 아무것도 충돌하지 않음 */
    constexpr uint32 None = 0;

    /** WorldStatic 비트 */
    constexpr uint32 WorldStatic = (1u << static_cast<uint8>(ECollisionChannel::WorldStatic));

    /** WorldDynamic 비트 */
    constexpr uint32 WorldDynamic = (1u << static_cast<uint8>(ECollisionChannel::WorldDynamic));

    /** Pawn 비트 */
    constexpr uint32 Pawn = (1u << static_cast<uint8>(ECollisionChannel::Pawn));

    /** PhysicsBody 비트 */
    constexpr uint32 PhysicsBody = (1u << static_cast<uint8>(ECollisionChannel::PhysicsBody));

    /** Projectile 비트 */
    constexpr uint32 Projectile = (1u << static_cast<uint8>(ECollisionChannel::Projectile));

    /** 기본 Pawn 마스크: WorldStatic, WorldDynamic, Pawn, Projectile과 충돌 (PhysicsBody 제외) */
    constexpr uint32 DefaultPawn = WorldStatic | WorldDynamic | Pawn | Projectile;

    /** 기본 PhysicsBody 마스크: WorldStatic, WorldDynamic, PhysicsBody와 충돌 (Pawn 제외) */
    constexpr uint32 DefaultPhysicsBody = WorldStatic | WorldDynamic | PhysicsBody;
}
