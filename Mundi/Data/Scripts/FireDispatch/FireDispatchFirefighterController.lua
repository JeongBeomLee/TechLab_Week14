-- ============================================================================
-- FireDispatchFirefighterController.lua
-- FireDispatch 컷씬 전용 소방수 캐릭터 컨트롤러
-- ============================================================================

-- ============================================================================
-- 상태를 전역 테이블에 저장 (핫 리로드 시 보존됨)
-- ============================================================================
State = State or {}

-- 애니메이션 상태 이름
local STATE_IDLE = "Idle"
local STATE_WALK = "Walk"
local STATE_RUNNING = "Running"
local STATE_PICKUP = "PickUp"
local STATE_KIP_UP = "KipUp"

-- 애니메이션 에셋 경로 (With_Cloth 버전 - 소방복 장착 상태)
local ANIM_WITH_CLOTH = {
    IDLE = "Data/firefighter/Firefighter_With_Cloth_Idle_mixamo.com",
    WALK = "Data/firefighter/Firefighter_With_Cloth_Walking_mixamo.com",
    RUNNING = "Data/firefighter/Firefighter_With_Cloth_Running_mixamo.com",
    PICKUP = "Data/firefighter/Firefighter_With_Cloth_Picking_Up_Object_mixamo.com",
    KIP_UP = "Data/firefighter/Firefighter_With_Cloth_Kip_Up_mixamo.com"
}

-- 메시 경로
local MESH_WITH_CLOTH = "Data/firefighter/Firefighter_With_Cloth.fbx"

-- 이동 속도
local WALK_SPEED = 20.0
local RUN_SPEED = 40.0  -- GameMode의 RunSpeed와 동일하게 설정 가능

-- ============================================================================
-- 참조 초기화 함수
-- ============================================================================
local function InitReferences()
    -- Obj는 LuaScriptComponent이므로 Owner(액터)를 가져와야 함
    State.Actor = GetOwnerAs(Obj, "AActor")
    State.SkeletalMeshComp = GetComponent(Obj, "USkeletalMeshComponent")

    if State.SkeletalMeshComp then
        State.StateMachine = State.SkeletalMeshComp:GetOrCreateStateMachine()
    end
end

function OnBeginPlay()
    print("[FireDispatchFirefighter] BeginPlay called")

    InitReferences()

    -- 디버깅: Component 정보 출력
    if State.SkeletalMeshComp then
        -- 래그돌 루트 본을 pelvis로 설정 (공중에 뜨는 문제 방지)
        if State.SkeletalMeshComp.SetRagdollRootBoneByName then
            local success = State.SkeletalMeshComp:SetRagdollRootBoneByName("mixamorig:Hips")
            if success then
                print("[FireDispatchFirefighter] Ragdoll root bone set to 'mixamorig:Hips'")
            end
        end
    end

    if not State.SkeletalMeshComp then
        print("[FireDispatchFirefighter] ERROR: No USkeletalMeshComponent found")
        return
    end

    -- 초기 상태
    State.CurrentPhase = "None"
    State.RunStartPosition = nil
    State.RunTargetPosition = nil
    State.RunElapsed = 0
    State.RunDuration = 2.0

    -- GetUp 블렌딩용 상태
    State.GetUpTimer = 0
    State.GetUpDuration = 1.5  -- GetUp 블렌딩 시간
    State.GetUpStartRotation = nil
    State.GetUpTargetRotation = nil

    -- 애니메이션 상태 머신 설정
    SetupAnimationStateMachine()
end

function OnHotReload()
    InitReferences()
end

