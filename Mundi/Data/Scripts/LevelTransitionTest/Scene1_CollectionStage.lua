-- ════════════════════════════════════════════════════════════════════════
-- Scene1_CollectionStage.lua
-- 아이템 수집 스테이지 테스트 스크립트
-- ════════════════════════════════════════════════════════════════════════
--
-- 사용법:
-- 1. 이 스크립트를 씬의 액터에 연결
-- 2. PIE 실행
-- 3. 키보드 입력으로 테스트
--    - [1] 키: 아이템 수집
--    - [2] 키: 플레이어 상태 저장
--    - [3] 키: GameInstance 정보 출력
--    - [Space] 키: 다음 씬으로 전환
--
-- ════════════════════════════════════════════════════════════════════════

-- 전역 변수
local itemsCollected = 0
local requiredItems = 5
local playerHealth = 100
local playerScore = 0

-- ════════════════════════════════════════════════════════════════════════
-- 초기화
-- ════════════════════════════════════════════════════════════════════════
function OnBeginPlay()
    print("═══════════════════════════════════════════════════════")
    print("[Scene1] Collection Stage - Begin Play")
    print("═══════════════════════════════════════════════════════")
    print("Controls:")
    print("  [1] - Collect Item (adds Key)")
    print("  [2] - Save Player State")
    print("  [3] - Print GameInstance Info")
    print("  [Space] - Transition to Scene2")
    print("═══════════════════════════════════════════════════════")

    -- GameInstance 초기화
    local gi = GetGameInstance()
    if gi then
        print("[Scene1] GameInstance is available")
        gi:ClearAll()  -- 테스트 시작 시 모든 데이터 초기화
        print("[Scene1] GameInstance cleared for fresh test")
    else
        print("[Scene1][ERROR] GameInstance is nil!")
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

    -- [1] 키: 아이템 수집
    if Input:IsKeyPressed(Keycode.Alpha1) then
        CollectItem("Key")
    end

    -- [2] 키: 플레이어 상태 저장
    if Input:IsKeyPressed(Keycode.Alpha2) then
        SavePlayerState()
    end

    -- [3] 키: GameInstance 정보 출력
    if Input:IsKeyPressed(Keycode.Alpha3) then
        PrintGameInstanceInfo()
    end

    -- [Space] 키: 씬 전환
    if Input:IsKeyPressed(Keycode.Space) then
        TransitionToNextScene()
    end
end

-- ════════════════════════════════════════════════════════════════════════
-- 아이템 수집
-- ════════════════════════════════════════════════════════════════════════
function CollectItem(itemTag)
    local gi = GetGameInstance()
    if not gi then
        print("[Scene1][ERROR] Cannot collect item - GameInstance is nil")
        return
    end

    -- GameInstance에 아이템 추가
    gi:AddItem(itemTag, 1)
    itemsCollected = itemsCollected + 1

    -- 점수 증가
    playerScore = playerScore + 100

    print(string.format("[Scene1] Collected %s! Total: %d/%d",
          itemTag, itemsCollected, requiredItems))
    print(string.format("[Scene1] Current Score: %d", playerScore))

    -- 목표 달성 확인
    if itemsCollected >= requiredItems then
        print("[Scene1] ★ Collection Complete! Press [Space] to continue ★")
    end
end

-- ════════════════════════════════════════════════════════════════════════
-- 플레이어 상태 저장
-- ════════════════════════════════════════════════════════════════════════
function SavePlayerState()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene1][ERROR] Cannot save state - GameInstance is nil")
        return
    end

    -- 플레이어 상태를 GameInstance에 저장
    gi:SetPlayerHealth(playerHealth)
    gi:SetPlayerScore(playerScore)
    gi:SetInt("CollectedItems", itemsCollected)
    gi:SetString("LastScene", "Scene1_CollectionStage")
    gi:SetBool("MissionComplete", itemsCollected >= requiredItems)

    print("───────────────────────────────────────────────────────")
    print("[Scene1] Player state saved to GameInstance")
    print(string.format("  - Health: %d", playerHealth))
    print(string.format("  - Score: %d", playerScore))
    print(string.format("  - Items Collected: %d", itemsCollected))
    print("───────────────────────────────────────────────────────")
end

-- ════════════════════════════════════════════════════════════════════════
-- GameInstance 정보 출력
-- ════════════════════════════════════════════════════════════════════════
function PrintGameInstanceInfo()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene1][ERROR] GameInstance is nil")
        return
    end

    print("═══════════════════════════════════════════════════════")
    print("[Scene1] GameInstance Debug Info:")
    print("═══════════════════════════════════════════════════════")

    -- 아이템 정보
    local keyCount = gi:GetItemCount("Key")
    local medkitCount = gi:GetItemCount("MedKit")
    local weaponCount = gi:GetItemCount("Weapon")

    print("Items:")
    print(string.format("  - Key: %d", keyCount))
    print(string.format("  - MedKit: %d", medkitCount))
    print(string.format("  - Weapon: %d", weaponCount))

    -- 플레이어 상태
    local health = gi:GetPlayerHealth()
    local score = gi:GetPlayerScore()

    print("Player State:")
    print(string.format("  - Health: %d", health))
    print(string.format("  - Score: %d", score))

    -- 범용 데이터
    local collectedItems = gi:GetInt("CollectedItems", 0)
    local lastScene = gi:GetString("LastScene", "None")
    local missionComplete = gi:GetBool("MissionComplete", false)

    print("Custom Data:")
    print(string.format("  - CollectedItems: %d", collectedItems))
    print(string.format("  - LastScene: %s", lastScene))
    print(string.format("  - MissionComplete: %s", tostring(missionComplete)))

    print("═══════════════════════════════════════════════════════")

    -- C++ 디버그 정보도 출력
    gi:PrintDebugInfo()
end

-- ════════════════════════════════════════════════════════════════════════
-- 다음 씬으로 전환
-- ════════════════════════════════════════════════════════════════════════
function TransitionToNextScene()
    local gi = GetGameInstance()
    if not gi then
        print("[Scene1][ERROR] Cannot transition - GameInstance is nil")
        return
    end

    -- 전환 전에 상태 저장
    SavePlayerState()

    print("═══════════════════════════════════════════════════════")
    print("[Scene1] Transitioning to Scene2...")
    print("═══════════════════════════════════════════════════════")

    -- 레벨 전환 (Scene2가 없으면 에러 발생)
    -- 실제 게임에서는 존재하는 씬 경로를 사용해야 함
    TransitionToLevel("Data/Scenes/TEST2.scene")

    -- 테스트용: 현재 씬 재로드로 대체 (Scene2가 없는 경우)
    print("[Scene1] NOTE: Create Scene2_RescueStage.scene to test transition")
    print("[Scene1] For now, reloading current scene to test data persistence")

    -- 임시: 현재 씬 재로드 (실제로는 다른 씬으로 전환)
    -- TransitionToLevel("Data/Scenes/YourCurrentScene.scene")
end

-- ════════════════════════════════════════════════════════════════════════
-- 종료
-- ════════════════════════════════════════════════════════════════════════
function OnEndPlay()
    print("[Scene1] Collection Stage - End Play")
end
