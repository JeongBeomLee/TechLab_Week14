-- ItemPickupManager.lua
-- 아이템 줍기 시스템 (캐릭터에 부착)
--
-- 기능:
-- - 범위 내 아이템 감지 (SphereComponent Overlap 이벤트)
-- - 가장 가까운 아이템 하이라이트
-- - 마우스 클릭으로 아이템 줍기
-- - 인벤토리(GameInstance)에 아이템 추가

-- ═══════════════════════════════════════════════════════════════════════════
-- 상태 변수
-- ═══════════════════════════════════════════════════════════════════════════

local ItemsInRange = {}        -- 범위 내 아이템 목록 (FGameObject)
local ClosestItem = nil        -- 현재 가까운 아이템
local PendingPickupItem = nil  -- 클릭 시점에 저장된 픽업 대상

-- ═══════════════════════════════════════════════════════════════════════════
-- 헬퍼 함수
-- ═══════════════════════════════════════════════════════════════════════════

-- 두 위치 사이의 거리 제곱 계산 (성능 최적화)
local function DistanceSquared(loc1, loc2)
    local dx = loc1.X - loc2.X
    local dy = loc1.Y - loc2.Y
    local dz = loc1.Z - loc2.Z
    return dx*dx + dy*dy + dz*dz
end

-- ItemType 상수 (C++ EItemType과 일치)
local ITEM_TYPE_ITEM = 0
local ITEM_TYPE_PERSON = 1

-- 아이템의 ItemComponent 가져오기
local function GetItemInfo(gameObject)
    if not gameObject then return nil end
    local itemComp = GetItemComponent(gameObject)
    if not itemComp then return nil end
    return {
        Name = itemComp.ItemName,
        Type = itemComp.ItemType,
        Quantity = itemComp.Quantity,
        CanPickUp = itemComp.bCanPickUp
    }
end

-- 아이템 목록에서 제거
local function RemoveFromList(item)
    for i, v in ipairs(ItemsInRange) do
        if v == item then
            table.remove(ItemsInRange, i)
            return true
        end
    end
    return false
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 하이라이트 관리
-- ═══════════════════════════════════════════════════════════════════════════

-- 하이라이트 색상 (흰색)
local HighlightColor = { R = 1.0, G = 1.0, B = 1.0, A = 1.0 }

local function SetHighlight(gameObject, bHighlight)
    if not gameObject then return end
    local itemComp = GetItemComponent(gameObject)
    if itemComp then
        itemComp.bIsHighlighted = bHighlight

        -- 렌더러에 하이라이트 등록/해제 (아웃라인 셰이더 연동)
        if bHighlight then
            AddHighlight(gameObject, HighlightColor)
        else
            RemoveHighlight(gameObject)
        end
    end
end

local function UpdateClosestItem()
    local myLoc = Obj.Location
    local closest = nil
    local closestDistSq = math.huge

    for _, item in ipairs(ItemsInRange) do
        if item and item.bIsActive then
            local itemInfo = GetItemInfo(item)
            if itemInfo and itemInfo.CanPickUp then
                local distSq = DistanceSquared(myLoc, item.Location)
                if distSq < closestDistSq then
                    closestDistSq = distSq
                    closest = item
                end
            end
        end
    end

    -- 이전 하이라이트 해제
    if ClosestItem and ClosestItem ~= closest then
        SetHighlight(ClosestItem, false)
    end

    -- 새 하이라이트 적용
    if closest then
        SetHighlight(closest, true)
    end

    ClosestItem = closest
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 아이템 줍기
-- ═══════════════════════════════════════════════════════════════════════════

