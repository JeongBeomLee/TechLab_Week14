#pragma once
#include "Info.h"
#include "ALevelTransitionManager.generated.h"

/**
 * 레벨 전환 상태
 */
enum class ELevelTransitionState : uint8
{
    Idle,           // 전환 중이 아님
    Transitioning   // 전환 중
};

/**
 * 레벨 전환 관리자 (런타임 전용)
 * - PIE 모드에서만 활성화
 * - Persistent Actor로 설정되어 씬 전환 시에도 유지됨
 * - GameInstance와 함께 PIE 세션 동안 영구 보존
 * - Lua에서 호출 가능한 TransitionToLevel() 제공
 *
 * 설계 원칙:
 * 1. PIE를 절대 종료하지 않음 (런타임 전환만 사용)
 * 2. World::SetLevel()로 직접 씬 교체
 * 3. Tick 완료 후 프레임 경계에서만 전환 (안전성)
 * 4. 각 씬은 독립적인 .scene 파일로 관리
 */
UCLASS(DisplayName="ALevelTransitionManager")
class ALevelTransitionManager : public AInfo
{
public:
    GENERATED_REFLECTION_BODY()

    ALevelTransitionManager();
    virtual ~ALevelTransitionManager() override;

    // ════════════════════════════════════════════════════════════════════════
    // 레벨 전환 API

    /**
     * 지정된 레벨로 전환 (다음 프레임 시작 시 실행)
     * @param LevelPath 레벨 파일 경로 (예: L"Data/Scenes/Level2.scene")
     *
     * 전환 과정:
     * 1. 현재 프레임 Tick 완료 대기
     * 2. 현재 레벨의 모든 액터 정리 (Persistent Actor 제외)
     * 3. 새 레벨 로드 및 액터 생성
     * 4. Persistent Actor들 새 레벨에 재등록
     * 5. BeginPlay 호출 (Persistent Actor 제외)
     */
    void TransitionToLevel(const FWideString& LevelPath);

    /**
     * 현재 전환 중인지 여부 (대기 중 포함)
     */
    bool IsTransitioning() const { return bPendingTransition || TransitionState == ELevelTransitionState::Transitioning; }

    /**
     * 현재 전환 상태 조회
     */
    ELevelTransitionState GetTransitionState() const { return TransitionState; }

    /**
     * 다음 씬 경로 설정 (Lua에서 각 씬마다 호출)
     * @param NextLevelPath 이 씬에서 전환할 다음 씬 경로
     *
     * 사용 예시 (Lua):
     * local manager = World:GetLevelTransitionManager()
     * manager:SetNextLevel("Data/Scenes/Level2.scene")
     */
    void SetNextLevel(const FWideString& NextLevelPath) { NextScenePath = NextLevelPath; }

    /**
     * 설정된 다음 씬으로 전환 (Lua에서 간편하게 호출)
     */
    void TransitionToNextLevel();

    /**
     * 현재 설정된 다음 씬 경로 조회
     */
    FWideString GetNextLevel() const { return NextScenePath; }

    /**
     * 대기 중인 전환 처리 (EditorEngine::MainLoop에서 호출)
     * Tick 완료 후 안전하게 씬 전환
     */
    void ProcessPendingTransition();

    // ════════════════════════════════════════════════════════════════════════
    // Actor 오버라이드

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // 전환 상태
    ELevelTransitionState TransitionState = ELevelTransitionState::Idle;

    // 지연 전환을 위한 변수 (프레임 경계에서 실행)
    bool bPendingTransition = false;
    FWideString PendingLevelPath;

    // 현재 씬의 다음 씬 경로 (각 씬의 Lua 스크립트에서 설정)
    FWideString NextScenePath;

    // 레벨 파일 존재 여부 확인 (유틸리티)
    bool DoesLevelFileExist(const FWideString& LevelPath) const;
};
