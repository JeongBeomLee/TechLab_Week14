-- ============================================================================
-- FirefighterController.lua
-- Firefighter 캐릭터 애니메이션 컨트롤러: WASD 입력으로 Idle/Walking/Running 전환
-- ============================================================================

-- ============================================================================
-- 상태를 전역 테이블에 저장 (핫 리로드 시 보존됨)
-- ============================================================================
State = State or {}

-- 이동 속도 설정 (상수는 local로 유지)
local WALK_SPEED = 6.0    -- 기본 걷기 속도
local RUN_SPEED = 8.0     -- 달리기 속도

-- 소화 게이지 설정
local EXTINGUISH_DRAIN_RATE = 10.0  -- 초당 소화 게이지 소모량
local WATER_MAGIC_DAMAGE_RATE = 0.5  -- 초당 불에 가하는 데미지 (0.5 = 2초에 불 50% 약화)

-- 애니메이션 상태 이름
local STATE_IDLE = "Idle"
local STATE_WALKING = "Walking"
local STATE_RUNNING = "Running"
local STATE_IDLE_WITH_OBJECT = "IdleWithObject"
local STATE_WALKING_WITH_OBJECT = "WalkingWithObject"
local STATE_RUNNING_WITH_OBJECT = "RunningWithObject"
local STATE_PICKUP = "PickUp"
local STATE_WATERMAGIC_START = "WaterMagic_Start"
local STATE_WATERMAGIC_PROGRESS = "WaterMagic_Progress"
local STATE_WATERMAGIC_END = "WaterMagic_End"

-- 애니메이션 에셋 경로 (Without_Cloth 버전)
local ANIM_WITHOUT_CLOTH = {
    IDLE = "Data/firefighter/Firefighter_Without_Cloth_Idle_mixamo.com",
    IDLE_WITH_OBJECT = "Data/firefighter/Firefighter_Without_Cloth_Idle_With_Object_mixamo.com",
    WALKING = "Data/firefighter/Firefighter_Without_Cloth_Walking_mixamo.com",
    WALKING_WITH_OBJECT = "Data/firefighter/Firefighter_Without_Cloth_Walking_With_Object_mixamo.com",
    RUNNING = "Data/firefighter/Firefighter_Without_Cloth_Running_mixamo.com",
    RUNNING_WITH_OBJECT = "Data/firefighter/Firefighter_Without_Cloth_Running_With_Object_mixamo.com",
    PICKUP = "Data/firefighter/Firefighter_Without_Cloth_Picking_Up_Object_mixamo.com",
    WATERMAGIC_START = "Data/firefighter/Firefighter_Without_Cloth_WaterMagic_Start_mixamo.com",
    WATERMAGIC_PROGRESS = "Data/firefighter/Firefighter_Without_Cloth_WaterMagic_Progress_mixamo.com",
    WATERMAGIC_END = "Data/firefighter/Firefighter_Without_Cloth_WaterMagic_End_mixamo.com"
}

-- 애니메이션 에셋 경로 (With_Cloth 버전 - 소방복 장착 시)
local ANIM_WITH_CLOTH = {
    IDLE = "Data/firefighter/Firefighter_With_Cloth_Idle_mixamo.com",
    IDLE_WITH_OBJECT = "Data/firefighter/Firefighter_With_Cloth_Idle_With_Object_mixamo.com",
    WALKING = "Data/firefighter/Firefighter_With_Cloth_Walking_mixamo.com",
    WALKING_WITH_OBJECT = "Data/firefighter/Firefighter_With_Cloth_Walking_With_Object_mixamo.com",
    RUNNING = "Data/firefighter/Firefighter_With_Cloth_Running_mixamo.com",
    RUNNING_WITH_OBJECT = "Data/firefighter/Firefighter_With_Cloth_Running_With_Object_mixamo.com",
    PICKUP = "Data/firefighter/Firefighter_With_Cloth_Picking_Up_Object_mixamo.com",
    WATERMAGIC_START = "Data/firefighter/Firefighter_With_Cloth_WaterMagic_Start_mixamo.com",
    WATERMAGIC_PROGRESS = "Data/firefighter/Firefighter_With_Cloth_WaterMagic_Progress_mixamo.com",
    WATERMAGIC_END = "Data/firefighter/Firefighter_With_Cloth_WaterMagic_End_mixamo.com"
}

