-- SafeSpawner.lua
-- 물리 조건(빈 공간, 바닥, 천장)을 확인하여 안전한 위치에 프리팹을 스폰하는 스크립트
--
-- 조건:
-- 1) 스폰 위치에서 ObjectHalfExtent 크기만큼 공간이 비어있어야 함
-- 2) 아래에 바닥이 있되, ExcludeFloorTag와 다른 태그여야 함
-- 3) 위에 천장이 있어야 함

-- ============================================================================
-- 스폰 설정
-- ============================================================================

-- 스폰 영역 (이 오브젝트 위치 기준 오프셋)
local AreaOffsetMin = Vector(-39, -20, -11.5)   -- 영역 최소 오프셋
local AreaOffsetMax = Vector(39, 20, 11.5)    -- 영역 최대 오프셋

-- 스폰할 오브젝트 크기 (반크기) - 충돌 체크용
local ObjectHalfExtent = Vector(2,2,2)

-- 물리 조건 설정
local ExcludeFloorTag = "Ground"       -- 이 태그의 바닥에는 스폰 안함 (빈 문자열이면 체크 안함)
local MaxFloorDistance = 20.0        -- 바닥 탐지 최대 거리
local MaxCeilingDistance = 20.0      -- 천장 탐지 최대 거리
local RequireCeiling = true          -- 천장 필수 여부
local MinSpawnDistance = 5.0         -- 스폰된 오브젝트 간 최소 거리

-- 프리팹 목록과 각각의 설정
-- path: 프리팹 경로
-- maxCount: 이 프리팹의 최대 스폰 개수
-- maxAttempts: 이 프리팹의 최대 스폰 시도 횟수
-- floorOffsetRatio: 바닥 오프셋 비율 (0.0=바닥, 1.0=HalfExtent.Z만큼 위) [기본값: 0.0]
-- rotationOffset: 회전 오프셋 Vector(Pitch, Yaw, Roll) (도 단위) [기본값: Vector(0,0,0)]
local PrefabList = {
    {
        path = "Data/Prefabs/David.prefab",
        maxCount = 2,
        maxAttempts = 100,
        floorOffsetRatio = 0.0,
        rotationOffset = Vector(0, 0, 0)
    },
    {
        path = "Data/Prefabs/Elizabeth.prefab",
        maxCount = 2,
        maxAttempts = 100,
        floorOffsetRatio = 0.0,
        rotationOffset = Vector(0, 0, 0)
    },
    {
        path = "Data/Prefabs/Lewis.prefab",
        maxCount = 2,
        maxAttempts = 100,
        floorOffsetRatio = 0.0,
        rotationOffset = Vector(0, 0, 0)
    },
    {
        path = "Data/Prefabs/Sophie.prefab",
        maxCount = 2,
        maxAttempts = 100,
        floorOffsetRatio = 0.0,
        rotationOffset = Vector(0, 0, 0)
    },
    {
        path = "Data/Prefabs/Suzie.prefab",
        maxCount = 2,
        maxAttempts = 100,
        floorOffsetRatio = 0.0,
        rotationOffset = Vector(0, 0, 0)
    },
    {
        path = "Data/Prefabs/Fire.prefab",
        maxCount = 50,
        maxAttempts = 1000,
        floorOffsetRatio = 0.2,
        rotationOffset = Vector(0, 0, 0)
    }
}

-- ============================================================================
-- 내부 변수
-- ============================================================================

local SpawnedObjects = {}            -- 스폰된 오브젝트들
local SpawnedPositions = {}          -- 스폰된 위치들 (최소 거리 체크용)
local bSpawnPending = false          -- 스폰 대기 중 플래그
local SpawnDelayTimer = 0            -- 스폰 지연 타이머

-- ============================================================================
-- 유틸리티 함수
-- ============================================================================

local function RandomInRange(minVal, maxVal)
    return minVal + (maxVal - minVal) * math.random()
end

-- 위치가 기존 스폰된 오브젝트들과 최소 거리를 유지하는지 체크
local function CheckMinDistance(position)
    if MinSpawnDistance <= 0 then
        return true
    end

    local minDistSq = MinSpawnDistance * MinSpawnDistance

    for _, spawnedPos in ipairs(SpawnedPositions) do
        local dx = position.X - spawnedPos.X
        local dy = position.Y - spawnedPos.Y
        local dz = position.Z - spawnedPos.Z
        local distSq = dx * dx + dy * dy + dz * dz

        if distSq < minDistSq then
            return false
        end
    end

    return true
