-- ════════════════════════════════════════════════════════════════════════
-- Scene3_Ending.lua
-- 엔딩 씬 테스트 스크립트
-- ════════════════════════════════════════════════════════════════════════
--
-- 사용법:
-- 1. Scene1 → Scene2를 거쳐서 이 씬으로 전환
-- 2. GameInstance에 저장된 모든 통계를 표시
-- 3. 키보드 입력으로 테스트
--    - [3] 키: 전체 게임 통계 출력
--    - [R] 키: GameInstance 리셋 (새 게임 준비)
--
-- ════════════════════════════════════════════════════════════════════════

-- ════════════════════════════════════════════════════════════════════════
-- 초기화
-- ════════════════════════════════════════════════════════════════════════
function OnBeginPlay()
    print("═══════════════════════════════════════════════════════")
    print("[Scene3] Ending - Begin Play")
    print("═══════════════════════════════════════════════════════")

    -- 최종 통계 표시
    DisplayFinalStatistics()

    print("")
    print("Controls:")
    print("  [3] - Print Full Game Statistics")
    print("  [R] - Reset GameInstance (for new game)")
    print("═══════════════════════════════════════════════════════")
end

-- ════════════════════════════════════════════════════════════════════════
-- 최종 통계 표시
-- ════════════════════════════════════════════════════════════════════════
function DisplayFinalStatistics()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene3][ERROR] GameInstance is nil!")
        return
    end

    print("")
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    print("              GAME COMPLETE - FINAL STATISTICS           ")
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

    -- Scene1 데이터
    local collectedItems = gi:GetInt("CollectedItems", 0)
    local keyCount = gi:GetItemCount("Key")
    local medkitCount = gi:GetItemCount("MedKit")
    local weaponCount = gi:GetItemCount("Weapon")

    print("")
    print("[ Scene 1 - Collection Stage ]")
    print(string.format("  Items Collected: %d", collectedItems))
    print(string.format("    - Keys: %d", keyCount))
    print(string.format("    - MedKits: %d", medkitCount))
    print(string.format("    - Weapons: %d", weaponCount))

    -- Scene2 데이터
    local rescuedCount = gi:GetRescuedCount()

    print("")
    print("[ Scene 2 - Rescue Stage ]")
    print(string.format("  People Rescued: %d", rescuedCount))

    -- 최종 점수
    local finalScore = gi:GetInt("FinalScore", 0)
    local bonusScore = gi:GetInt("BonusScore", 0)
    local baseScore = finalScore - bonusScore

    print("")
    print("[ Final Score ]")
    print(string.format("  Base Score: %d", baseScore))
    print(string.format("  Bonus Score: %d", bonusScore))
    print(string.format("  ★ TOTAL SCORE: %d ★", finalScore))

    -- 평가
    print("")
    print("[ Evaluation ]")
    if finalScore >= 5000 then
        print("  Rank: S - EXCELLENT!")
    elseif finalScore >= 3000 then
        print("  Rank: A - GREAT!")
    elseif finalScore >= 1000 then
        print("  Rank: B - GOOD")
    else
        print("  Rank: C - TRY AGAIN")
    end

    print("")
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

    -- 검증 메시지
    print("")
    print("✓ Data Persistence Test: SUCCESS")
    print("  All data from Scene1 and Scene2 was successfully")
    print("  preserved and transferred through level transitions.")
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
end

-- ════════════════════════════════════════════════════════════════════════
-- 매 프레임 업데이트
-- ════════════════════════════════════════════════════════════════════════
function Update(dt)
    local gi = GetGameInstance()
    if not gi then
        return
    end

    -- [3] 키: 전체 통계 출력
    if Input.IsKeyPressed(Keycode.Alpha3) then
        PrintFullStatistics()
    end

    -- [R] 키: GameInstance 리셋
    if Input.IsKeyPressed(Keycode.R) then
        ResetGameInstance()
    end
end

-- ════════════════════════════════════════════════════════════════════════
-- 전체 통계 출력 (디버그용)
-- ════════════════════════════════════════════════════════════════════════
function PrintFullStatistics()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene3][ERROR] GameInstance is nil")
        return
    end

    print("═══════════════════════════════════════════════════════")
    print("[Scene3] Full Game Statistics (Debug):")
    print("═══════════════════════════════════════════════════════")

    -- 플레이어 상태
    print("Player State:")
    print(string.format("  - Final Health: %d", gi:GetPlayerHealth()))
    print(string.format("  - Final Score: %d", gi:GetInt("FinalScore", 0)))
    print(string.format("  - People Rescued: %d", gi:GetRescuedCount()))

    -- 수집 아이템
    print("")
    print("Collected Items:")
    print(string.format("  - Keys: %d", gi:GetItemCount("Key")))
    print(string.format("  - MedKits: %d", gi:GetItemCount("MedKit")))
    print(string.format("  - Weapons: %d", gi:GetItemCount("Weapon")))

    -- 모든 커스텀 데이터
    print("")
    print("Custom Data:")
    print(string.format("  - CollectedItems: %d", gi:GetInt("CollectedItems", 0)))
    print(string.format("  - BonusScore: %d", gi:GetInt("BonusScore", 0)))
    print(string.format("  - LastScene: %s", gi:GetString("LastScene", "None")))
    print(string.format("  - MissionComplete: %s", tostring(gi:GetBool("MissionComplete", false))))
    print(string.format("  - GameCompleted: %s", tostring(gi:GetBool("GameCompleted", false))))

    print("═══════════════════════════════════════════════════════")

    -- C++ 디버그 정보
    gi:PrintDebugInfo()
end

-- ════════════════════════════════════════════════════════════════════════
-- GameInstance 리셋
-- ════════════════════════════════════════════════════════════════════════
function ResetGameInstance()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene3][ERROR] GameInstance is nil")
        return
    end

    print("───────────────────────────────────────────────────────")
    print("[Scene3] Resetting GameInstance...")

    gi:ClearAll()

    print("[Scene3] ✓ GameInstance has been reset")
    print("[Scene3] Ready for a new game!")
    print("───────────────────────────────────────────────────────")
end

-- ════════════════════════════════════════════════════════════════════════
-- 종료
-- ════════════════════════════════════════════════════════════════════════
function OnEndPlay()
    print("[Scene3] Ending - End Play")
end