-- 메시 경로
local MESH_WITHOUT_CLOTH = "Data/firefighter/Firefighter_Without_Cloth.fbx"
local MESH_WITH_CLOTH = "Data/firefighter/Firefighter_With_Cloth.fbx"

-- 피직스 에셋 경로
local PHYSICS_WITHOUT_CLOTH = "Data/Physics/firefighter_nocloth.physicsasset"
local PHYSICS_WITH_CLOTH = "Data/Physics/firefighter_cloth.physicsasset"

-- 현재 애니메이션 세트 (기본: Without_Cloth)
local CurrentAnims = ANIM_WITHOUT_CLOTH

-- ============================================================================
-- 참조 초기화 함수 (BeginPlay와 HotReload에서 공통 사용)
-- ============================================================================
local function InitReferences()
    State.Character = GetOwnerAs(Obj, "AFirefighterCharacter")
    State.SkeletalMeshComp = GetComponent(Obj, "USkeletalMeshComponent")
    State.MovementComp = GetComponent(Obj, "UCharacterMovementComponent")

    if State.SkeletalMeshComp then
        State.StateMachine = State.SkeletalMeshComp:GetOrCreateStateMachine()
    end
end

function OnBeginPlay()
    print("[FirefighterController] BeginPlay called")

    InitReferences()

    if State.Character then
        print("[FirefighterController] Character found!")
    else
        print("[FirefighterController] ERROR: Could not cast owner to ACharacter")
        return
    end

    if not State.SkeletalMeshComp then
        print("[FirefighterController] ERROR: No USkeletalMeshComponent found")
        return
    end

    if State.MovementComp then
        print("[FirefighterController] MovementComponent found!")
        State.MovementComp.JumpZVelocity = 5.0
    end

    -- 상태 초기화
    State.bIsMoving = false
    State.bIsRunning = false
    State.bIsPerformingAction = false
    State.bHasFireSuit = false  -- 소방복 장착 상태

    -- 물 마법 상태 초기화
    State.bIsUsingWaterMagic = false      -- 물 마법 사용 중 여부
    State.bWaterMagicStarted = false      -- WaterMagic_Start 애니메이션 완료 여부
    State.bWaterMagicEnding = false       -- WaterMagic_End 애니메이션 재생 중 여부

    -- 게임패드 상태 초기화
    State.bWasRTPressed = false           -- RT 트리거 이전 프레임 상태 (Press 감지용)

    -- 애니메이션 상태 머신 설정
    SetupAnimationStateMachine()
end

-- ============================================================================
-- 핫 리로드 콜백 (스크립트 파일 변경 시 호출됨)
-- ============================================================================
function OnHotReload()
    print("[FirefighterController] === HOT RELOAD ===")
    InitReferences()
    --SetupAnimationStateMachine()
    print("[FirefighterController] References re-initialized")
end