end


-- ============================================================================
-- 물리 조건 체크 함수
-- ============================================================================

local function CheckSpawnConditions(position, halfExtent)
    local result = {
        bValid = false,
        FailReason = nil,
        FloorLocation = nil,
        FloorTag = nil,
        CeilingLocation = nil,
        CeilingTag = nil
    }

    -- 디버그: Physics 테이블 확인
    if not Physics then
        print("[SafeSpawner][ERROR] Physics is nil!")
        result.FailReason = "PhysicsNil"
        return result
    end

    if not Physics.OverlapBox then
        print("[SafeSpawner][ERROR] Physics.OverlapBox is nil!")
        result.FailReason = "OverlapBoxNil"
        return result
    end

    if not Physics.Raycast then
        print("[SafeSpawner][ERROR] Physics.Raycast is nil!")
        result.FailReason = "RaycastNil"
        return result
    end

    -- 1. 공간이 비어있는지 확인
    local overlapResult = Physics.OverlapBox(position, halfExtent)
    print("[SafeSpawner][DEBUG] OverlapBox at (" ..
          string.format("%.2f, %.2f, %.2f", position.X, position.Y, position.Z) ..
          ") = " .. tostring(overlapResult))

    if overlapResult then
        result.FailReason = "SpaceOccupied"
        return result
    end

    -- 1.5. 벽 체크 (8개 꼭짓점에서 중심으로 레이캐스트)
    -- PhysX는 Triangle Mesh에 대해 overlap을 제대로 지원하지 않으므로 raycast로 대체
    -- 각 꼭짓점에서 중심으로 ray를 쏴서 중간에 벽이 있으면 박스가 벽을 관통하는 것
    local corners = {
        Vector(position.X - halfExtent.X, position.Y - halfExtent.Y, position.Z - halfExtent.Z),
        Vector(position.X - halfExtent.X, position.Y - halfExtent.Y, position.Z + halfExtent.Z),
        Vector(position.X - halfExtent.X, position.Y + halfExtent.Y, position.Z - halfExtent.Z),
        Vector(position.X - halfExtent.X, position.Y + halfExtent.Y, position.Z + halfExtent.Z),
        Vector(position.X + halfExtent.X, position.Y - halfExtent.Y, position.Z - halfExtent.Z),
        Vector(position.X + halfExtent.X, position.Y - halfExtent.Y, position.Z + halfExtent.Z),
        Vector(position.X + halfExtent.X, position.Y + halfExtent.Y, position.Z - halfExtent.Z),
        Vector(position.X + halfExtent.X, position.Y + halfExtent.Y, position.Z + halfExtent.Z)
    }

    for i, corner in ipairs(corners) do
        -- 꼭짓점에서 중심으로의 방향과 거리 계산
        local toCenter = Vector(position.X - corner.X, position.Y - corner.Y, position.Z - corner.Z)
        local dist = math.sqrt(toCenter.X * toCenter.X + toCenter.Y * toCenter.Y + toCenter.Z * toCenter.Z)

        if dist > 0.01 then
            local dir = Vector(toCenter.X / dist, toCenter.Y / dist, toCenter.Z / dist)

            -- 꼭짓점에서 중심 방향으로 raycast
            local hit = Physics.Raycast(corner, dir, dist - 0.01)  -- 중심 직전까지만

            if hit.bHit then
                -- 꼭짓점과 중심 사이에 벽이 있음 = 박스가 벽을 관통
                result.FailReason = "WallIntersect"
                return result
            end
        end
    end

    -- 2. 바닥 확인 (아래로 레이캐스트) - Z축이 Up/Down
    local floorCheckStart = Vector(position.X, position.Y, position.Z - halfExtent.Z)
    print("[SafeSpawner][DEBUG] Raycast floor from (" ..
          string.format("%.2f, %.2f, %.2f", floorCheckStart.X, floorCheckStart.Y, floorCheckStart.Z) .. ")")

    local floorHit = Physics.Raycast(floorCheckStart, Vector(0, 0, -1), MaxFloorDistance)

    print("[SafeSpawner][DEBUG] FloorHit.bHit = " .. tostring(floorHit.bHit) ..
          ", ActorTag = " .. tostring(floorHit.ActorTag))

    if not floorHit.bHit then
        result.FailReason = "NoFloor"
        return result
    end

    -- 제외할 바닥 태그 체크
    if ExcludeFloorTag and #ExcludeFloorTag > 0 then
        print("[SafeSpawner][DEBUG] Checking ExcludeFloorTag: '" .. ExcludeFloorTag ..
              "' vs ActorTag: '" .. tostring(floorHit.ActorTag) .. "'")
        if floorHit.ActorTag and floorHit.ActorTag == ExcludeFloorTag then
            result.FailReason = "ExcludedFloor"
            result.FloorTag = floorHit.ActorTag
            return result
        end
    end

    result.FloorLocation = floorHit.Location
    result.FloorTag = floorHit.ActorTag or ""

    -- 3. 천장 확인 (위로 레이캐스트) - Z축이 Up/Down
    if RequireCeiling then
        local ceilingCheckStart = Vector(position.X, position.Y, position.Z + halfExtent.Z)
        print("[SafeSpawner][DEBUG] Raycast ceiling from (" ..
              string.format("%.2f, %.2f, %.2f", ceilingCheckStart.X, ceilingCheckStart.Y, ceilingCheckStart.Z) .. ")")

        local ceilingHit = Physics.Raycast(ceilingCheckStart, Vector(0, 0, 1), MaxCeilingDistance)

        print("[SafeSpawner][DEBUG] CeilingHit.bHit = " .. tostring(ceilingHit.bHit))

        if not ceilingHit.bHit then
            result.FailReason = "NoCeiling"
            return result
        end

        result.CeilingLocation = ceilingHit.Location
        result.CeilingTag = ceilingHit.ActorTag or ""
    end

    -- 모든 조건 통과
    print("[SafeSpawner][DEBUG] All conditions passed!")
    result.bValid = true
    return result
