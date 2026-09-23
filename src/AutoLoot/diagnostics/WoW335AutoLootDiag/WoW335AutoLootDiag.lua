-- WoW 3.3.5a (30300) addon. NOT a native DLL and NOT automatic corpse scanning.
-- Starts disabled. No movement, coordinate spoofing, injection or patching.
local ADDON = ...
local f = CreateFrame("Frame")
local enabled = false
local windowOpen = false
local attempts = 0
local cooldown = 0
local cleared = {}
local maxAttempts = 3
local scanInterval = 0.25
local recorded = {}
local maxEntries = 120

-- Intentionally silent in game: keep diagnostic events in SavedVariables for
-- the updater report, but never post AutoLoot diagnostics to the chat frame.
-- Slash commands remain available without producing chat messages.
local function Print(_) end

local function Record(event, detail)
    local when = date("%Y-%m-%d %H:%M:%S")
    local line = when .. " " .. tostring(event) .. " " .. tostring(detail or "")
    table.insert(recorded, line)
    while #recorded > maxEntries do table.remove(recorded, 1) end
    if type(WoW335AutoLootDiagLog) ~= "table" then
        WoW335AutoLootDiagLog = {}
    end
    WoW335AutoLootDiagLog.entries = recorded
    WoW335AutoLootDiagLog.interface = select(4, GetBuildInfo())
    WoW335AutoLootDiagLog.client = GetBuildInfo()
end

local function Pending()
    local count = GetNumLootItems() or 0
    local pending = 0
    for slot = 1, count do
        if not cleared[slot] then
            local texture, name, amount, quality, locked = GetLootSlotInfo(slot)
            if texture or name then
                if not locked then pending = pending + 1 end
            end
        end
    end
    return count, pending
end

local function DrainOnce()
    if not enabled or not windowOpen or attempts >= maxAttempts then return end
    attempts = attempts + 1
    local count, pending = Pending()
    local submitted, failed = 0, 0
    for slot = count, 1, -1 do
        if not cleared[slot] then
            local texture, name, amount, quality, locked = GetLootSlotInfo(slot)
            if (texture or name) and not locked then
                local ok, err = pcall(LootSlot, slot)
                if ok then
                    submitted = submitted + 1
                    -- Confirmation can be required for bind-on-pickup loot.
                    -- Do not auto-confirm an item that was not explicitly accepted.
                else
                    failed = failed + 1
                    Record("LOOT_CALL_FAILED", tostring(slot) .. ":" .. tostring(err))
                end
            end
        end
    end
    Record("DRAIN", "attempt=" .. attempts .. " slots=" .. count .. " pending=" .. pending .. " submitted=" .. submitted .. " failed=" .. failed)
    if attempts == 1 or failed > 0 then
        Print("Okno lootu: sloty=" .. count .. ", wyslane proby=" .. submitted .. ", bledy=" .. failed)
    end
end

f:RegisterEvent("PLAYER_LOGIN")
f:RegisterEvent("LOOT_OPENED")
f:RegisterEvent("LOOT_SLOT_CLEARED")
f:RegisterEvent("LOOT_CLOSED")
f:RegisterEvent("UI_ERROR_MESSAGE")
f:RegisterEvent("PLAYER_LOGOUT")
f:SetScript("OnEvent", function(self, event, arg1)
    if event == "PLAYER_LOGIN" then
        if type(WoW335AutoLootDiagLog) == "table" and type(WoW335AutoLootDiagLog.entries) == "table" then
            recorded = WoW335AutoLootDiagLog.entries
        end
        Record("LOGIN", "diagnostic addon; default OFF; no automatic corpse interaction")
        Print("Gotowy (OFF). /al335 on | off | status | log | clear")
    elseif event == "LOOT_OPENED" then
        windowOpen = true
        attempts = 0
        cooldown = 0
        cleared = {}
        Record("LOOT_OPENED", "num=" .. tostring(GetNumLootItems()) .. " autoloot_cvar=" .. tostring(GetCVar("autoLootDefault")) .. " enabled=" .. tostring(enabled))
        if enabled then DrainOnce() end
    elseif event == "LOOT_SLOT_CLEARED" then
        if arg1 then cleared[arg1] = true end
        Record("LOOT_SLOT_CLEARED", "slot=" .. tostring(arg1))
    elseif event == "LOOT_CLOSED" then
        local count, pending = Pending()
        Record("LOOT_CLOSED", "num=" .. count .. " pending_estimate=" .. pending .. " attempts=" .. attempts)
        windowOpen = false
        Print("Okno zamkniete; zapisz raport komenda /reload lub wylogowaniem.")
    elseif event == "UI_ERROR_MESSAGE" then
        if windowOpen then Record("UI_ERROR", tostring(arg1)) end
    elseif event == "PLAYER_LOGOUT" then
        Record("LOGOUT", "saved")
    end
end)

f:SetScript("OnUpdate", function(self, elapsed)
    if not enabled or not windowOpen or attempts >= maxAttempts then return end
    cooldown = cooldown + elapsed
    if cooldown < scanInterval then return end
    cooldown = 0
    local count, pending = Pending()
    if pending > 0 then DrainOnce() end
end)

SLASH_AUTOLOOT335DIAG1 = "/al335"
SlashCmdList["AUTOLOOT335DIAG"] = function(text)
    local cmd = string.lower(string.match(text or "", "^%s*(%S+)") or "status")
    if cmd == "on" then
        enabled = true
        Record("ENABLE", "manual open only")
        Print("ON. Otworz recznie zwloki NPC (prawy klik). Dodatek zbiera sloty z otwartego okna.")
        if windowOpen then DrainOnce() end
    elseif cmd == "off" then
        enabled = false
        Record("DISABLE", "")
        Print("OFF. Nie bedzie wysylal prob zbierania.")
    elseif cmd == "log" then
        local from = math.max(1, #recorded - 9)
        for i = from, #recorded do Print(recorded[i]) end
        if #recorded == 0 then Print("Brak zdarzen") end
    elseif cmd == "clear" then
        recorded = {}
        Record("LOG_CLEARED", "")
        Print("Wyczyszczono zapis zdarzen")
    elseif cmd == "status" then
        local _, _, _, interfaceVersion = GetBuildInfo()
        Print("stan=" .. (enabled and "ON" or "OFF") .. " okno=" .. tostring(windowOpen) .. " interface=" .. tostring(interfaceVersion) .. " zapisane_zdarzenia=" .. tostring(#recorded))
    else
        Print("Komendy: /al335 on | off | status | log | clear")
    end
end