function SetupAnimationStateMachine()
    print("[FirefighterController] Setting up Animation State Machine")

    -- 상태 머신 사용 활성화 및 인스턴스 가져오기
    State.SkeletalMeshComp:UseStateMachine()
    State.StateMachine = State.SkeletalMeshComp:GetOrCreateStateMachine()

    if not State.StateMachine then
        print("[FirefighterController] ERROR: Failed to create state machine")
        return
    end

    -- 기존 상태들 클리어 (메시 변경 후 재설정 시 필요)
    State.StateMachine:Clear()

    -- 상태 추가 (이름, 에셋 경로, 재생 속도, 루핑 여부)
    local idleIdx = State.StateMachine:AddState(STATE_IDLE, CurrentAnims.IDLE, 1.0, true)
    local walkIdx = State.StateMachine:AddState(STATE_WALKING, CurrentAnims.WALKING, 1.0, true)
    local runIdx = State.StateMachine:AddState(STATE_RUNNING, CurrentAnims.RUNNING, 1.5, true)  -- 달리기 속도 1.5배

    -- WITH_OBJECT 상태 추가 (사람 들고 있을 때)
    local idleObjIdx = State.StateMachine:AddState(STATE_IDLE_WITH_OBJECT, CurrentAnims.IDLE_WITH_OBJECT, 1.0, true)
    local walkObjIdx = State.StateMachine:AddState(STATE_WALKING_WITH_OBJECT, CurrentAnims.WALKING_WITH_OBJECT, 1.0, true)
    local runObjIdx = State.StateMachine:AddState(STATE_RUNNING_WITH_OBJECT, CurrentAnims.RUNNING_WITH_OBJECT, 1.5, true)

    local pickupIdx = State.StateMachine:AddState(STATE_PICKUP, CurrentAnims.PICKUP, 1.0, false)  -- 루핑 안함
    local waterStartIdx = State.StateMachine:AddState(STATE_WATERMAGIC_START, CurrentAnims.WATERMAGIC_START, 1.0, false)  -- 루핑 안함
    local waterProgressIdx = State.StateMachine:AddState(STATE_WATERMAGIC_PROGRESS, CurrentAnims.WATERMAGIC_PROGRESS, 1.0, true)  -- 루핑
    local waterEndIdx = State.StateMachine:AddState(STATE_WATERMAGIC_END, CurrentAnims.WATERMAGIC_END, 1.0, false)  -- 루핑 안함

    print("[FirefighterController] States added: Idle=" .. tostring(idleIdx) .. ", Walking=" .. tostring(walkIdx) .. ", Running=" .. tostring(runIdx) .. ", PickUp=" .. tostring(pickupIdx))
    print("[FirefighterController] WITH_OBJECT states: IdleObj=" .. tostring(idleObjIdx) .. ", WalkObj=" .. tostring(walkObjIdx) .. ", RunObj=" .. tostring(runObjIdx))
    print("[FirefighterController] WaterMagic states: Start=" .. tostring(waterStartIdx) .. ", Progress=" .. tostring(waterProgressIdx) .. ", End=" .. tostring(waterEndIdx))

    -- 전이(Transition) 추가 (BlendTime 0.2초)
    -- Idle <-> Walking
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_WALKING, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALKING, STATE_IDLE, 0.2)

    -- Idle <-> Running
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_RUNNING, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_IDLE, 0.2)

    -- Walking <-> Running
    State.StateMachine:AddTransitionByName(STATE_WALKING, STATE_RUNNING, 0.15)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_WALKING, 0.15)

    -- WITH_OBJECT 상태 간 전이
    State.StateMachine:AddTransitionByName(STATE_IDLE_WITH_OBJECT, STATE_WALKING_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALKING_WITH_OBJECT, STATE_IDLE_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_IDLE_WITH_OBJECT, STATE_RUNNING_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING_WITH_OBJECT, STATE_IDLE_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALKING_WITH_OBJECT, STATE_RUNNING_WITH_OBJECT, 0.15)
    State.StateMachine:AddTransitionByName(STATE_RUNNING_WITH_OBJECT, STATE_WALKING_WITH_OBJECT, 0.15)

    -- 일반 <-> WITH_OBJECT 전이 (사람 들기/내려놓기)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_IDLE_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_IDLE_WITH_OBJECT, STATE_IDLE, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALKING, STATE_WALKING_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALKING_WITH_OBJECT, STATE_WALKING, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_RUNNING_WITH_OBJECT, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING_WITH_OBJECT, STATE_RUNNING, 0.2)

    -- PickUp 전이 (모든 상태에서 PickUp으로, PickUp에서 Idle 또는 IdleWithObject로)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_PICKUP, 0.1)
    State.StateMachine:AddTransitionByName(STATE_WALKING, STATE_PICKUP, 0.1)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_PICKUP, 0.1)
    State.StateMachine:AddTransitionByName(STATE_PICKUP, STATE_IDLE, 0.2)
    State.StateMachine:AddTransitionByName(STATE_PICKUP, STATE_IDLE_WITH_OBJECT, 0.2)

    -- WaterMagic 전이
    -- 모든 기본 상태에서 WaterMagic_Start로
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_WATERMAGIC_START, 0.1)
    State.StateMachine:AddTransitionByName(STATE_WALKING, STATE_WATERMAGIC_START, 0.1)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_WATERMAGIC_START, 0.1)
    -- WaterMagic_Start -> WaterMagic_Progress
    State.StateMachine:AddTransitionByName(STATE_WATERMAGIC_START, STATE_WATERMAGIC_PROGRESS, 0.1)
    -- WaterMagic_Progress -> WaterMagic_End
    State.StateMachine:AddTransitionByName(STATE_WATERMAGIC_PROGRESS, STATE_WATERMAGIC_END, 0.15)
    -- WaterMagic_End -> Idle
    State.StateMachine:AddTransitionByName(STATE_WATERMAGIC_END, STATE_IDLE, 0.2)

    -- 초기 상태: Idle
    State.StateMachine:SetState(STATE_IDLE, 0.0)

    print("[FirefighterController] Animation State Machine setup complete!")