function SetupAnimationStateMachine()

    -- 상태 머신 사용 활성화
    State.SkeletalMeshComp:UseStateMachine()
    State.StateMachine = State.SkeletalMeshComp:GetOrCreateStateMachine()

    if not State.StateMachine then
        return
    end

    -- 기존 상태 클리어
    State.StateMachine:Clear()

    -- 상태 추가 (이름, 에셋 경로, 재생 속도, 루핑 여부)
    local idleIdx = State.StateMachine:AddState(STATE_IDLE, ANIM_WITH_CLOTH.IDLE, 1.0, true)
    local walkIdx = State.StateMachine:AddState(STATE_WALK, ANIM_WITH_CLOTH.WALK, 1.0, true)
    local runIdx = State.StateMachine:AddState(STATE_RUNNING, ANIM_WITH_CLOTH.RUNNING, 1.5, true)
    local pickupIdx = State.StateMachine:AddState(STATE_PICKUP, ANIM_WITH_CLOTH.PICKUP, 1.0, false)
    local kipUpIdx = State.StateMachine:AddState(STATE_KIP_UP, ANIM_WITH_CLOTH.KIP_UP, 1.0, false)

    -- 전이(Transition) 추가 (BlendTime)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_WALK, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALK, STATE_IDLE, 0.2)
    State.StateMachine:AddTransitionByName(STATE_WALK, STATE_RUNNING, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_WALK, 0.2)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_RUNNING, 0.2)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_IDLE, 0.2)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_PICKUP, 0.1)
    State.StateMachine:AddTransitionByName(STATE_RUNNING, STATE_PICKUP, 0.1)
    State.StateMachine:AddTransitionByName(STATE_PICKUP, STATE_IDLE, 0.2)
    State.StateMachine:AddTransitionByName(STATE_IDLE, STATE_KIP_UP, 0.2)
    State.StateMachine:AddTransitionByName(STATE_KIP_UP, STATE_IDLE, 0.5)

    -- 초기 상태: Idle
    State.StateMachine:SetState(STATE_IDLE, 0.0)
end

-- ============================================================================
-- 페이즈 체크 및 실행 (GameInstance 플래그 기반)
-- ============================================================================

