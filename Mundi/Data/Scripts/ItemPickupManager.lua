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

    -- 인벤토리에 추가
    local gameInstance = GetGameInstance()
    if gameInstance then
        gameInstance:AddItem(itemInfo.Name, itemInfo.Quantity)
        gameInstance:SetString("ItemType_" .. itemInfo.Name, tostring(itemInfo.Type))
        print("[ItemPickup] Picked up: " .. itemInfo.Name .. " x" .. itemInfo.Quantity)
    end

    -- 픽업할 아이템 참조 저장
    local pickedItem = PendingPickupItem
    PendingPickupItem = nil

    -- 목록에서 제거
    RemoveFromList(pickedItem)
    if ClosestItem == pickedItem then
        ClosestItem = nil
    end

    -- 아이템 파괴
    DestroyObject(pickedItem)

    -- 가장 가까운 아이템 다시 계산
    UpdateClosestItem()
end

-- 클릭 시 호출 - 대상 아이템 저장 (이미 대기 중이면 무시)
local function OnPickupStarted()
    -- 이미 픽업 대기 중이면 덮어쓰지 않음
    if PendingPickupItem then
        return
    end

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
            print("[ItemPickupManager] Item in range: " .. (itemInfo.Name or "Unknown"))
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
        print("[ItemPickupManager] Item left range")
    end
end

function Update(dt)
    -- 마우스 클릭 시 픽업 대상 저장 (실제 줍기는 PickUp 노티파이에서)
    if Input:IsMouseButtonPressed(MouseButton.Left) then
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