end

-- ============================================================================
-- 프리팹 스폰 함수
-- ============================================================================

-- 단일 스폰 시도 (특정 프리팹을 지정된 위치에 스폰)
local function TrySpawnOnce(areaMin, areaMax, halfExtent, prefabData)
    -- 랜덤 위치 생성
    local testPos = Vector(
        RandomInRange(areaMin.X, areaMax.X),
        RandomInRange(areaMin.Y, areaMax.Y),
        RandomInRange(areaMin.Z, areaMax.Z)
    )

    -- 조건 체크
    local checkResult = CheckSpawnConditions(testPos, halfExtent)

    if not checkResult.bValid then
        return nil, checkResult
    end

    -- 최소 거리 체크
    if not CheckMinDistance(testPos) then
        return nil, { bValid = false, FailReason = "TooCloseToOther" }
    end

    -- 프리팹별 설정 (기본값 적용)
    local floorOffsetRatio = prefabData.floorOffsetRatio or 0.0
    local rotationOffset = prefabData.rotationOffset or Vector(0, 0, 0)

    -- 바닥 위로 위치 조정 (Z축이 Up/Down)
    -- floorOffsetRatio: 0.0 = 피벗이 바닥에 위치, 1.0 = 피벗이 HalfExtent.Z만큼 위
    local spawnPos = Vector(
        testPos.X,
        testPos.Y,
        checkResult.FloorLocation.Z + (floorOffsetRatio * halfExtent.Z) + 0.05
    )
    -- 참고: 조정된 위치에서의 OverlapBox 체크는 제거함
    -- (floorOffsetRatio=0일 때 박스 하단이 바닥 아래로 가서 항상 실패하기 때문)

    -- 프리팹 스폰
    local spawnedObj = SpawnPrefab(prefabData.path)
    if spawnedObj then
        spawnedObj.Location = spawnPos
        -- 랜덤 Z 회전 + rotationOffset 적용
        local randomYaw = math.random() * 360.0
        spawnedObj.Rotation = Vector(
            rotationOffset.X,
            rotationOffset.Y,
            rotationOffset.Z + randomYaw
        )
        table.insert(SpawnedObjects, spawnedObj)
        table.insert(SpawnedPositions, spawnPos)  -- 위치 기록
        print("[SafeSpawner] Spawned: " .. prefabData.path ..
              " at (" .. string.format("%.2f", spawnPos.X) .. ", " ..
              string.format("%.2f", spawnPos.Y) .. ", " ..
              string.format("%.2f", spawnPos.Z) .. ")" ..
              " FloorTag: " .. (checkResult.FloorTag or "none"))
        return spawnedObj, checkResult
    end

    return nil, { bValid = false, FailReason = "SpawnFailed" }
end

