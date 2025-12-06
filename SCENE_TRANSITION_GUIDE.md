# 씬 전환 시스템 사용 가이드 (간소화 버전)

## 개요
PIE 실행 중 씬을 전환할 수 있는 런타임 전환 시스템입니다.
- **PIE를 멈추지 않음** - GameInstance만 PIE 종료까지 유지됨
- **프레임 경계에서 안전하게 전환** - 크래시 없음
- **각 씬마다 LevelTransitionManager 배치 필요** - 씬별 독립 관리

---

## 핵심 구조 (간소화)

### 1. LevelTransitionManager (씬마다 배치)
- **각 씬에 직접 배치해야 함** - Persistent 개념 제거
- 씬 전환 시 삭제되고, 새 씬의 Manager가 생성됨
- 다음 씬 경로를 저장하고 관리

### 2. GameInstance (PIE 세션 공유)
- EditorEngine이 소유
- PIE 종료 시까지 유지됨
- 씬 간 데이터 공유 (아이템, 점수, 플레이어 상태 등)

### 3. 모든 액터는 씬 전환 시 삭제됨
- Persistent Actor 개념 완전 제거
- 씬 전환 = 이전 씬의 모든 액터 삭제 + 새 씬 로드

---

## Lua API

### 1. SetNextLevel(경로)
현재 씬에서 전환할 다음 씬 경로를 설정합니다.
보통 씬의 BeginPlay나 초기화 스크립트에서 호출합니다.

```lua
-- Level1.lua (BeginPlay에서 호출)
function OnBeginPlay()
    SetNextLevel("Data/Scenes/Level2.scene")
    print("Next level set to Level2")
end
```

### 2. TransitionToNextLevel()
설정된 다음 씬으로 전환합니다.
조건 충족 시 (예: 트리거, 아이템 수집 완료) 호출합니다.

```lua
-- 트리거 충돌 시 다음 씬으로 이동
function OnTriggerEnter(other)
    if other:GetTag() == "Player" then
        print("Player reached exit. Transitioning to next level...")
        TransitionToNextLevel()
    end
end
```

### 3. TransitionToLevel(경로)
지정된 경로의 씬으로 직접 전환합니다.
조건부 분기가 필요한 경우 사용합니다.

```lua
-- 예: 플레이어 선택에 따라 다른 씬으로 이동
function OnPlayerChoice(choice)
    if choice == "A" then
        TransitionToLevel("Data/Scenes/PathA.scene")
    else
        TransitionToLevel("Data/Scenes/PathB.scene")
    end
end
```

---

## GameInstance API (씬 간 데이터 공유)

### 아이템 관리
```lua
local gameInstance = GetGameInstance()

-- 아이템 추가
gameInstance:AddItem("Key_Red", 1)
gameInstance:AddItem("Coin", 10)

-- 아이템 조회
local keyCount = gameInstance:GetItemCount("Key_Red")
print("Red keys:", keyCount)

-- 아이템 사용
if gameInstance:RemoveItem("Key_Red", 1) then
    print("Door unlocked!")
end
```

### 플레이어 상태 관리
```lua
local gameInstance = GetGameInstance()

-- 상태 저장
gameInstance:SetPlayerHealth(75)
gameInstance:SetPlayerScore(1000)
gameInstance:SetRescuedCount(3)

-- 상태 조회
local health = gameInstance:GetPlayerHealth()
local score = gameInstance:GetPlayerScore()
```

### 범용 데이터 저장
```lua
local gameInstance = GetGameInstance()

-- 정수
gameInstance:SetInt("EnemiesKilled", 42)
local kills = gameInstance:GetInt("EnemiesKilled", 0)

-- 실수
gameInstance:SetFloat("PlayTime", 123.45)
local time = gameInstance:GetFloat("PlayTime", 0.0)

-- 문자열
gameInstance:SetString("PlayerName", "Hero")
local name = gameInstance:GetString("PlayerName", "Unknown")

-- 불리언
gameInstance:SetBool("HasSeenTutorial", true)
local seen = gameInstance:GetBool("HasSeenTutorial", false)
```

---

## 6개 씬 연결 예시

### 중요: 각 씬마다 LevelTransitionManager 배치!
- TEST.scene → LevelTransitionManager 배치 + Lua 스크립트 부착
- TEST2.scene → LevelTransitionManager 배치 + Lua 스크립트 부착
- Level1.scene → LevelTransitionManager 배치 + Lua 스크립트 부착
- Level2.scene → LevelTransitionManager 배치 + Lua 스크립트 부착
- ... (모든 씬에 배치)

### Level1 - LevelTransitionManager의 Lua 스크립트
```lua
function OnBeginPlay()
    -- 다음 씬 설정
    SetNextLevel("Data/Scenes/Level2.scene")

    -- 초기 데이터 설정 (첫 씬에서만)
    local gi = GetGameInstance()
    gi:SetPlayerHealth(100)
    gi:SetPlayerScore(0)
    gi:ClearItems()

    print("Level 1 started. Collect the key to proceed.")
end
```