local function CheckAndExecutePhase()
    local gi = GetGameInstance()
    if not gi then return end

    -- Run 페이즈
    if gi:GetBool("FireDispatch_Run", false) then
        gi:SetBool("FireDispatch_Run", false)
        State.CurrentPhase = "Running"

        -- 달리기 애니메이션
        if State.StateMachine then
            State.StateMachine:SetState(STATE_RUNNING, 0.2)
        end
    end

    -- Walk 페이즈
    if gi:GetBool("FireDispatch_Walk", false) then
        gi:SetBool("FireDispatch_Walk", false)
        State.CurrentPhase = "Walking"

        -- 걷기 애니메이션
        if State.StateMachine then
            State.StateMachine:SetState(STATE_WALK, 0.2)
        end
    end

    -- Fall 페이즈
    if gi:GetBool("FireDispatch_Fall", false) then
        gi:SetBool("FireDispatch_Fall", false)
        State.CurrentPhase = "Falling"

        -- 현재 애니메이션 상태를 정지시켜서 포즈 고정
        if State.StateMachine then
            -- 현재 상태의 재생 속도를 0으로 설정 (애니메이션 정지)
            local currentState = State.StateMachine:GetCurrentStateName()
            if currentState and currentState ~= "" then
                State.StateMachine:SetStatePlayRate(currentState, 0.0)
            end
        end

        -- 래그돌 모드 활성화
        if State.SkeletalMeshComp then
            -- 래그돌 중 컴포넌트 위치 자동 업데이트 비활성화 (공중에 뜨는 문제 방지)
            if State.SkeletalMeshComp.bUpdateComponentLocationFromRagdoll ~= nil then
                State.SkeletalMeshComp.bUpdateComponentLocationFromRagdoll = false
            end

            -- SetPhysicsMode 함수로 래그돌 모드 활성화 (내부적으로 InitRagdoll 등 처리)
            if State.SkeletalMeshComp.SetPhysicsMode then
                State.SkeletalMeshComp:SetPhysicsMode(2)  -- EPhysicsMode::Ragdoll = 2
            else
                -- 함수가 없으면 프로퍼티로 직접 설정
                State.SkeletalMeshComp.PhysicsMode = 2
            end

        end
    end

    -- GetUp 페이즈
    if gi:GetBool("FireDispatch_GetUp", false) then
        gi:SetBool("FireDispatch_GetUp", false)
        State.CurrentPhase = "GettingUp"
        State.GetUpTimer = 0  -- 타이머 리셋
        State.GetUpBlendStarted = false  -- 블렌딩 시작 플래그
        State.GetUpPoseSaved = false  -- 래그돌 포즈 저장 플래그

        -- 현재 회전을 Euler로 변환하여 저장 (보간용)
        if Obj and Obj.Rotation then
            -- Rotation을 Euler 각도로 변환 (ToEulerZYXDeg 사용)
            if Obj.Rotation.ToEulerZYXDeg then
                local eulerAngles = Obj.Rotation:ToEulerZYXDeg()
                State.GetUpStartYaw = eulerAngles.Z  -- Yaw만 저장
                State.GetUpTargetYaw = eulerAngles.Z  -- Yaw 유지
            end
        end

        -- 캐릭터가 공중에 떠있을 수 있으므로 Z 위치를 바닥으로 조정
        if Obj and Obj.Location then
            -- 현재 위치에서 Z만 바닥 높이로 설정
            local currentLoc = Obj.Location
            local groundZ = -19.42  -- FireDispatch 씬의 바닥 높이
            Obj.Location = Vector(currentLoc.X, currentLoc.Y, groundZ)
        end

        -- 컴포넌트 위치 자동 업데이트 재활성화
        if State.SkeletalMeshComp and State.SkeletalMeshComp.bUpdateComponentLocationFromRagdoll ~= nil then
            State.SkeletalMeshComp.bUpdateComponentLocationFromRagdoll = true
        end

        -- 모든 애니메이션 상태의 재생 속도를 정상으로 복구
        if State.StateMachine then
            State.StateMachine:SetStatePlayRate(STATE_IDLE, 1.0)
            State.StateMachine:SetStatePlayRate(STATE_WALK, 1.0)
            State.StateMachine:SetStatePlayRate(STATE_RUNNING, 1.5)
            State.StateMachine:SetStatePlayRate(STATE_PICKUP, 1.0)
        end

    end

    -- Board 페이즈
    if gi:GetBool("FireDispatch_Board", false) then
        gi:SetBool("FireDispatch_Board", false)
        State.CurrentPhase = "Boarding"

        -- PickUp 애니메이션 실행 (차량 탑승 동작으로 사용)
        if State.StateMachine then
            State.StateMachine:SetState(STATE_PICKUP, 0.1)
        end
        -- 카메라 전환 트리거
        local gi = GetGameInstance()
        if gi then
            gi:SetBool("FireDispatch_BoardingCameraTransition", true)
        end
    end

    -- Drive 페이즈
    if gi:GetBool("FireDispatch_Drive", false) then
        print("[FireDispatchFirefighter] *** Drive phase detected! Setting camera transition flag ***")
        gi:SetBool("FireDispatch_Drive", false)
        State.CurrentPhase = "Driving"

        -- 카메라 전환 트리거 (Drive 페이즈에서도 카메라 전환)
        gi:SetBool("FireDispatch_DriveCameraTransition", true)
        print("[FireDispatchFirefighter] FireDispatch_DriveCameraTransition flag set to true")

        -- 액터 숨김 - Location을 아주 멀리 이동
        if Obj then
            Obj.Location = Vector(10000, 10000, 10000)
        end
    end
end

