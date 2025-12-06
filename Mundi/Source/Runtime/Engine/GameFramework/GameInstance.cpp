#include "pch.h"
#include "GameInstance.h"

IMPLEMENT_CLASS(UGameInstance)

UGameInstance::UGameInstance()
{
    UE_LOG("[info] GameInstance: Created");
}

UGameInstance::~UGameInstance()
{
    UE_LOG("[info] GameInstance: Destroyed");
}

// ════════════════════════════════════════════════════════════════════════
// 아이템 관리
// ════════════════════════════════════════════════════════════════════════

void UGameInstance::AddItem(const FString& ItemTag, int32 Count)
{
    if (ItemTag.empty())
    {
        UE_LOG("[warning] GameInstance::AddItem: Empty ItemTag");
        return;
    }

    if (Count <= 0)
    {
        UE_LOG("[warning] GameInstance::AddItem: Invalid count %d", Count);
        return;
    }

    // 기존 아이템이 있으면 추가, 없으면 새로 생성
    auto it = CollectedItems.find(ItemTag);
    if (it != CollectedItems.end())
    {
        it->second += Count;
        UE_LOG("[info] GameInstance::AddItem: %s x%d (Total: %d)",
               ItemTag.c_str(), Count, it->second);
    }
    else
    {
        CollectedItems[ItemTag] = Count;
        UE_LOG("[info] GameInstance::AddItem: %s x%d (New)",
               ItemTag.c_str(), Count);
    }
}

int32 UGameInstance::GetItemCount(const FString& ItemTag) const
{
    auto it = CollectedItems.find(ItemTag);
    return (it != CollectedItems.end()) ? it->second : 0;
}

bool UGameInstance::RemoveItem(const FString& ItemTag, int32 Count)
{
    if (ItemTag.empty() || Count <= 0)
        return false;

    auto it = CollectedItems.find(ItemTag);
    if (it == CollectedItems.end())
    {
        UE_LOG("[warning] GameInstance::RemoveItem: Item not found: %s", ItemTag.c_str());
        return false;
    }

    if (it->second < Count)
    {
        UE_LOG("[warning] GameInstance::RemoveItem: Not enough items (Have: %d, Need: %d)",
               it->second, Count);
        return false;
    }

    it->second -= Count;
    UE_LOG("[info] GameInstance::RemoveItem: %s x%d (Remaining: %d)",
           ItemTag.c_str(), Count, it->second);

    // 개수가 0이 되면 맵에서 제거
    if (it->second <= 0)
    {
        CollectedItems.erase(it);
        UE_LOG("[info] GameInstance::RemoveItem: %s removed from inventory", ItemTag.c_str());
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════════
// 플레이어 상태 관리

void UGameInstance::ResetPlayerData()
{
    PlayerHealth = 100;
    PlayerScore = 0;
    RescuedCount = 0;
    PlayerName.clear();
    UE_LOG("[info] GameInstance::ResetPlayerData: Player data reset");
}

// ════════════════════════════════════════════════════════════════════════
// 범용 키-값 저장소

void UGameInstance::SetInt(const FString& Key, int32 Value)
{
    if (Key.empty())
    {
        UE_LOG("[warning] GameInstance::SetInt: Empty key");
        return;
    }

    IntData[Key] = Value;
}

int32 UGameInstance::GetInt(const FString& Key, int32 DefaultValue) const
{
    auto it = IntData.find(Key);
    return (it != IntData.end()) ? it->second : DefaultValue;
}

void UGameInstance::SetFloat(const FString& Key, float Value)
{
    if (Key.empty())
    {
        UE_LOG("[warning] GameInstance::SetFloat: Empty key");
        return;
    }

    FloatData[Key] = Value;
}

float UGameInstance::GetFloat(const FString& Key, float DefaultValue) const
{
    auto it = FloatData.find(Key);
    return (it != FloatData.end()) ? it->second : DefaultValue;
}

void UGameInstance::SetString(const FString& Key, const FString& Value)
{
    if (Key.empty())
    {
        UE_LOG("[warning] GameInstance::SetString: Empty key");
        return;
    }

    StringData[Key] = Value;
}

FString UGameInstance::GetString(const FString& Key, const FString& DefaultValue) const
{
    auto it = StringData.find(Key);
    return (it != StringData.end()) ? it->second : DefaultValue;
}

bool UGameInstance::HasKey(const FString& Key) const
{
    return IntData.find(Key) != IntData.end() ||
           FloatData.find(Key) != FloatData.end() ||
           StringData.find(Key) != StringData.end();
}

void UGameInstance::ClearAll()
{
    CollectedItems.clear();
    IntData.clear();
    FloatData.clear();
    StringData.clear();
    ResetPlayerData();
    UE_LOG("[info] GameInstance::ClearAll: All data cleared");
}

// ════════════════════════════════════════════════════════════════════════
// 디버그/로그

void UGameInstance::PrintDebugInfo() const
{
    UE_LOG("════════════════════════════════════════════════════════");
    UE_LOG("[debug] GameInstance Debug Info:");
    UE_LOG("────────────────────────────────────────────────────────");

    // 플레이어 상태
    UE_LOG("Player Data:");
    UE_LOG("  - Name: %s", PlayerName.empty() ? "(none)" : PlayerName.c_str());
    UE_LOG("  - Health: %d", PlayerHealth);
    UE_LOG("  - Score: %d", PlayerScore);
    UE_LOG("  - Rescued Count: %d", RescuedCount);

    // 아이템
    UE_LOG("────────────────────────────────────────────────────────");
    UE_LOG("Collected Items (%zu):", CollectedItems.size());
    for (const auto& Pair : CollectedItems)
    {
        UE_LOG("  - %s: x%d", Pair.first.c_str(), Pair.second);
    }

    // Int 데이터
    UE_LOG("────────────────────────────────────────────────────────");
    UE_LOG("Int Data (%zu):", IntData.size());
    for (const auto& Pair : IntData)
    {
        UE_LOG("  - %s: %d", Pair.first.c_str(), Pair.second);
    }

    // Float 데이터
    UE_LOG("────────────────────────────────────────────────────────");
    UE_LOG("Float Data (%zu):", FloatData.size());
    for (const auto& Pair : FloatData)
    {
        UE_LOG("  - %s: %.2f", Pair.first.c_str(), Pair.second);
    }

    // String 데이터
    UE_LOG("────────────────────────────────────────────────────────");
    UE_LOG("String Data (%zu):", StringData.size());
    for (const auto& Pair : StringData)
    {
        UE_LOG("  - %s: %s", Pair.first.c_str(), Pair.second.c_str());
    }

    UE_LOG("════════════════════════════════════════════════════════");
}