-- 영역 내 각 프리팹별로 최대 개수까지 스폰 시도
local function SpawnMultiple()
    -- 스폰 영역 계산 (현재 위치 기준)
    local basePos = Obj.Location
    local areaMin = Vector(
        basePos.X + AreaOffsetMin.X,
        basePos.Y + AreaOffsetMin.Y,
        basePos.Z + AreaOffsetMin.Z
    )
    local areaMax = Vector(
        basePos.X + AreaOffsetMax.X,
        basePos.Y + AreaOffsetMax.Y,
        basePos.Z + AreaOffsetMax.Z
    )

    local totalSpawned = 0
    local personSpawned = 0  -- 사람 프리팹 카운트 (Fire 제외)

    -- 각 프리팹별로 개별 스폰
    for _, prefabData in ipairs(PrefabList) do
        local maxCount = prefabData.maxCount or 10
        local maxAttempts = prefabData.maxAttempts or 100
        local spawnedCount = 0

        -- 사람 프리팹인지 확인 (Fire가 아닌 것)
        local isPerson = not string.find(prefabData.path, "Fire")

        for attempt = 1, maxAttempts do
            if spawnedCount >= maxCount then
                break
            end

            local obj, result = TrySpawnOnce(areaMin, areaMax, ObjectHalfExtent, prefabData)
            if obj then
                spawnedCount = spawnedCount + 1
                totalSpawned = totalSpawned + 1
                if isPerson then
                    personSpawned = personSpawned + 1
                end
            end
        end

        print("[SafeSpawner] " .. prefabData.path .. ": " .. spawnedCount .. "/" .. maxCount ..
              " (attempts: " .. maxAttempts .. ")")
    end

    print("[SafeSpawner] Total spawned: " .. totalSpawned .. ", Persons: " .. personSpawned)

    -- GameMode에 TotalPersonCount 설정
    local rescueGameMode = GetGameModeAs("ARescueGameMode")
    if rescueGameMode then
        rescueGameMode.TotalPersonCount = personSpawned
        print("[SafeSpawner] Set GameMode.TotalPersonCount = " .. personSpawned)
    else
        print("[SafeSpawner] WARNING: Could not set TotalPersonCount - RescueGameMode not found")
    end

    -- GameInstance에도 TotalPersonCount 저장 (Ending에서 사용)
    local gi = GetGameInstance()
    if gi then
        gi:SetTotalPersonCount(personSpawned)
        print("[SafeSpawner] Set GameInstance.TotalPersonCount = " .. personSpawned)
    end

    return totalSpawned
end

-- ============================================================================
-- 이벤트 핸들러
-- ============================================================================

function OnBeginPlay()
    print("[SafeSpawner] BeginPlay called!")

    -- 랜덤 시드 초기화 (위치 기반으로 각 스포너마다 다른 시드 사용)
    local pos = Obj.Location
    print("[SafeSpawner] Obj.Location = " ..
          string.format("%.2f, %.2f, %.2f", pos.X, pos.Y, pos.Z))

    local seed = os.time() + math.floor((pos.X + pos.Y + pos.Z) * 1000)
    math.randomseed(seed)

    -- Lua의 math.random()은 시드 직후 편향된 값을 반환하므로 몇 번 버림
    math.random(); math.random(); math.random()

    -- 물리 씬이 완전히 초기화될 때까지 스폰 지연
    bSpawnPending = true
    SpawnDelayTimer = 0.1  -- 0.1초 후 스폰 시도

    print("[SafeSpawner] BeginPlay finished! Spawn will be attempted after delay.")
end

function OnEndPlay()
    SpawnedObjects = {}
    SpawnedPositions = {}
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Update(dt)
    -- 스폰 대기 중이면 타이머 감소 후 스폰 시도
    if bSpawnPending then
        SpawnDelayTimer = SpawnDelayTimer - dt
        if SpawnDelayTimer <= 0 then
            bSpawnPending = false

            -- 테스트: 현재 위치에서 바로 아래로 레이캐스트
            local pos = Obj.Location
            local testHit = Physics.Raycast(pos, Vector(0, 0, -1), 100.0)
            print("[SafeSpawner][TEST] Delayed raycast from Obj.Location down: bHit = " .. tostring(testHit.bHit))
            if testHit.bHit then
                print("[SafeSpawner][TEST] Hit at Z = " .. string.format("%.2f", testHit.Location.Z) ..
                      ", ActorTag = " .. tostring(testHit.ActorTag))
            end

            print("[SafeSpawner] Calling SpawnMultiple after delay...")
            SpawnMultiple()
            print("[SafeSpawner] SpawnMultiple finished!")
        end
    end
end