### Level2 - LevelTransitionManager의 Lua 스크립트
```lua
function OnBeginPlay()
    SetNextLevel("Data/Scenes/Level3.scene")

    local gi = GetGameInstance()
    print("Level 2 started. Health:", gi:GetPlayerHealth())
    print("Items from Level 1:", gi:GetItemCount("Key_Level1"))
end
```

### Level3 ~ Level6 - 각각의 LevelTransitionManager Lua 스크립트
```lua
-- Level3
function OnBeginPlay()
    SetNextLevel("Data/Scenes/Level4.scene")
end

-- Level4
function OnBeginPlay()
    SetNextLevel("Data/Scenes/Level5.scene")
end

-- Level5
function OnBeginPlay()
    SetNextLevel("Data/Scenes/Level6.scene")
end

-- Level6 (마지막 씬)
function OnBeginPlay()
    local gi = GetGameInstance()
    print("=== Game Complete ===")
    print("Final Score:", gi:GetPlayerScore())
    print("Total Rescued:", gi:GetRescuedCount())
end
```

### 트리거로 전환하기 (다른 액터의 Lua 스크립트)
```lua
-- 출구 트리거 액터의 스크립트
function OnTriggerEnter(other)
    if other:GetTag() == "Player" then
        print("Player reached exit!")
        TransitionToNextLevel()  -- LevelTransitionManager를 찾아서 전환
    end
end
```

---

## 주의사항

### ✅ 안전한 사용법
1. **각 씬의 BeginPlay에서 SetNextLevel 호출** - 씬 로드 시 자동 설정
2. **조건 충족 시 TransitionToNextLevel 호출** - 간단하고 안전
3. **GameInstance로 데이터 공유** - 씬 전환 후에도 유지됨

### ❌ 피해야 할 패턴
1. **같은 프레임에 여러 번 전환 시도** - 첫 번째 호출만 처리됨
2. **전환 중에 다시 전환 호출** - 무시됨 (경고 로그)
3. **존재하지 않는 씬 경로** - 전환 실패 (에러 로그)

---

## 테스트 방법

### 1. 기본 테스트 (L 키)
- PIE 실행 중 **L 키**를 누르면 다음 씬으로 전환됨 (T 키는 UV Scroll과 충돌)
- LevelTransitionManager.cpp:38에 하드코딩됨
- 실제 게임에서는 Lua에서 조건에 따라 호출

### 2. Lua 스크립트 테스트
```lua
-- TEST1.scene의 Lua 스크립트
function OnBeginPlay()
    SetNextLevel("Data/Scenes/TEST2.scene")
    print("Press L to transition to TEST2")
end

-- TEST2.scene의 Lua 스크립트
function OnBeginPlay()
    SetNextLevel("Data/Scenes/TEST1.scene")
    print("Press L to transition back to TEST1")

    -- GameInstance 데이터 확인
    local gi = GetGameInstance()
    print("Transition count:", gi:GetInt("TransitionCount", 0))
    gi:SetInt("TransitionCount", gi:GetInt("TransitionCount", 0) + 1)
end
```

---

## 문제 해결

### 크래시 발생 시
1. **로그 확인**: `[info] LevelTransitionManager:` 로그 찾기
2. **씬 파일 경로 확인**: 파일이 존재하는지 체크
3. **PIE 모드 확인**: 에디터 모드에서는 전환 불가

### GameInstance 데이터 손실 시
- **PIE를 종료하지 않았는지 확인**: EndPIE 시 GameInstance 삭제됨
- **전환 중 크래시**: 이전 방식(PIE 재시작)의 잔재일 가능성

---

## 구현 세부사항

### 안전성 보장
1. **프레임 경계 전환**: Tick 초반에 bPendingTransition 처리
2. **Persistent Actor 보존**: World::SetLevel()에서 자동 처리
3. **물리/충돌 안전**: Tick 후반의 PhysScene 업데이트 전에 전환 완료

### 전환 흐름
```
[현재 프레임 Tick 완료]
↓
[다음 프레임 Tick 시작]
↓
LevelTransitionManager::Tick()
  ↓ bPendingTransition = true?
  ↓ YES
ExecuteTransition()
  ↓
  1. PIE World 찾기
  2. LoadLevelFromFile() - 현재 레벨 정리, 새 레벨 로드
  3. BeginPlay() - 새 액터들만 초기화
  ↓
[일반 액터들 Tick 진행]
```

### 메모리 관리
- **LevelTransitionManager**: PIE 종료 시까지 유지
- **GameInstance**: PIE 종료 시까지 유지
- **일반 액터**: 씬 전환 시 삭제됨
- **Persistent Actor**: 씬 전환 후에도 유지됨