end

-- ============================================================================
-- 소방복 장착 함수 (외부에서 호출 가능)
-- ============================================================================
function EquipFireSuit()
    if State.bHasFireSuit then
        print("[FirefighterController] Already wearing fire suit!")
        return
    end

    print("[FirefighterController] Equipping Fire Suit...")

    -- 현재 상태와 시간 저장 (애니메이션 이어서 재생용)
    local prevStateName = nil
    local prevStateTime = 0
    if State.StateMachine then
        prevStateName = State.StateMachine:GetCurrentStateName()
        if prevStateName and prevStateName ~= "" then
            prevStateTime = State.StateMachine:GetStateTime(prevStateName)
            print("[FirefighterController] Saving state: " .. prevStateName .. " at time " .. prevStateTime)
        end
    end

    State.bHasFireSuit = true
    -- bIsPerformingAction은 유지 (픽업 중이면 계속 픽업 상태)
    CurrentAnims = ANIM_WITH_CLOTH

    -- 메시 변경
    if State.SkeletalMeshComp then
        State.SkeletalMeshComp:SetSkeletalMesh(MESH_WITH_CLOTH)
        print("[FirefighterController] Mesh changed to With_Cloth")

        -- 피직스 에셋 변경
        State.SkeletalMeshComp:SetPhysicsAsset(PHYSICS_WITH_CLOTH)
        print("[FirefighterController] PhysicsAsset changed to With_Cloth")
    end

    -- 참조 재초기화 (메시 변경 후 필요할 수 있음)
    InitReferences()

    -- 상태 머신 재설정 (새 애니메이션 경로로)
    SetupAnimationStateMachine()

    -- 이전 상태와 시간 복원 (애니메이션 이어서 재생)
    if prevStateName and prevStateName ~= "" and State.StateMachine then
        State.StateMachine:SetState(prevStateName, 0)  -- 즉시 전환 (블렌드 없음)
        State.StateMachine:SetStateTime(prevStateName, prevStateTime)
        print("[FirefighterController] Restored state: " .. prevStateName .. " at time " .. prevStateTime)
    end

    -- 파티클 이펙트 재생
    if State.Character and State.Character.PlayFireSuitEquipEffect then
        State.Character:PlayFireSuitEquipEffect()
        print("[FirefighterController] Playing equip particle effect!")
    end

    print("[FirefighterController] Fire Suit equipped!")
end

-- 소방복 장착 여부 확인 함수
function HasFireSuit()
    return State.bHasFireSuit or false
end

