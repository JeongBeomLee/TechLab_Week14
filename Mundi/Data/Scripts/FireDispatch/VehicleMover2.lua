-- ============================================================================
-- VehicleMover2.lua
-- StaticMeshActor를 직선으로 이동시키는 스크립트 (두 번째 차량용)
-- ============================================================================

-- ============================================================================
-- 상태를 전역 테이블에 저장 (핫 리로드 시 보존됨)
-- 각 스크립트마다 고유한 State 테이블 사용
-- ============================================================================
State2 = State2 or {}

-- ============================================================================
-- 설정 (필요시 수정 가능)
-- ============================================================================
local MOVE_SPEED = 30.0  -- 이동 속도 (m/s)
local MOVE_DIRECTION = Vector(0, -1, 0)  -- 이동 방향 (Y축 음의 방향)
local START_DELAY = 0.0  -- 시작 지연 시간 (초)
local MAX_DISTANCE = 0  -- 최대 이동 거리 (0 = 무제한, 양수 = 제한)
local ACCELERATION_TIME = 1.0  -- 가속 시간 (초)

-- ============================================================================
-- 참조 초기화 함수
-- ============================================================================
local function InitReferences()
    State2.Actor = GetOwnerAs(Obj, "AActor")
end

function OnBeginPlay()

    InitReferences()

    if not State2.Actor then
        return
    end

    -- 초기 상태
    State2.bIsMoving = false
    State2.ElapsedTime = 0
    State2.StartPosition = nil
    State2.AccelerationTimer = 0.0

    print("[VehicleMover2] Initialized - Waiting for engine start signal")
end

function OnHotReload()
    InitReferences()
end

-- ============================================================================
-- Update 함수
-- ============================================================================
function Update(DeltaTime)
    if not State2.Actor then
        return
    end

    State2.ElapsedTime = State2.ElapsedTime + DeltaTime

    -- GameInstance에서 시동 완료 플래그 확인
    local gi = GetGameInstance()
    if gi and not State2.bIsMoving then
        if gi:GetBool("FireDispatch_EngineStarted", false) then
            print("[VehicleMover2] Engine started! Vehicle starting to move forward")
            State2.bIsMoving = true
            State2.StartPosition = Obj.Location
            State2.AccelerationTimer = 0.0
        end
    end

    -- 이동 중인 경우
    if State2.bIsMoving and Obj and Obj.Location then
        -- 시작 지연 체크
        if State2.ElapsedTime < START_DELAY then
            return
        end

        -- 최대 이동 거리 체크
        if MAX_DISTANCE > 0 and State2.StartPosition then
            local currentLoc = Obj.Location
            local dx = currentLoc.X - State2.StartPosition.X
            local dy = currentLoc.Y - State2.StartPosition.Y
            local dz = currentLoc.Z - State2.StartPosition.Z
            local distanceMoved = math.sqrt(dx * dx + dy * dy + dz * dz)

            if distanceMoved >= MAX_DISTANCE then
                State2.bIsMoving = false
                return
            end
        end

        -- 가속 처리
        State2.AccelerationTimer = State2.AccelerationTimer + DeltaTime
        local speedMultiplier = math.min(State2.AccelerationTimer / ACCELERATION_TIME, 1.0)

        -- 현재 위치 가져오기
        local currentLoc = Obj.Location

        -- 새로운 위치 계산 (방향 * 속도 * 시간 * 가속)
        local currentSpeed = MOVE_SPEED * speedMultiplier
        local offset = Vector(
            MOVE_DIRECTION.X * currentSpeed * DeltaTime,
            MOVE_DIRECTION.Y * currentSpeed * DeltaTime,
            MOVE_DIRECTION.Z * currentSpeed * DeltaTime
        )

        local newLoc = Vector(
            currentLoc.X + offset.X,
            currentLoc.Y + offset.Y,
            currentLoc.Z + offset.Z
        )

        -- 위치 업데이트
        Obj.Location = newLoc

        -- 디버그: 일정 간격으로 위치 출력
        if math.floor(State2.ElapsedTime * 2) ~= math.floor((State2.ElapsedTime - DeltaTime) * 2) then
            print(string.format("[VehicleMover2] Moving - Speed: %.1f%%, Pos: (%.2f, %.2f, %.2f)",
                speedMultiplier * 100, newLoc.X, newLoc.Y, newLoc.Z))
        end
    end
end

function OnEndPlay()
    State2.Actor = nil
end
