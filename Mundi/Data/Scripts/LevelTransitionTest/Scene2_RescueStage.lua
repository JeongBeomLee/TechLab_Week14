-- ════════════════════════════════════════════════════════════════════════
-- Scene2_RescueStage.lua
-- 구출 스테이지 테스트 스크립트
-- ════════════════════════════════════════════════════════════════════════
--
-- 사용법:
-- 1. Scene1에서 데이터를 저장하고 전환
-- 2. 이 씬에서 저장된 데이터를 확인
-- 3. 키보드 입력으로 테스트
--    - [1] 키: 사람 구출 (RescuedCount 증가)
--    - [2] 키: 수집한 아이템으로 플레이어 강화
--    - [3] 키: GameInstance 정보 출력
--    - [4] 키: 최종 통계 저장
--    - [Space] 키: 엔딩 씬으로 전환
--
-- ════════════════════════════════════════════════════════════════════════

-- 전역 변수
local peopleRescued = 0
local playerHealth = 0
local playerScore = 0
local collectedKeys = 0
local isPlayerEnhanced = false

-- ════════════════════════════════════════════════════════════════════════
-- 초기화
-- ════════════════════════════════════════════════════════════════════════
function OnBeginPlay()
    print("═══════════════════════════════════════════════════════")
    print("[Scene2] Rescue Stage - Begin Play")
    print("═══════════════════════════════════════════════════════")

    -- GameInstance에서 저장된 데이터 로드
    LoadPlayerState()

    print("Controls:")
    print("  [1] - Rescue Person")
    print("  [2] - Enhance Player (use collected items)")
    print("  [3] - Print GameInstance Info")
    print("  [4] - Save Final Statistics")
    print("  [Space] - Transition to Ending")
    print("═══════════════════════════════════════════════════════")
end

-- ════════════════════════════════════════════════════════════════════════
-- 저장된 플레이어 상태 로드
-- ════════════════════════════════════════════════════════════════════════
function LoadPlayerState()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] GameInstance is nil!")
        return
    end

    -- GameInstance에서 데이터 불러오기
    playerHealth = gi:GetPlayerHealth()
    playerScore = gi:GetPlayerScore()
    collectedKeys = gi:GetItemCount("Key")

    local collectedItems = gi:GetInt("CollectedItems", 0)
    local lastScene = gi:GetString("LastScene", "Unknown")
    local missionComplete = gi:GetBool("MissionComplete", false)

    print("───────────────────────────────────────────────────────")
    print("[Scene2] Loaded Player State from GameInstance:")
    print(string.format("  - Health: %d", playerHealth))
    print(string.format("  - Score: %d", playerScore))
    print(string.format("  - Collected Keys: %d", collectedKeys))
    print(string.format("  - Items Collected (Scene1): %d", collectedItems))
    print(string.format("  - Last Scene: %s", lastScene))
    print(string.format("  - Mission Complete: %s", tostring(missionComplete)))
    print("───────────────────────────────────────────────────────")

    -- 검증
    if collectedKeys > 0 then
        print(string.format("[Scene2] ✓ Data persistence verified! Found %d keys from Scene1", collectedKeys))
    else
        print("[Scene2] ⚠ Warning: No keys found. Did you collect items in Scene1?")
    end
end

-- ════════════════════════════════════════════════════════════════════════
-- 매 프레임 업데이트
-- ════════════════════════════════════════════════════════════════════════
function Update(dt)
    local gi = GetGameInstance()
    if not gi then
        return
    end

    -- [1] 키: 사람 구출
    if Input:IsKeyPressed(Keycode.Alpha1) then
        RescuePerson()
    end

    -- [2] 키: 플레이어 강화
    if Input:IsKeyPressed(Keycode.Alpha2) then
        EnhancePlayer()
    end

    -- [3] 키: GameInstance 정보 출력
    if Input:IsKeyPressed(Keycode.Alpha3) then
        PrintGameInstanceInfo()
    end

    -- [4] 키: 최종 통계 저장
    if Input:IsKeyPressed(Keycode.Alpha4) then
        SaveFinalStatistics()
    end

    -- [Space] 키: 엔딩으로 전환
    if Input:IsKeyPressed(Keycode.Space) then
        TransitionToEnding()
    end
end

-- ════════════════════════════════════════════════════════════════════════
-- 사람 구출
-- ════════════════════════════════════════════════════════════════════════
function RescuePerson()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] Cannot rescue - GameInstance is nil")
        return
    end

    peopleRescued = peopleRescued + 1
    playerScore = playerScore + 500

    -- GameInstance에 구출 수 저장
    gi:SetRescuedCount(peopleRescued)
    gi:SetPlayerScore(playerScore)

    print(string.format("[Scene2] ★ Rescued person! Total rescued: %d", peopleRescued))
    print(string.format("[Scene2] Score: %d (+500)", playerScore))
end