function Update(DeltaTime)
    if not State.Character or not State.StateMachine then
        return
    end

    -- 소방복 장착 플래그 체크 (GameInstance에서)
    if not State.bHasFireSuit then
        local gi = GetGameInstance()
        if gi and gi:GetBool("bEquipFireSuit", false) then
            EquipFireSuit()
            gi:SetBool("bEquipFireSuit", false)  -- 플래그 리셋
        end
    end

    -- 현재 프레임 이동 상태 초기화
    State.bIsMoving = false
    State.bIsRunning = false

    -- 물 마법 종료 중이면 대기 (WaterMagic_End 애니메이션 완료 대기)
    if State.bWaterMagicEnding then
        if State.MovementComp then
            State.MovementComp.MaxWalkSpeed = 0
        end
        return
    end

    -- 물 마법 사용 중 (WaterMagic_Progress 재생 중)
    if State.bIsUsingWaterMagic then
        if State.MovementComp then
            State.MovementComp.MaxWalkSpeed = 0
        end
        -- C++ 변수 설정 (회전 허용용)
        State.Character:SetUsingWaterMagic(true)

        -- 오른쪽 마우스 또는 RT 누르고 있고 소화 게이지가 남아있으면 계속 사용
        local bRightMouseDown = Input:IsMouseButtonDown(1)  -- RButton = 1
        local rtValue = Input:GetGamepadRightTrigger()
        local bRTDown = rtValue > 0.5
        local bInputHeld = bRightMouseDown or bRTDown
        local extinguishGauge = State.Character.ExtinguishGauge or 0

        -- 디버그: 매 프레임 게이지 출력 (1초마다)
        State.DebugTimer = (State.DebugTimer or 0) + DeltaTime
        if State.DebugTimer > 1.0 then
            print("[FirefighterController] WaterMagic Progress - Gauge: " .. tostring(extinguishGauge) .. ", InputHeld: " .. tostring(bInputHeld))
            State.DebugTimer = 0
        end

        if bInputHeld and extinguishGauge > 0 then
            -- 소화 게이지 감소 (C++ 함수 호출)
            local drainAmount = EXTINGUISH_DRAIN_RATE * DeltaTime
            --State.Character:DrainExtinguishGauge(drainAmount)

            -- 물 마법 발사 (전방의 불에 데미지)
            local waterDamage = WATER_MAGIC_DAMAGE_RATE * DeltaTime
            State.Character:FireWaterMagic(waterDamage)
        else
            -- 물 마법 종료 조건: 입력 놓음 또는 소화 게이지 0
            print("[FirefighterController] WaterMagic ending (gauge=" .. tostring(extinguishGauge) .. ", inputHeld=" .. tostring(bInputHeld) .. ")")
            State.bIsUsingWaterMagic = false
            State.Character:SetUsingWaterMagic(false)  -- C++ 변수도 리셋
            State.bWaterMagicEnding = true
            State.StateMachine:SetState(STATE_WATERMAGIC_END, 0.15)
            -- 파티클 중지
            if State.Character.StopWaterMagicEffect then
                State.Character:StopWaterMagicEffect()
            end
        end
        return
    end

    -- 액션 수행 중이면 이동 불가 및 다른 입력 무시 (PickUp 또는 WaterMagic_Start)
    if State.bIsPerformingAction then
        if State.MovementComp then
            State.MovementComp.MaxWalkSpeed = 0
        end
        return
    end

    -- 왼쪽 마우스 클릭 또는 B 버튼으로 줍기
    local bPickupInput = Input:IsMouseButtonPressed(0) or Input:IsGamepadButtonPressed(GamepadButton.B)
    if bPickupInput then
        -- 현재 상태가 Idle/Walking/Running일 때만 픽업 허용 (전환 중 광클 방지)
        local currentState = State.StateMachine:GetCurrentStateName()
        if currentState == STATE_IDLE or currentState == STATE_WALKING or currentState == STATE_RUNNING then
            print("[FirefighterController] PickUp triggered!")
            State.bIsPerformingAction = true
            State.StateMachine:SetState(STATE_PICKUP, 0.1)
            return
        end
    end

    -- 오른쪽 마우스 클릭 또는 RT로 물 마법 시작
    local extinguishGauge = State.Character.ExtinguishGauge or 0
    local rtValue = Input:GetGamepadRightTrigger()
    local bRTPressed = (rtValue > 0.5) and (not State.bWasRTPressed)  -- RT Press 감지
    local bWaterMagicInput = Input:IsMouseButtonPressed(1) or bRTPressed

    if bWaterMagicInput and extinguishGauge > 0 then
        -- 현재 상태가 Idle/Walking/Running일 때만 물 마법 허용 (전환 중 광클 방지)
        local currentState = State.StateMachine:GetCurrentStateName()
        if currentState == STATE_IDLE or currentState == STATE_WALKING or currentState == STATE_RUNNING then
            print("[FirefighterController] WaterMagic Start triggered! (gauge=" .. tostring(extinguishGauge) .. ")")
            State.bIsPerformingAction = true
            State.bWaterMagicStarted = false
            State.StateMachine:SetState(STATE_WATERMAGIC_START, 0.1)
            State.bWasRTPressed = rtValue > 0.5
            return
        end
    end

    -- RT 상태 업데이트 (Press 감지용)
    State.bWasRTPressed = rtValue > 0.5

    -- Shift 키 또는 LB로 달리기
    if Input:IsKeyDown(16) or Input:IsGamepadButtonDown(GamepadButton.LB) then
        State.bIsRunning = true
    end

    -- WASD 또는 좌 스틱으로 이동 체크
    local leftStick = Input:GetGamepadLeftStick()
    local stickMagnitude = math.sqrt(leftStick.X * leftStick.X + leftStick.Y * leftStick.Y)

    if Input:IsKeyDown(87) or Input:IsKeyDown(65) or
       Input:IsKeyDown(83) or Input:IsKeyDown(68) or
       stickMagnitude > 0.1 then
        State.bIsMoving = true
    end

    -- 이동 속도 조절
    if State.MovementComp then
        if State.bIsRunning then
            State.MovementComp.MaxWalkSpeed = RUN_SPEED
        else
            State.MovementComp.MaxWalkSpeed = WALK_SPEED
        end
    end

    -- 애니메이션 상태 업데이트
    UpdateAnimationState()