-- PickUp 노티파이 시점에 클릭했던 아이템 줍기
local function TryPickup()
    -- 클릭 시점에 저장된 아이템 사용
    if not PendingPickupItem then
        print("[ItemPickup] No pending item to pick up")
        return
    end

    -- 아이템이 유효한지 확인
    if not PendingPickupItem.bIsActive then
        print("[ItemPickup] Pending item is no longer active")
        PendingPickupItem = nil
        return
    end

    local itemInfo = GetItemInfo(PendingPickupItem)
    if not itemInfo or not itemInfo.CanPickUp then
        PendingPickupItem = nil
        return
    end

    -- Person 타입인 경우 특수 처리 (들기)
    if itemInfo.Type == ITEM_TYPE_PERSON then
        print("[ItemPickup] Person detected! Starting carry...")

        -- Firefighter Character 가져오기 (이 스크립트는 Firefighter에 부착됨)
        local firefighter = GetOwnerAs(Obj, "AFirefighterCharacter")
        if firefighter then
            -- StartCarryingPerson 호출 (FGameObject 직접 전달)
            firefighter:StartCarryingPerson(PendingPickupItem)
            print("[ItemPickup] StartCarryingPerson called!")

            -- 목록에서 제거 (파괴는 하지 않음)
            RemoveFromList(PendingPickupItem)
            if ClosestItem == PendingPickupItem then
                ClosestItem = nil
            end

            -- 더 이상 픽업 불가능하게 설정
            local itemComp = GetItemComponent(PendingPickupItem)
            if itemComp then
                itemComp.bCanPickUp = false
            end
        else
            print("[ItemPickup] ERROR: Could not get Firefighter character")
        end

        PendingPickupItem = nil
        UpdateClosestItem()
        return
    end

    -- 일반 아이템: 인벤토리에 추가
    local gameInstance = GetGameInstance()
    if gameInstance then
        gameInstance:AddItem(itemInfo.Name, itemInfo.Quantity)
        gameInstance:SetString("ItemType_" .. itemInfo.Name, tostring(itemInfo.Type))
        print("[ItemPickup] Picked up: " .. itemInfo.Name .. " x" .. itemInfo.Quantity)

        -- 소방복 아이템인 경우 GameInstance에 플래그 설정
        -- (FirefighterController에서 매 프레임 체크하여 장착 처리)
        if itemInfo.Name == "FireSuit" then
            print("[ItemPickup] FireSuit detected! Setting equip flag...")
            gameInstance:SetBool("bEquipFireSuit", true)
        end
    end

    -- 픽업할 아이템 참조 저장
    local pickedItem = PendingPickupItem
    local itemLocation = pickedItem.Location  -- 파괴 전에 위치 저장
    PendingPickupItem = nil

    -- 목록에서 제거
    RemoveFromList(pickedItem)
    if ClosestItem == pickedItem then
        ClosestItem = nil
    end

    -- 아이템 픽업 파티클 재생
    local firefighter = GetOwnerAs(Obj, "AFirefighterCharacter")
    if firefighter then
        firefighter:PlayItemPickupEffect(itemLocation)
    end

    -- 아이템 파괴
    DestroyObject(pickedItem)

    -- 가장 가까운 아이템 다시 계산
    UpdateClosestItem()
end

-- 클릭 시 호출 - 대상 아이템 저장 (항상 최신 아이템으로 갱신)
local function OnPickupStarted()
    -- 항상 현재 가장 가까운 아이템으로 갱신 (거부된 클릭의 구형 값 방지)
    if ClosestItem then
        PendingPickupItem = ClosestItem
        print("[ItemPickup] Pickup target: " .. (GetItemInfo(ClosestItem).Name or "Unknown"))
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 생명주기 콜백
-- ═══════════════════════════════════════════════════════════════════════════

function OnBeginPlay()
    print("[ItemPickupManager] Initialized")
    ItemsInRange = {}
    ClosestItem = nil
    PendingPickupItem = nil
end

function OnBeginOverlap(OtherObj)
    if not OtherObj then return end

    -- ItemComponent가 있는지 확인
    if HasItemComponent(OtherObj) then
        local itemInfo = GetItemInfo(OtherObj)
        if itemInfo and itemInfo.CanPickUp then
            table.insert(ItemsInRange, OtherObj)
            UpdateClosestItem()
            --print("[ItemPickupManager] Item in range: " .. (itemInfo.Name or "Unknown"))
        end
    end
end

function OnEndOverlap(OtherObj)
    if not OtherObj then return end

    -- 목록에서 제거
    local wasRemoved = RemoveFromList(OtherObj)
    if wasRemoved then
        SetHighlight(OtherObj, false)
        if ClosestItem == OtherObj then
            ClosestItem = nil
        end
        UpdateClosestItem()
        --print("[ItemPickupManager] Item left range")
    end
end

function Update(dt)
    -- 마우스 클릭 또는 게임패드 B 버튼으로 픽업 대상 저장 (실제 줍기는 PickUp 노티파이에서)
    local bPickupInput = Input:IsMouseButtonPressed(MouseButton.Left) or Input:IsGamepadButtonPressed(GamepadButton.B)
    if bPickupInput then
        OnPickupStarted()
    end

    -- 가장 가까운 아이템 매 프레임 갱신 (플레이어/아이템 이동 대응)
    UpdateClosestItem()
end

-- AnimNotify 콜백 (C++에서 호출됨)
function OnAnimNotify(NotifyName)
    print("[ItemPickupManager] AnimNotify received: " .. tostring(NotifyName))

    -- PickUp 노티파이: 실제로 아이템 줍기
    if NotifyName == "PickUp" then
        print("[ItemPickupManager] PickUp notify - executing pickup!")
        TryPickup()
    end
    -- EndPickUp은 FirefighterController에서 처리 (Idle 전환)
end

function OnEndPlay()
    -- PIE 종료 시에는 개별 오브젝트 접근하지 않음 (이미 파괴되었을 수 있음)
    -- 렌더러의 하이라이트 목록만 클리어
    ClearHighlights()

    -- 로컬 상태만 정리
    ItemsInRange = {}
    ClosestItem = nil
    PendingPickupItem = nil
    print("[ItemPickupManager] Cleaned up")
end
