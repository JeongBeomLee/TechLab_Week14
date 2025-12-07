#pragma once
#include "Object.h"
#include "UGameInstance.generated.h"

/**
 * 레벨 전환 간 데이터를 보관하는 게임 인스턴스
 * - 아이템 수집 정보 저장
 * - 플레이어 상태 저장 (HP, 스코어 등)
 * - 범용 키-값 저장소 (Int, Float, String)
 * - GameEngine이 소유하며 PIE 세션 동안 유지됨
 */
UCLASS(DisplayName="UGameInstance")
class UGameInstance : public UObject
{
public:
    GENERATED_REFLECTION_BODY()
    
    UGameInstance();
    virtual ~UGameInstance() override;

    // ════════════════════════════════════════════════════════════════════════
    // 아이템 관리

    /** 아이템 추가 (수집 씬에서 사용) */
    void AddItem(const FString& ItemTag, int32 Count = 1);

    /** 아이템 개수 조회 */
    int32 GetItemCount(const FString& ItemTag) const;

    /** 아이템 제거 (사용 시) - 성공 여부 반환 */
    bool RemoveItem(const FString& ItemTag, int32 Count = 1);

    /** 모든 아이템 제거 */
    void ClearItems() { CollectedItems.clear(); }

    /** 특정 아이템 소유 여부 */
    bool HasItem(const FString& ItemTag) const { return CollectedItems.find(ItemTag) != CollectedItems.end(); }

    /** 전체 아이템 목록 조회 (HUD 표시용) */
    const TMap<FString, int32>& GetAllItems() const { return CollectedItems; }

    // ════════════════════════════════════════════════════════════════════════
    // 플레이어 상태 관리

    /** 플레이어 체력 설정 */
    void SetPlayerHealth(int32 Health) { PlayerHealth = Health; }
    int32 GetPlayerHealth() const { return PlayerHealth; }

    /** 플레이어 점수 설정 */
    void SetPlayerScore(int32 Score) { PlayerScore = Score; }
    int32 GetPlayerScore() const { return PlayerScore; }

    /** 플레이어 이름 설정 */
    void SetPlayerName(const FString& Name) { PlayerName = Name; }
    FString GetPlayerName() const { return PlayerName; }

    /** 구출한 사람 수 */
    void SetRescuedCount(int32 Count) { RescuedCount = Count; }
    int32 GetRescuedCount() const { return RescuedCount; }

    /** 플레이어 상태 초기화 */
    void ResetPlayerData();

    // ════════════════════════════════════════════════════════════════════════
    // 범용 키-값 저장소 (Lua 친화적)

    /** 정수 값 저장 */
    void SetInt(const FString& Key, int32 Value);
    int32 GetInt(const FString& Key, int32 DefaultValue = 0) const;

    /** 실수 값 저장 */
    void SetFloat(const FString& Key, float Value);
    float GetFloat(const FString& Key, float DefaultValue = 0.0f) const;

    /** 문자열 값 저장 */
    void SetString(const FString& Key, const FString& Value);
    FString GetString(const FString& Key, const FString& DefaultValue = "") const;

    /** 불리언 값 저장 (내부적으로 Int 사용) */
    void SetBool(const FString& Key, bool Value) { SetInt(Key, Value ? 1 : 0); }
    bool GetBool(const FString& Key, bool DefaultValue = false) const { return GetInt(Key, DefaultValue ? 1 : 0) != 0; }

    /** 키 존재 여부 확인 */
    bool HasKey(const FString& Key) const;

    /** 모든 데이터 초기화 */
    void ClearAll();

    /** 저장된 모든 데이터 로그 출력 */
    void PrintDebugInfo() const;

private:
    // 아이템 저장소 (Key: 아이템 태그명, Value: 아이템 수)
    TMap<FString, int32> CollectedItems;

    // 플레이어 상태
    uint32 PlayerHealth = 100;
    uint32 PlayerScore = 0;
    uint32 RescuedCount = 0;
    FString PlayerName;

    // 범용 키-값 저장소
    TMap<FString, uint32> IntData;
    TMap<FString, float> FloatData;
    TMap<FString, FString> StringData;
};