end

function UpdateAnimationState()
    if not State.StateMachine then
        return
    end

    local currentState = State.StateMachine:GetCurrentStateName()
    local desiredState = STATE_IDLE

    -- 사람을 들고 있는지 체크 (C++ bIsCarryingPerson 변수)
    local bCarryingPerson = State.Character and State.Character.bIsCarryingPerson or false

    if State.bIsMoving then
        if State.bIsRunning then
            desiredState = bCarryingPerson and STATE_RUNNING_WITH_OBJECT or STATE_RUNNING
        else
            desiredState = bCarryingPerson and STATE_WALKING_WITH_OBJECT or STATE_WALKING
        end
    else
        desiredState = bCarryingPerson and STATE_IDLE_WITH_OBJECT or STATE_IDLE
    end

    -- 상태가 다르면 전환
    if currentState ~= desiredState then
        State.StateMachine:SetState(desiredState, -1.0)  -- -1.0: 전이 시간 사용
    end
end

-- AnimNotify 콜백 (C++에서 호출됨)
function OnAnimNotify(NotifyName)
    print("[FirefighterController] AnimNotify received: " .. tostring(NotifyName))

    -- EndPickUp 노티파이 처리: 액션 종료, Idle로 전환 (사람 들고 있으면 IdleWithObject)
    if NotifyName == "EndPickUp" then
        local bCarryingPerson = State.Character and State.Character.bIsCarryingPerson or false
        local targetState = bCarryingPerson and STATE_IDLE_WITH_OBJECT or STATE_IDLE
        print("[FirefighterController] EndPickUp notify triggered! Returning to " .. targetState)
        State.bIsPerformingAction = false
        if State.StateMachine then
            State.StateMachine:SetState(targetState, 0.2)
        end
    end

    -- WaterMagic_Start 완료 노티파이: Progress로 전환, 파티클 시작
    if NotifyName == "EndWaterMagicStart" then
        print("[FirefighterController] WaterMagic_Start complete! Transitioning to Progress.")
        State.bIsPerformingAction = false
        State.bWaterMagicStarted = true
        State.bIsUsingWaterMagic = true
        if State.StateMachine then
            State.StateMachine:SetState(STATE_WATERMAGIC_PROGRESS, 0.1)
        end
        -- 파티클 시작
        if State.Character and State.Character.PlayWaterMagicEffect then
            State.Character:PlayWaterMagicEffect()
        end
    end

    -- WaterMagic_End 완료 노티파이: Idle로 전환
    if NotifyName == "EndWaterMagicEnd" then
        print("[FirefighterController] WaterMagic_End complete! Returning to Idle.")
        State.bWaterMagicEnding = false
        State.bIsUsingWaterMagic = false
        State.bWaterMagicStarted = false
        if State.StateMachine then
            State.StateMachine:SetState(STATE_IDLE, 0.2)
        end
    end
end

function OnEndPlay()
    print("[FirefighterController] EndPlay called")
    State.Character = nil
    State.SkeletalMeshComp = nil
    State.StateMachine = nil
    State.MovementComp = nil
end
