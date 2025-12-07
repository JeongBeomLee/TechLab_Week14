-- ============================================================================
-- VehicleMover.lua
-- StaticMeshActor를 직선으로 이동시키는 스크립트
-- ============================================================================

-- ============================================================================
-- 상태를 전역 테이블에 저장 (핫 리로드 시 보존됨)
-- ============================================================================
State = State or {}

-- ============================================================================
-- 설정 (필요시 수정 가능)
-- ============================================================================
local MOVE_SPEED = 20.0  -- 이동 속도 (m/s)
local MOVE_DIRECTION = Vector(0, -1, 0)  -- 이동 방향 (X축 음의 방향)
local START_DELAY = 0.0  -- 시작 지연 시간 (초)
local MAX_DISTANCE = 0  -- 최대 이동 거리 (0 = 무제한, 양수 = 제한)

-- ============================================================================
-- 참조 초기화 함수
-- ============================================================================
local function InitReferences()
    State.Actor = GetOwnerAs(Obj, "AActor")
end

function OnBeginPlay()

    InitReferences()

    if not State.Actor then
        return
    end

    -- 초기 상태
    State.bIsMoving = false
    State.ElapsedTime = 0
    State.StartPosition = nil

    -- GameInstance에서 시작 플래그 확인
    local gi = GetGameInstance()
    if gi then
        -- 자동 시작하지 않음 (GameMode에서 플래그로 제어)
    end

end

function OnHotReload()
    InitReferences()
end

-- ============================================================================
-- Update 함수
-- ============================================================================
function Update(DeltaTime)
    if not State.Actor then
        return
    end

    State.ElapsedTime = State.ElapsedTime + DeltaTime

    -- GameInstance에서 이동 시작 플래그 확인
    local gi = GetGameInstance()
    if gi and not State.bIsMoving then
        if gi:GetBool("Vehicle_StartMove", false) then
            gi:SetBool("Vehicle_StartMove", false)
            State.bIsMoving = true
            State.StartPosition = Obj.Location
        end
    end

    -- 이동 중인 경우
    if State.bIsMoving and Obj and Obj.Location then
        -- 시작 지연 체크
        if State.ElapsedTime < START_DELAY then
            return
        end

        -- 최대 이동 거리 체크
        if MAX_DISTANCE > 0 and State.StartPosition then
            local currentLoc = Obj.Location
            local dx = currentLoc.X - State.StartPosition.X
            local dy = currentLoc.Y - State.StartPosition.Y
            local dz = currentLoc.Z - State.StartPosition.Z
            local distanceMoved = math.sqrt(dx * dx + dy * dy + dz * dz)

            if distanceMoved >= MAX_DISTANCE then
                State.bIsMoving = false
                return
            end
        end

        -- 현재 위치 가져오기
        local currentLoc = Obj.Location

        -- 새로운 위치 계산 (방향 * 속도 * 시간)
        local offset = Vector(
            MOVE_DIRECTION.X * MOVE_SPEED * DeltaTime,
            MOVE_DIRECTION.Y * MOVE_SPEED * DeltaTime,
            MOVE_DIRECTION.Z * MOVE_SPEED * DeltaTime
        )

        local newLoc = Vector(
            currentLoc.X + offset.X,
            currentLoc.Y + offset.Y,
            currentLoc.Z + offset.Z
        )

        -- 위치 업데이트
        Obj.Location = newLoc

        -- 디버그: 일정 간격으로 위치 출력
        if math.floor(State.ElapsedTime * 2) ~= math.floor((State.ElapsedTime - DeltaTime) * 2) then
        end
    end
end

function OnEndPlay()
    State.Actor = nil
end
