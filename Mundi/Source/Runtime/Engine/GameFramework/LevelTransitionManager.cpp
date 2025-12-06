#include "pch.h"
#include "LevelTransitionManager.h"
#include "World.h"
#include <filesystem>

IMPLEMENT_CLASS(ALevelTransitionManager)

ALevelTransitionManager::ALevelTransitionManager()
{
    ObjectName = "LevelTransitionManager";
    bCanEverTick = true;  // Tick 활성화 (Lua 스크립트 Update 호출을 위해)
}

ALevelTransitionManager::~ALevelTransitionManager()
{
    UE_LOG("[info] LevelTransitionManager: Destroyed");
}

void ALevelTransitionManager::BeginPlay()
{
    Super::BeginPlay();

    // PIE 세션 시작 시 모든 상태 초기화
    UE_LOG("[info] LevelTransitionManager: BeginPlay - Resetting transition state for new PIE session.");
    TransitionState = ELevelTransitionState::Idle;
    bPendingTransition = false;
    PendingLevelPath.clear();
    NextScenePath.clear();
}

void ALevelTransitionManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

// ════════════════════════════════════════════════════════════════════════
// 레벨 전환 API

void ALevelTransitionManager::TransitionToLevel(const FWideString& LevelPath)
{
    // 이미 전환 중이면 무시
    if (IsTransitioning() || bPendingTransition) { return; }

    // 경로가 비어있으면 에러
    if (LevelPath.empty()) { return; }

    // 파일이 존재하지 않으면 에러
    if (!DoesLevelFileExist(LevelPath)) { return; }

    // 2. 지연 전환 예약 (다음 프레임 초에 실행 - 현재 Tick 완료 보장)
    bPendingTransition = true;
    PendingLevelPath = LevelPath;
    TransitionState = ELevelTransitionState::Transitioning;
}

void ALevelTransitionManager::TransitionToNextLevel()
{
    if (NextScenePath.empty()) { return; }

    TransitionToLevel(NextScenePath);
}

void ALevelTransitionManager::ProcessPendingTransition()
{
    // bPendingTransition 플래그가 세워져 있으면 전환 실행
    if (!bPendingTransition) { return; }

    // LoadLevelFromFile() 호출 시 this가 파괴되므로, 경로를 로컬 변수에 복사
    FWideString LevelToLoad = PendingLevelPath;

    // 1. PIE 모드 확인
    if (!GEngine.IsPIEActive())
    {
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 2. PIE World 찾기
    UWorld* PIEWorld = nullptr;
    for (const auto& Context : GEngine.GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game)
        {
            PIEWorld = Context.World;
            break;
        }
    }

    if (!PIEWorld)
    {
        UE_LOG("[error] LevelTransitionManager: Could not find PIE world context.");
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 3. 런타임 씬 전환 (PIE 종료 없이 직접 교체)
    if (PIEWorld->LoadLevelFromFile(LevelToLoad) == false)
    {
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 4. 새 씬의 게임플레이 시작
    PIEWorld->BeginPlay();
}

// ════════════════════════════════════════════════════════════════════════
// 유틸리티

bool ALevelTransitionManager::DoesLevelFileExist(const FWideString& LevelPath) const
{
    try
    {
        std::filesystem::path FilePath(LevelPath);
        return std::filesystem::exists(FilePath);
    }
    catch (const std::exception& e)
    {
        UE_LOG("[error] LevelTransitionManager: Exception while checking file existence: %s", e.what());
        return false;
    }
}
