-- KneePraying.lua
-- Play knee_praying animation in loop

local STATE_NAME = "KneePraying"
local ASSET_PATH = "Data/Animations/KneePraying.animsequence"

local M = {}
M.Comp = nil

function OnBeginPlay()
    local comp = GetComponent(Obj, "USkeletalMeshComponent")
    if comp == nil then
        print("[KneePraying] No USkeletalMeshComponent on actor")
        return
    end

    comp:UseStateMachine()
    local sm = comp:GetOrCreateStateMachine()

    -- Add state with looping enabled (4th parameter = true)
    sm:AddState(STATE_NAME, ASSET_PATH, 1.0, true)

    -- Start playing
    sm:SetState(STATE_NAME, 0.0)

    M.Comp = comp
    print("[KneePraying] Playing knee_praying animation in loop")
end

function Tick(Delta)
    -- No input handling needed, just loop the animation
end

return M