-- ============================================================================
-- Update 함수
-- ============================================================================
function Update(DeltaTime)
    if not State.Actor or not State.StateMachine then
        return
    end

    -- 페이즈 플래그 체크
    CheckAndExecutePhase()

    -- Running 페이즈 중 이동 처리
    if State.CurrentPhase == "Running" then
        local gi = GetGameInstance()
        if gi and Obj and Obj.Location then
            local runSpeed = gi:GetFloat("FireDispatch_RunSpeed", RUN_SPEED)
            -- 새로운 위치 생성하여 설정 (Vector 함수 사용, X 방향 반대로)
            local newLoc = Vector(
                Obj.Location.X - runSpeed * DeltaTime,  -- X 방향 반대
                Obj.Location.Y,
                Obj.Location.Z
            )
            Obj.Location = newLoc
        end
    end

    -- Walking 페이즈 중 이동 처리
    if State.CurrentPhase == "Walking" then
        local gi = GetGameInstance()
        if gi and Obj and Obj.Location then
            local walkSpeed = gi:GetFloat("FireDispatch_WalkSpeed", WALK_SPEED)
            -- 새로운 위치 생성하여 설정 (Vector 함수 사용, X 방향 반대로)
            local newLoc = Vector(
                Obj.Location.X - walkSpeed * DeltaTime,  -- X 방향 반대
                Obj.Location.Y,
                Obj.Location.Z
            )
            Obj.Location = newLoc
        end
    end

    -- GettingUp 페이즈 중 회전 보간 및 애니메이션 블렌딩 처리
    if State.CurrentPhase == "GettingUp" then
        State.GetUpTimer = State.GetUpTimer + DeltaTime

        -- 첫 프레임: 래그돌 포즈를 BaseAnimationPose로 저장
        if not State.GetUpPoseSaved then
            State.GetUpPoseSaved = true

            -- 래그돌 포즈 저장 (Ragdoll 모드에서 현재 포즈를 캡처)
            if State.SkeletalMeshComp and State.SkeletalMeshComp.SaveCurrentPoseAsBase then
                State.SkeletalMeshComp:SaveCurrentPoseAsBase()
            end

            -- Animation 모드로 전환 (래그돌 포즈를 유지)
            if State.SkeletalMeshComp and State.SkeletalMeshComp.SetPhysicsMode then
                State.SkeletalMeshComp:SetPhysicsMode(0)  -- EPhysicsMode::Animation = 0
            end
        end

        -- 두 번째 프레임: Idle 애니메이션 블렌딩 시작
        if State.GetUpPoseSaved and not State.GetUpBlendStarted then
            State.GetUpBlendStarted = true
            if State.StateMachine then
                -- 래그돌 포즈(BaseAnimationPose)에서 KipUp으로 블렌딩 시작
                State.StateMachine:SetState(STATE_KIP_UP, 0.0)  -- 0.0초 블렌드 (즉시 시작)
            end
        end

        if State.GetUpTimer < State.GetUpDuration and Obj then
            -- 보간 진행 (0 ~ 1)
            local t = State.GetUpTimer / State.GetUpDuration
            -- EaseOut 커브 적용 (부드러운 감속)
            t = 1 - (1 - t) * (1 - t)

            -- 회전을 서서히 정상 자세로 보정 (Roll=0, Pitch=0, Yaw는 유지)
            if State.GetUpTargetYaw and Obj.Rotation then
                -- Quaternion.MakeFromEulerZYX 사용 (Roll, Pitch, Yaw 순서)
                -- Roll과 Pitch를 서서히 0으로, Yaw는 유지
                local newRotation = Quaternion.MakeFromEulerZYX(Vector(0, 0, State.GetUpTargetYaw))
                Obj.Rotation = newRotation
            end
        elseif Obj then
            -- 보간 완료 - 최종 회전 설정 및 Kinematic 모드로 전환
            if State.GetUpTargetYaw then
                local finalRotation = Quaternion.MakeFromEulerZYX(Vector(0, 0, State.GetUpTargetYaw))
                Obj.Rotation = finalRotation
            end

            -- Kinematic 모드로 전환 (애니메이션이 물리를 제어하도록)
            if State.SkeletalMeshComp and State.SkeletalMeshComp.SetPhysicsMode then
                State.SkeletalMeshComp:SetPhysicsMode(1)  -- EPhysicsMode::Kinematic = 1
                print("[FireDispatchFirefighter] GetUp complete, switched to Kinematic mode")
            end
            
            -- GetUp 페이즈 완료 후 상태 초기화
            State.CurrentPhase = "None"
        end
    end
end

function OnEndPlay()
    State.Actor = nil
    State.SkeletalMeshComp = nil
    State.StateMachine = nil
end