-- ════════════════════════════════════════════════════════════════════════
-- 플레이어 강화 (수집한 아이템 사용)
-- ════════════════════════════════════════════════════════════════════════
function EnhancePlayer()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] Cannot enhance - GameInstance is nil")
        return
    end

    if isPlayerEnhanced then
        print("[Scene2] Player already enhanced!")
        return
    end

    -- 수집한 아이템 확인
    local keys = gi:GetItemCount("Key")
    local medkits = gi:GetItemCount("MedKit")
    local weapons = gi:GetItemCount("Weapon")

    if keys == 0 and medkits == 0 and weapons == 0 then
        print("[Scene2] No items to use for enhancement!")
        return
    end

    print("───────────────────────────────────────────────────────")
    print("[Scene2] Enhancing player with collected items...")

    -- Key: HP 증가
    if keys > 0 then
        local hpBonus = keys * 20
        playerHealth = playerHealth + hpBonus
        gi:SetPlayerHealth(playerHealth)
        print(string.format("  - Used %d Keys → +%d HP (Total: %d)", keys, hpBonus, playerHealth))
    end

    -- MedKit: 추가 HP 보너스
    if medkits > 0 then
        local hpBonus = medkits * 50
        playerHealth = playerHealth + hpBonus
        gi:SetPlayerHealth(playerHealth)
        print(string.format("  - Used %d MedKits → +%d HP (Total: %d)", medkits, hpBonus, playerHealth))
    end

    -- Weapon: 스코어 배율 증가
    if weapons > 0 then
        local scoreBonus = weapons * 1000
        playerScore = playerScore + scoreBonus
        gi:SetPlayerScore(playerScore)
        print(string.format("  - Used %d Weapons → +%d Score (Total: %d)", weapons, scoreBonus, playerScore))
    end

    print("───────────────────────────────────────────────────────")
    print("[Scene2] ★ Player enhanced successfully! ★")

    isPlayerEnhanced = true
end

-- ════════════════════════════════════════════════════════════════════════
-- GameInstance 정보 출력
-- ════════════════════════════════════════════════════════════════════════
function PrintGameInstanceInfo()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] GameInstance is nil")
        return
    end

    print("═══════════════════════════════════════════════════════")
    print("[Scene2] GameInstance Debug Info:")
    print("═══════════════════════════════════════════════════════")

    -- 아이템 정보
    local keyCount = gi:GetItemCount("Key")
    local medkitCount = gi:GetItemCount("MedKit")
    local weaponCount = gi:GetItemCount("Weapon")

    print("Items (from Scene1):")
    print(string.format("  - Key: %d", keyCount))
    print(string.format("  - MedKit: %d", medkitCount))
    print(string.format("  - Weapon: %d", weaponCount))

    -- 플레이어 상태
    local health = gi:GetPlayerHealth()
    local score = gi:GetPlayerScore()
    local rescued = gi:GetRescuedCount()

    print("Player State:")
    print(string.format("  - Health: %d", health))
    print(string.format("  - Score: %d", score))
    print(string.format("  - People Rescued: %d", rescued))

    -- 범용 데이터
    local collectedItems = gi:GetInt("CollectedItems", 0)
    local lastScene = gi:GetString("LastScene", "None")
    local missionComplete = gi:GetBool("MissionComplete", false)

    print("Custom Data:")
    print(string.format("  - CollectedItems (Scene1): %d", collectedItems))
    print(string.format("  - LastScene: %s", lastScene))
    print(string.format("  - MissionComplete: %s", tostring(missionComplete)))

    print("═══════════════════════════════════════════════════════")

    -- C++ 디버그 정보도 출력
    gi:PrintDebugInfo()
end

-- ════════════════════════════════════════════════════════════════════════
-- 최종 통계 저장
-- ════════════════════════════════════════════════════════════════════════
function SaveFinalStatistics()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] Cannot save statistics - GameInstance is nil")
        return
    end

    -- 최종 통계 계산
    local finalScore = playerScore
    local bonusScore = 0

    -- 보너스 점수 계산
    if peopleRescued >= 5 then
        bonusScore = bonusScore + 2000
        print("[Scene2] Bonus: Rescued 5+ people (+2000)")
    end

    if isPlayerEnhanced then
        bonusScore = bonusScore + 1000
        print("[Scene2] Bonus: Used items wisely (+1000)")
    end

    finalScore = finalScore + bonusScore

    -- GameInstance에 최종 통계 저장
    gi:SetInt("FinalScore", finalScore)
    gi:SetInt("BonusScore", bonusScore)
    gi:SetString("LastScene", "Scene2_RescueStage")
    gi:SetBool("GameCompleted", true)

    print("───────────────────────────────────────────────────────")
    print("[Scene2] Final Statistics Saved:")
    print(string.format("  - People Rescued: %d", peopleRescued))
    print(string.format("  - Base Score: %d", playerScore - bonusScore))
    print(string.format("  - Bonus Score: %d", bonusScore))
    print(string.format("  - Final Score: %d", finalScore))
    print("───────────────────────────────────────────────────────")
end

-- ════════════════════════════════════════════════════════════════════════
-- 엔딩으로 전환
-- ════════════════════════════════════════════════════════════════════════
function TransitionToEnding()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene2][ERROR] Cannot transition - GameInstance is nil")
        return
    end

    -- 전환 전에 통계 저장
    SaveFinalStatistics()

    print("═══════════════════════════════════════════════════════")
    print("[Scene2] Transitioning to Ending...")
    print("═══════════════════════════════════════════════════════")

    -- 엔딩 씬으로 전환
    TransitionToLevel("Data/Scenes/TEST.scene")

    -- 테스트용 메시지
    print("[Scene2] NOTE: Create Scene3_Ending.scene to test full flow")
    print("[Scene2] Test completed! Check console logs for data persistence verification")
end

-- ════════════════════════════════════════════════════════════════════════
-- 종료
-- ════════════════════════════════════════════════════════════════════════
function OnEndPlay()
    print("[Scene2] Rescue Stage - End Play")
end
