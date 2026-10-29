#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CozyRealmDataTypes.h"
#include "Estate/CozyEstateState.h"
#include "Estate/CozyEstateSaveIO.h"
#include "CozyEstateSubsystem.generated.h"

class UDataTable;
class ACozyFacilityActor;

/** bStructural: 배치·재료처럼 구조가 바뀌면 true, 시간만 흐르면 false */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyEstateChanged, bool /*bStructural*/);

/**
 *  영지 일 알림 하나 (소리·떠오르는 숫자·강조 효과용 · 출시 수정 2차 2026-10-30).
 *  플레이어 행동과 켜 둔 동안의 진행에서만 보냄 · 방치 정산(꺼 둔 동안·절전 복귀) 중에는 보내지 않음
 */
struct FCozyEstateEvent
{
	ECozyEstateEventKind Kind = ECozyEstateEventKind::Harvest;
	/** 일이 일어난 시설 고유 ID (없으면 빈 ID · 예: 바닥 칠하기) */
	FGuid FacilityId;
	/** 시설 정의 ID */
	FName DefinitionId;
	/** 품목 (수확·가공 = 완료품 · 판매 = 받은 재화 · 구매 = 장식·바닥 ID) */
	FName ItemId;
	/** 수량 (옮긴 개수 · 받은 재화 · 칸 수) */
	int32 Amount = 0;
	/** 업그레이드 완료면 새 레벨 */
	int32 Level = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyEstateEvent, const FCozyEstateEvent&);

/** 밤 판정 디버그 덮어쓰기 */
UENUM(BlueprintType)
enum class ECozyNightOverride : uint8
{
	/** 게임 시계(현실 시간에서 계산 · 하루 GameDaySeconds)로 판정 */
	Auto,
	ForceDay,
	ForceNight
};

/** 시설 하나의 생산 상태 (UI가 읽기만 하는 값) */
struct FCozyProductionView
{
	bool bHasProduction = false;
	bool bWorking = false;
	float Progress01 = 0.f;
	float RemainingSeconds = 0.f;
	FText Status;
	/** 이 시설의 미수령 생산물 합계 / 한도 */
	int32 UnclaimedAmount = 0;
	int32 UnclaimedCapacity = 0;
	/** 미수령 생산물 표시용 재료 이름 */
	FText UnclaimedItemName;
	FText CollectButtonLabel;
	/** 그 생산물의 공용 창고 보유량 / 한도 (미수령분은 포함하지 않음) */
	int32 StoredAmount = 0;
	int32 StorageCap = 0;
	/** 지금 수령하면 창고로 옮겨질 수량 = min(미수령, 창고 남은 공간) */
	int32 CollectableNow = 0;
	/** 공통 성장 효과로 받는 생산 속도 배율 (D39 · 진행 중인 주기에는 다음 주기부터 적용 · D40) */
	float SpeedMultiplier = 1.f;
	/** 진행 중인 주기의 길이 (초 · 시작할 때 정해짐) */
	float CycleSeconds = 0.f;
	/** 성장 효과를 주는 관리 시설 이름 · 레벨 (관리 시설이 정해지지 않은 시설이면 비어 있음 · 레벨 0이면 아직 없음) */
	FText GrowthSourceName;
	int32 GrowthSourceLevel = 0;
	bool bHasGrowthSource = false;
};

/** 레시피 견적: 선택한 횟수로 시작할 수 있는지 · 최대 횟수 · 총 재료·완료품·시간 (공통 가공 창이 읽음 · D32) */
struct FCozyRecipeQuote
{
	/** 이 시설의 레시피가 맞는가 */
	bool bValid = false;
	bool bCanStart = false;
	/** 선택한 실행 횟수 */
	int32 Runs = 0;
	/** 지금 고를 수 있는 최대 횟수 = min(재료, 미수령 공간) · 품목·칸·주민 조건이 안 맞으면 0 */
	int32 MaxRuns = 0;
	int32 MaxByMaterials = 0;
	int32 MaxBySpace = 0;
	int32 OutputPerRun = 0;
	int32 TotalOutput = 0;
	double SecondsPerRun = 0.0;
	double TotalSeconds = 0.0;
	/** 재료 ID · 선택 횟수 기준 총 필요량 · 지금 보유량 */
	struct FInput { FName ItemId; int32 Need = 0; int32 Have = 0; };
	TArray<FInput> Inputs;
	FText OutputName;
	/** 시작할 수 없는 이유 (시작 가능하면 비어 있음) */
	FText BlockReason;
	/** 같은 완료품을 만드는 작업이 칸을 모두 쓰고 있어 '제작 추가'(앞 작업 뒤에 이어서)로 넣는 경우 */
	bool bAppend = false;
	/** 같은 완료품 작업들의 아직 남은 회차 (진행 중 + 대기) · '제작 추가' 안내용 */
	int32 CurrentRemainingRuns = 0;
};

/** 진행·일시 정지 중인 가공 작업 하나 (가공 창 표시용) */
struct FCozyProcessingJobView
{
	FGuid JobId;
	FText OutputName;
	int32 CompletedRuns = 0;
	int32 TotalRuns = 0;
	int32 OutputPerRun = 0;
	bool bPaused = false;
	/** 앞 작업 뒤에서 기다리는 추가 제작 (가공 칸을 차지하지 않음) */
	bool bQueued = false;
	float RunProgress01 = 0.f;
	float RunRemainingSeconds = 0.f;
	float TotalRemainingSeconds = 0.f;
	FText Status;
};

/** 시설의 미수령 보관 상태 (생산·가공 공통 · D31) */
struct FCozyUnclaimedView
{
	/** 지금 이 시설이 맡고 있는 품목 (미수령분 또는 진행 작업의 완료품) · 없으면 NAME_None */
	FName ItemId;
	FText ItemName;
	int32 Amount = 0;
	int32 Capacity = 0;
	/** 진행·일시 정지 중인 가공 작업이 확보해 둔 공간 (남은 회차 × 1회 개수) */
	int32 Reserved = 0;
	/** 그 품목의 공용 창고 보유량 / 한도 (미수령분은 포함하지 않음) */
	int32 StoredAmount = 0;
	int32 StorageCap = 0;
	/** 지금 수령하면 창고로 옮겨질 수량 = min(미수령, 창고 남은 공간) */
	int32 CollectableNow = 0;
};

/** 판매 견적: 고른 재료·수량으로 팔 수 있는지 · 받을 재화 (판매소 창이 읽음) */
struct FCozySellQuote
{
	bool bCanSell = false;
	int32 Amount = 0;
	/** 지금 팔 수 있는 최대 수량 = 창고 보유량 (미수령분은 포함하지 않음) */
	int32 MaxAmount = 0;
	int32 UnitPrice = 0;
	int32 TotalPrice = 0;
	FText ItemName;
	FText CurrencyName;
	/** 팔 수 없는 이유 (팔 수 있으면 비어 있음) */
	FText BlockReason;
};

/** 업그레이드 견적 (후신소 창이 읽음 · D9·D25·D36·D38) */
struct FCozyUpgradeQuote
{
	/** 이 시설에 다음 단계 성장 행이 있는가 */
	bool bValid = false;
	bool bCanStart = false;
	bool bUpgrading = false;
	FName GrowthRowId;
	int32 FromLevel = 0;
	int32 ToLevel = 0;
	double Seconds = 0.0;
	struct FAmount { FName ItemId; int32 Amount = 0; int32 Have = 0; };
	/** 시작 비용 */
	TArray<FAmount> Costs;
	/** D36: 종료되는 가공의 미완료 회차 재료 (반환) */
	TArray<FAmount> Refunds;
	/** D36: 종료되는 가공 설명 (완성된 회차는 미수령분에 남음) */
	TArray<FText> EndingJobs;
	/** 이 단계가 끝나면 해금되는 작물 이름 (D37) */
	TArray<FText> UnlockCropNames;
	/** 이 단계가 끝나면 해금되는 시설 이름 */
	TArray<FText> UnlockFacilityNames;
	/** 조건 하나 (UI의 ✅ 표시와 '이동' 버튼용) · FacilityId가 있으면 그 시설로 이동할 수 있음 */
	struct FCondition { FText Label; bool bMet = false; FName FacilityId; int32 Current = 0; };
	/** 신사 상한 · 선행 시설 조건 (데이터 순서) */
	TArray<FCondition> Conditions;
	/** 관리 시설이면 완료 후 모든 밭의 속도 배율 (0이면 해당 없음 · D39) */
	float NextFieldSpeedMultiplier = 0.f;
	/** 레벨 효과 안내 (예: '동시 가공 1 → 2개' · '들일 수 있는 주민 3 → 4명' · 출시 수정 2차) */
	TArray<FText> EffectLines;
	/** 시작할 수 없는 이유 (시작 가능하면 비어 있음) */
	FText BlockReason;
};

/** 시간 단축 미리보기 (D10~D12 · 확정 전에 보여 주고, 확정할 때 다시 계산) */
struct FCozySpeedupQuote
{
	bool bValid = false;
	bool bCanApply = false;
	int32 Count = 0;
	int32 Owned = 0;
	/** 남은 시간을 다 줄이는 데 필요한 최소 장수 (더 쓰면 초과분은 버려짐) */
	int32 MaxUseful = 0;
	float SecondsPerItem = 0.f;
	float RemainingBefore = 0.f;
	float Reduce = 0.f;
	float RemainingAfter = 0.f;
	/** 남은 시간보다 많이 써서 버려지는 시간 (D12) */
	float Wasted = 0.f;
	FText BlockReason;
};

/** 방치 보상 결과 (게임을 꺼 둔 동안 정산한 내용 · 팝업 표시용) */
struct FCozyOfflineReport
{
	/** 꺼 둔 실제 시간 */
	double AwaySeconds = 0.0;
	/** 정산한 시간 (최대 12시간) */
	double AppliedSeconds = 0.0;
	bool bClamped = false;
	TArray<FText> Lines;
};

/** 진행 중인 업그레이드 하나 (후신소 창 표시용) */
struct FCozyUpgradeJobView
{
	FGuid JobId;
	FGuid FacilityId;
	FText FacilityName;
	int32 ToLevel = 0;
	float Progress01 = 0.f;
	float RemainingSeconds = 0.f;
};

/** 밭 관리 시설 상태 (관리 창 표시용 · D37·D39) */
struct FCozyFieldManagementView
{
	int32 Level = 0;
	float CurrentMultiplier = 1.f;
	/** 다음 단계 배율 (0이면 다음 단계 없음) */
	float NextMultiplier = 0.f;
	TArray<FText> UnlockedCrops;
	TArray<FText> NextUnlockCrops;
	int32 ManagedFacilities = 0;
	bool bUpgrading = false;
};

/** 꾸미기 장식 구매 견적 (꾸미기 상점 창이 읽음 · 상태를 바꾸지 않음) */
struct FCozyDecorationQuote
{
	/** Decorations.csv에 있는 장식인가 */
	bool bValid = false;
	bool bCanBuy = false;
	FText Name;
	FIntPoint Size = FIntPoint(1, 1);
	int32 Points = 0;
	struct FAmount { FName ItemId; int32 Amount = 0; int32 Have = 0; };
	/** 1개 가격 (재화 ID · 개수 · 지금 보유량) */
	TArray<FAmount> Costs;
	/** 영지에 놓인 개수 · 보관함에 있는 개수 */
	int32 Placed = 0;
	int32 Stored = 0;
	/** 살 수 없는 이유 (살 수 있으면 비어 있음) */
	FText BlockReason;
};

/** 꾸미기 점수 상태 (점수 · 보너스 · 다음 단계까지) */
struct FCozyDecorationScoreView
{
	/** 영지에 놓인 장식 점수 합계 (보관함 장식은 제외) */
	int32 Score = 0;
	/** 지금 생산·가공 속도 보너스 (0.05 = +5%) */
	float Bonus = 0.f;
	float MaxBonus = 0.f;
	/** 다음 보너스 단계까지 필요한 점수 (최대 보너스면 0) */
	int32 PointsToNext = 0;
	bool bMaxed = false;
};

/** 바닥 칠하기 견적 (꾸미기 상점 '바닥' 줄 · 칠하기 직전 확인) */
struct FCozyFloorQuote
{
	bool bValid = false;
	bool bCanPay = false;
	FText Name;
	ECozyFloorTool Tool = ECozyFloorTool::Brush;
	int32 Style = 0;
	int32 Cells = 0;
	struct FAmount { FName ItemId; int32 Amount = 0; int32 Have = 0; };
	/** Cells칸 비용 (재화 ID · 총 개수 · 지금 보유량) */
	TArray<FAmount> Costs;
	FText BlockReason;
};

/** 수령 결과 (UI가 메시지로 보여 줌) */
struct FCozyCollectResult
{
	int32 Moved = 0;
	int32 Remaining = 0;
	FText Message;
};

/**
 *  영지 공통 서비스 (정리 6-2): 게임 데이터 · 영지 상태 · 재료·재화 · 작업 기록과 게임 시간.
 *  UI와 시설 액터는 이 서비스의 상태를 읽고 요청만 보낸다 (6-7).
 */
/** 타이틀 화면이 보는 저장 상태 */
UENUM()
enum class ECozySaveProbe : uint8
{
	None,
	Valid,
	Recovered,
	Invalid
};

UCLASS()
class UCozyEstateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	// --- 시작 ---

	/** CSV 데이터를 읽고 새 게임 상태를 만든 뒤 시설 액터를 놓는다 (게임 모드가 호출) */
	void StartEstate();

	/** 데이터를 다시 검사해 로그에 경고를 남긴다 (디버그 메뉴) · 문제 개수는 로그 끝줄에 */
	void ValidateData() const;
	/**
	 *  쓸 곳(레시피 재료·업그레이드 비용·장식 가격)도 판매가도 없는 재료 (시험용 레시피는 셈하지 않음 · 데이터 검사가 경고).
	 *  OutDeadEnd = 데모에서 만들 수 있는데 쓸 곳이 없음(진행이 막힘) · OutUnused = 만드는 곳도 없음
	 */
	void FindUnusableMaterials(TArray<FName>& OutDeadEnd, TArray<FName>& OutUnused) const;
	/** 이 시설 정의가 데이터상 오를 수 있는 최대 레벨 (성장 단계가 없으면 1) */
	int32 GetMaxFacilityLevel(FName DefinitionId) const;
	/**
	 *  이 시설이 최대 속도(성장 최대 단계 × 꾸미기 최대 보너스)로 방치 최대 시간 동안 만드는 양 (시험용 레시피 제외).
	 *  🙋 미수령 한도는 이 값 이상이어야 방치 보상 최대 12시간치가 전부 쌓인다 (데이터 검사·자동 시험이 확인)
	 */
	int32 GetMaxOfflineOutput(FName DefinitionId) const;
	/** 진행 검사 (7-6): 성장 단계를 레벨·조건만 따라가며 끝까지 도달할 수 있는지 · 단계 빈칸·중복·상한·달성 불가·순환 */
	void ValidateProgression(TFunctionRef<void(const FString&)> Warn) const;

	// --- 데이터 조회 ---

	const FCozyFacilityRow* GetFacilityDef(FName Id) const;
	const FCozyCropRow* GetCropDef(FName Id) const;
	const FCozyItemRow* GetItemDef(FName Id) const;
	const FCozyResidentRow* GetResidentDef(FName Id) const;
	const FCozyEstateConfigRow& GetConfig() const { return Config; }

	/** HUD에 표시할 재료·재화 ID (HudOrder 순) */
	TArray<FName> GetHudItems() const;

	// --- 상태 조회 ---

	const FCozyEstateState& GetState() const { return State; }
	const FCozyFacilityState* FindFacility(const FGuid& Id) const;
	const FCozyResidentState* FindResident(const FGuid& Id) const;
	const FCozyJobRecord* FindJob(const FGuid& Id) const;
	FText GetFacilityDisplayName(const FGuid& FacilityId) const;
	FText GetResidentDisplayName(const FGuid& ResidentId) const;

	/** 시설의 생산 진행 상태 */
	FCozyProductionView GetProductionView(const FGuid& FacilityId) const;

	// --- 재료·재화 ---

	int32 GetAmount(FName ItemId) const;
	/** 보관 가능한 만큼만 더하고 실제로 더한 개수를 돌려줌 */
	int32 AddResource(FName ItemId, int32 Amount);

	/** 공용 창고에 이 재료를 더 받을 수 있는 수량 (재화는 한도 없음 → MAX_int32) */
	int32 GetStorageSpace(FName ItemId) const;

	/** 창고 화면에 보여 줄 재료·재화 ID (재료 먼저, 표 순서) */
	TArray<FName> GetStorageItems() const;

	// --- 미수령 생산물 ---

	/** 시설의 미수령 생산물을 공용 창고로 옮긴다 · 창고에 들어갈 만큼만 옮기고 나머지는 시설에 남김 */
	FCozyCollectResult CollectUnclaimed(const FGuid& FacilityId);
	/** Space 전부 수확: 모든 자동 생산 시설의 미수령분을 같은 수령 규칙으로 (D27) · 결과 한 줄 */
	FText CollectAllProduction();

	/** 시설의 미수령 보관 상태 (품목 · 수량 · 한도 · 확보 공간 · 창고 상태) */
	FCozyUnclaimedView GetUnclaimedView(const FGuid& FacilityId) const;

	/**
	 *  이 시설이 ItemId 완료품을 새로 맡을 수 있는가 (생산·가공 공통 · D31).
	 *  다른 품목이 미수령으로 남아 있거나 다른 품목을 만드는 작업이 진행 중이면 false + 이유.
	 *  밭 작물 변경도 이 검사를 쓴다 (작물 선택 화면을 만들 때 연결).
	 */
	bool CanAcceptOutputItem(const FGuid& FacilityId, FName ItemId, FText& OutReason) const;

	// --- 공통 가공 (D28~D33) ---

	const FCozyRecipeRow* GetRecipeDef(FName Id) const;

	/** 이 시설에서 쓸 수 있는 레시피 ID (데이터 순서) · 테스트 레시피는 '테스트 레시피 보이기'를 켰을 때만 */
	TArray<FName> GetFacilityRecipes(const FGuid& FacilityId) const;

	/** 선택한 횟수의 견적 · 최대 횟수와 시작할 수 없는 이유까지 계산 (상태를 바꾸지 않음) */
	FCozyRecipeQuote GetRecipeQuote(const FGuid& FacilityId, FName RecipeId, int32 Runs) const;

	/**
	 *  가공 시작: 조건 재확인 → 선택 횟수 전체 재료 차감 → 작업 등록을 한 번에 처리.
	 *  실패하면 아무것도 바꾸지 않고 이유를 돌려줌 · 수량을 임의로 줄여 시작하지 않음.
	 */
	bool StartProcessing(const FGuid& FacilityId, FName RecipeId, int32 Runs, FText& OutMessage);

	/** 유저가 직접 취소: 완성된 회차는 미수령분에 그대로, 미완료 회차는 재료 반환·완료품 없이 종료 · 확보 공간 해제 (D29·D33) */
	bool CancelProcessing(const FGuid& JobId, FText& OutMessage);

	/** 시설의 가공 작업 목록 (시작한 순서) · 여러 가공 칸이 생겨도 같은 함수 */
	TArray<FCozyProcessingJobView> GetProcessingJobs(const FGuid& FacilityId) const;

	bool GetShowTestRecipes() const { return bShowTestRecipes; }
	void SetShowTestRecipes(bool bShow);
	/** UI 미리보기 중 자동 저장 멈춤 (미리보기는 게임 상태를 바꾸지 않지만, 확인하는 동안 저장 파일도 건드리지 않게) */
	void SetAutosavePaused(bool bPaused) { bAutosavePaused = bPaused; }

	// --- 공통 성장 · 업그레이드 (후신소 · D9·D25·D36~D40) ---

	/** 업그레이드할 수 있는 시설 (성장 설정표에 행이 있는 시설 · 놓인 순서) */
	TArray<FGuid> GetUpgradableFacilities() const;

	/** 업그레이드 견적 (상태를 바꾸지 않음) */
	FCozyUpgradeQuote GetUpgradeQuote(const FGuid& FacilityId) const;

	/**
	 *  업그레이드 시작: 조건 재확인 → 비용 차감 + (가공 시설이면) 가공 종료·미완료 재료 반환·예약 공간 해제 + 작업 등록을 한 번에.
	 *  실패하면 아무것도 바꾸지 않음 (D36·D38).
	 */
	bool StartUpgrade(const FGuid& FacilityId, FText& OutMessage);

	TArray<FCozyUpgradeJobView> GetUpgradeJobs() const;

	// --- 시간 단축 (D10~D12 · 업그레이드 작업만) ---

	/** 부적 Count장을 쓸 때의 결과 미리보기 */
	FCozySpeedupQuote GetSpeedupQuote(const FGuid& JobId, int32 Count) const;
	/** 확정: 다시 계산해 부적 차감 + 남은 시간 단축을 한 번에 · 실패하면 아무것도 바뀌지 않음 */
	bool ApplySpeedup(const FGuid& JobId, int32 Count, FText& OutMessage);

	// --- 첫 실행 안내 (Tutorial.csv · 완료 여부는 영지 저장 상태에) ---

	/** 안내 단계 (행 이름 순서) */
	TArray<const FCozyTutorialRow*> GetTutorialSteps() const;
	/** 이 영지에서 안내를 끝냈거나 건너뛰었는가 (새 게임이면 false · 저장 파일에 남음) */
	bool IsTutorialDone() const { return State.bTutorialDone; }
	void SetTutorialDone(bool bDone);

	// --- 일회성 보상 (정리 7-11) ---

	/** 이미 받은 보상인가 */
	bool HasGrantedReward(FName RewardId) const;
	/** 보상을 한 번만 지급 (지급 기록은 상태에 남아 저장된다) · 이미 받았거나 없는 보상이면 false */
	bool GrantOneTimeReward(FName RewardId, FText& OutMessage);
	/** 동시에 진행할 수 있는 업그레이드 수 (후신소 데이터 · UpgradeSlots + SlotBonusLevels 중 닿은 레벨 수) */
	int32 GetUpgradeSlotCount() const;
	/** 이 가공 시설의 동시 가공 칸 수 (ProcessingSlots + SlotBonusLevels 중 닿은 레벨 수 · 출시 수정 2차) */
	int32 GetProcessingSlotCount(const FGuid& FacilityId) const;
	/** 영지에 들일 수 있는 주민 수 (영지에 놓인 숙소 시설의 ResidentCapacity + 레벨당 증가 · 숙소가 없으면 0) */
	int32 GetResidentCapacity() const;
	/** 숙소 한 곳의 주민 한도 (레벨 기준 · 숙소가 아니면 0) */
	static int32 ComputeResidentCapacity(const FCozyFacilityRow& Def, int32 Level);
	/** SlotBonusLevels 중 Level 이하인 개수 (레벨로 늘어난 칸 수) */
	static int32 CountSlotBonus(const FCozyFacilityRow& Def, int32 Level);

	// --- 데모 끝 (신사 최대 레벨 · 출시 수정 2차) ---

	/** 데모의 마지막 신사 레벨 (성장 표의 신사 최대 레벨 · 신사가 없으면 0) */
	int32 GetDemoFinalShrineLevel() const;
	/** 신사가 데모 마지막 레벨에 닿았고 아직 '데모는 여기까지' 안내를 보여 주지 않았는가 */
	bool ShouldShowDemoEnd() const;
	/** 안내를 보여 줬다고 기록 (영지 저장 상태에 남아 다시 띄우지 않음) */
	void MarkDemoEndShown();
	bool IsFacilityUpgrading(const FGuid& FacilityId) const;
	/** 해금된 시설인가 (이미 놓여 있음 또는 해금 단계를 지남 · 건설 기능에서 사용 · 따로 저장하지 않음) */
	bool IsFacilityUnlocked(FName DefinitionId) const;
	/** 신사 레벨 (신사가 없으면 0) */
	int32 GetShrineLevel() const;

	/** 밭 관리 시설 상태 */
	FCozyFieldManagementView GetFieldManagementView(const FGuid& ManagerFacilityId) const;

	// --- 작물 선택 (개별 밭 · D37) ---

	/** 해금된 작물인가 (처음부터 열림 또는 관리 시설 단계로 해금) */
	bool IsCropUnlocked(FName CropId) const;
	/** 이 작물을 여는 성장 단계가 데이터에 있는가 (없고 처음부터 열린 작물도 아니면 선택 목록에 보이지 않음) */
	bool HasCropUnlockPath(FName CropId) const;
	/** 작물 선택 창에 보여 줄 작물 (해금됨 · 해금 단계가 있는 잠긴 작물 · 지금 키우는 작물 · 데이터 순서) */
	TArray<FName> GetCropChoices(const FGuid& FacilityId) const;
	/** 잠긴 작물의 해금 안내 (예: '밭 관리 시설 Lv2에서 해금') */
	FText GetCropUnlockHint(FName CropId) const;
	/** 이 밭의 작물을 지금 바꿀 수 있는가 · 미수령분·진행 주기가 있으면 불가 (D31) */
	bool CanSelectCrop(const FGuid& FacilityId, FName CropId, FText& OutReason) const;
	bool SelectCrop(const FGuid& FacilityId, FName CropId, FText& OutMessage);

	// --- 판매 (판매소 · 주민 없이 기본 작동 · D4·D14) ---

	/** 판매 목록에 보일 재료 ID (창고 재료 · 데이터 순서) · 판매가 0인 재료는 '판매 불가'로 표시 */
	TArray<FName> GetSaleListItems() const;

	/** 판매 견적 (상태를 바꾸지 않음) */
	FCozySellQuote GetSellQuote(const FGuid& ShopFacilityId, FName ItemId, int32 Amount) const;

	/** 판매: 조건 재확인 → 창고에서 재료 차감 → 재화 지급을 한 번에 · 실패하면 아무것도 바뀌지 않음 */
	bool SellItem(const FGuid& ShopFacilityId, FName ItemId, int32 Amount, FText& OutMessage);

	// --- 꾸미기 상점 · 꾸미기 점수 (4주차 기능 2) ---
	// 🤖 장식은 '기능이 없는 시설'로 다룬다: Decorations.csv 행을 시설 정의로도 등록해
	//    배치 모드(옮기기·회전·보관)·칸 겹침·저장·지형 재확인을 시설과 같은 코드로 처리한다.

	const FCozyDecorationRow* GetDecorationDef(FName Id) const;
	/** 이 시설 정의가 꾸미기 장식인가 */
	bool IsDecoration(FName DefinitionId) const { return GetDecorationDef(DefinitionId) != nullptr; }
	/** 상점 목록 (Decorations.csv 순서) */
	TArray<FName> GetDecorationCatalog() const;
	/** 구매 견적 (상태를 바꾸지 않음) */
	FCozyDecorationQuote GetDecorationQuote(FName DecorationId) const;
	/** 구매: 다시 확인 → 가격 차감 → 보관함에 1개 추가를 한 번에 · 실패하면 아무것도 바뀌지 않음 (🤖 산 장식은 보관함으로 · 배치 모드에서 꺼내 놓음) */
	bool BuyDecoration(FName DecorationId, FText& OutMessage);
	/** 영지에 놓인(보관하지 않은) 장식 점수 합계 */
	int32 GetDecorationScore() const;
	FCozyDecorationScoreView GetDecorationScoreView() const;
	/** 점수 → 보너스 (🙋 PointsPerStep점마다 BonusPerStep · 최대 BonusMax) · 순수 계산이라 시험에서 바로 씀 */
	static float ComputeDecorationBonus(int32 Score, int32 PointsPerStep, float BonusPerStep, float BonusMax);
	// --- 바닥 꾸미기 (Floors.csv · 꾸미기 상점 '바닥' 묶음 · 칸당 비용 · 🙋 2026-10-30 상점에 합치기로 결정) ---

	const FCozyFloorRow* GetFloorDef(FName FloorId) const;
	/** 상점 '바닥' 목록 (Floors.csv 순서) */
	TArray<FName> GetFloorCatalog() const;
	/** 도구·종류 번호로 바닥 ID (없으면 NAME_None = 비용 없음) */
	FName FindFloorId(ECozyFloorTool Tool, int32 Style) const;
	/** Cells칸 칠하는 견적 (상태를 바꾸지 않음) */
	FCozyFloorQuote GetFloorQuote(FName FloorId, int32 Cells) const;
	/** Cells칸 비용을 확인 → 차감을 한 번에 · 부족하면 아무것도 바꾸지 않고 false (지우기는 부르지 않음 = 무료·환불 없음) */
	bool PayFloorCost(FName FloorId, int32 Cells, FText& OutMessage);
	/** 원(지면 붓)이 걸치는 칸 (영지 밖 칸 포함 · 칸당 비용 계산용) */
	TArray<FIntPoint> GetCellsInCircle(const FVector& Center, float Radius) const;

	/** 걸어서 지나갈 수 있는 시설·장식인가 (🤖 돌길처럼 bWalkable인 장식만) */
	bool IsWalkableFacility(const FCozyFacilityState& Facility) const;
	/**
	 *  칸 겹침 검사 (시설·장식 공통 · 자기 자신과 보관한 것은 제외).
	 *  GroundZ = 놓을 자리의 지면 높이 · 층이 다른 시설(높이 차가 큰 것)은 겹치지 않는 것으로 봄 · 겹치면 상대 ID.
	 */
	bool FindPlacementOverlap(const FGuid& FacilityId, FIntPoint Coord, int32 Rotation, float GroundZ, FGuid& OutOther) const;

	// --- 주민 배치 ---

	/** 주민을 시설에 배치한다 · 다른 시설에 있으면 옮긴다 · 실패하면 이유를 돌려줌 (나가야 창이 호출) */
	bool AssignResident(const FGuid& ResidentId, const FGuid& FacilityId, FText& OutFailReason);

	/** 주민을 시설에서 빼서 나가야로 돌려보낸다 */
	bool UnassignResident(const FGuid& ResidentId, FText& OutFailReason);

	/** 이 시설에 지금 주민을 더 배치할 수 있는가 */
	bool CanAcceptResident(const FGuid& FacilityId, FText& OutFailReason) const;

	// --- 시간 ---

	double GetGameSeconds() const { return State.GameSeconds; }
	float GetTimeScale() const { return TimeScale; }
	void SetTimeScale(float NewScale);
	bool IsNight() const;
	/** 게임 시계의 지금 시각 (0~24시 · 소수 = 분) · 현실 시각에서 계산하므로 게임을 꺼 둔 동안에도 흐름 · 디버그 배속만큼 빨라짐 */
	double GetDayHour() const;
	/** 밤이 시작·끝나는 게임 시각 (시) */
	int32 GetNightStartHour() const { return Config.NightStartHour; }
	int32 GetNightEndHour() const { return Config.NightEndHour; }
	ECozyNightOverride GetNightOverride() const { return NightOverride; }
	void SetNightOverride(ECozyNightOverride NewOverride);

	// --- 디버그 ---

	void DebugAddResource(FName ItemId, int32 Amount);
	/** 재료 보유량을 정해진 값으로 맞춘다 (수령 검증용) */
	void DebugSetResource(FName ItemId, int32 Amount);
	/** 테스트용으로 같은 종류 시설을 빈 칸에 하나 더 놓는다 (추가 건설 규칙과 무관한 디버그 기능) */
	bool DebugAddFacility(FName DefinitionId);
	/** 상태를 지우고 시작 설정으로 새 게임을 다시 만든다 (저장 파일도 새 게임으로 덮어씀) */
	void DebugRestartNewGame();
	/** 자동 시험용: CSV만 읽고 새 게임 상태를 만든다 (액터·저장 없음 · bStarted는 그대로 false) */
	bool DebugInitDataOnly(FString& OutErrors);
	/** 자동 시험용: 상태를 통째로 바꾼다 (액터·저장 없음 · 불러오기와 같은 정리 SanitizeLoadedState 적용) */
	void DebugSetState(const FCozyEstateState& NewState);
	/** 자동 시험용: 시설 레벨을 그대로 바꾼다 (데이터 상한 무시 · 불러오기 정리 없음 · 데모 밖 레벨 효과 시험) */
	void DebugSetFacilityLevel(const FGuid& FacilityId, int32 Level);
	/** 디버그: 꾸미기 점수·보너스·장식 목록을 로그와 한 줄 글로 */
	FString DebugDescribeDecorations() const;

	// --- 배치 모드 (3주차 기능 2 · D45) ---

	/** 회전을 반영한 차지 칸 수 */
	FIntPoint GetFootprint(const FGuid& FacilityId, int32 Rotation) const;
	/** 영지 안 · 다른 시설과 겹치지 않음 (자기 자신은 제외) */
	bool CanPlaceFacility(const FGuid& FacilityId, FIntPoint Coord, int32 Rotation, FText& OutReason) const;
	/** 보관 가능: 핵심 시설 아님 · 진행·일시 정지 작업 없음 · 미수령분 없음 */
	bool CanStoreFacility(const FGuid& FacilityId, FText& OutReason) const;
	/** 보관: 배치 주민은 나가야로 · 레벨·외형·밭 작물 유지 · 실패하면 아무것도 바뀌지 않음 */
	bool StoreFacility(const FGuid& FacilityId, FText& OutMessage);
	TArray<FGuid> GetStoredFacilities() const;

	/** 옮기기·꺼내기 미리보기 시작 (보관함 시설이면 빈 자리에 임시 상자) */
	bool BeginPlacement(const FGuid& FacilityId);
	/** 미리보기 위치·회전 (영지 밖으로 나가지 않게 맞춤) · 초록/빨강 표시 */
	void UpdatePlacement(FIntPoint Coord, int32 Rotation);
	void RotatePlacement();
	/** 확정: 다시 검사한 뒤 위치·회전 저장 (진행 작업·예약 공간은 그대로) */
	bool ConfirmPlacement(FText& OutMessage);
	/** 취소: 원래 자리로 (보관함에서 꺼낸 것이면 다시 보관함) */
	void CancelPlacement();
	bool IsPlacing() const { return PlacementId.IsValid(); }
	const FGuid& GetPlacementId() const { return PlacementId; }
	bool IsPlacementFromStorage() const { return bPlacementFromStorage; }
	FIntPoint GetPlacementCoord() const { return PlacementCoord; }
	int32 GetPlacementRotation() const { return PlacementRotation; }
	FGuid GetPlacementSurface() const { return PlacementSurfaceId; }
	void SetPlacementSurface(FGuid Id) { PlacementSurfaceId = Id; }
	ACozyFacilityActor* GetPlacementActor() const { return PlacementActor; }
	/** 월드 위치 → 칸 (영지 밖이어도 계산) */
	FIntPoint WorldToCell(const FVector& WorldLocation) const;

	// --- 주인공 이동 (4주차 기능 1) ---

	FIntPoint GetGridSize() const { return Config.GridSize; }
	float GetCellSize() const { return Config.CellSize; }
	/** 칸 가운데의 월드 위치 (바닥 높이) */
	FVector GetCellCenter(FIntPoint Cell) const;
	/** 걸을 수 있는 칸: 영지 안 · 놓인 시설이 차지하지 않음 (보관한 시설은 제외) */
	bool IsCellWalkable(FIntPoint Cell) const;
	/** 장식 타일 설치·교체·제거. 시설 점유와 별도로 관리한다. */
	bool EditFloorTile(FVector Position, int32 Style, int32 Rotation, bool bErase, FText& Reason, const FGuid* Surface = nullptr, const FGuid& MovingId = FGuid());
	bool StoreFloorTile(const FGuid& Id, FText& Reason);
	const FCozyFloorTile* FindFloorTile(FVector Position, const FGuid& Surface) const;
	/** 위치에서 가장 가까운 시설 (시설이 차지한 칸 테두리까지 거리가 Range 이하 · 없으면 빈 ID) */
	FGuid FindNearestFacility(const FVector& Location, float Range) const;

	// --- 저장 / 불러오기 (2주차 기능 3 · 슬롯 1개 · 자동 저장 1분마다 + 종료 시) ---

	/** 지금 상태를 저장 (Reason은 로그용) */
	bool SaveEstate(const FString& Reason);
	/**
	 *  곧 저장해 달라는 요청 (판매·가공 시작·업그레이드·부적·구매 등 · 자동 저장 1분을 기다리지 않음).
	 *  EstateConfig SaveRequestDelaySeconds 동안 들어온 요청을 묶어 한 번만 저장 · 영지를 시작하지 않았으면 무시
	 */
	void RequestSave(const TCHAR* Reason);
	/** 종료·검증처럼 디스크 반영 완료가 필요한 지점에서만 사용한다. */
	void FlushPendingSave() { PollSave(true); }
	bool HasSaveFile() const;
	/** 영지를 시작했는지 (StartEstate 이후) · 타이틀처럼 영지를 시작하지 않은 맵에서는 저장하지 않는다 */
	bool IsStarted() const { return bStarted; }
	/** 이번 실행에서 새 게임으로 시작했는가 (저장이 없었거나 새 게임 · 첫 실행 안내를 띄울지 판단) */
	bool IsNewGameSession() const { return bNewGameSession; }
	/**
	 *  저장 파일을 읽기만 해서 상태를 알려준다 (파일을 만들거나 고치지 않음 · 타이틀 화면용).
	 *  Valid = 이어하기 가능 · Recovered = 본 파일은 못 읽지만 직전 정상본으로 이어짐 · Invalid = 손상·미래 버전(덮어쓰지 않음)
	 */
	static ECozySaveProbe ProbeSaveFile(FDateTime& OutSavedUtc, FString& OutDetail);
	/**
	 *  새 게임: 지금 저장 파일들을 SaveGames/Backup_NewGame_<UTC> 폴더로 옮긴다 (삭제하지 않음).
	 *  옮기지 못하면 false (아무것도 바꾸지 않은 상태로 둠).
	 */
	static bool ArchiveSaveForNewGame(FString& OutFolder, FString& OutError);
	const FString& GetRecoveryNotice() const { return RecoveryNotice; }
	/** 디버그: 저장 파일에서 다시 불러오기 (재접속과 같은 흐름) */
	void DebugReloadFromSave();
	/** 디버그: 저장 파일의 저장 시각을 Seconds만큼 과거로 (현재 상태는 그대로 · 방치 검증용) */
	void DebugShiftSaveTime(double Seconds);

	// --- 방치 보상 (2주차 기능 2 · D44) ---

	/** 아직 보여 주지 않은 방치 보상 결과 (없으면 nullptr) */
	const FCozyOfflineReport* GetPendingOfflineReport() const { return bOfflineReportPending ? &OfflineReport : nullptr; }
	void DismissOfflineReport() { bOfflineReportPending = false; }
	/** 디버그: 방치 시간 건너뛰기 (저장 파일 없이 같은 정산 처리 · 영지를 시작했으면 정산 후 저장) */
	void DebugSkipOffline(double Seconds);
	/**
	 *  실제 시각 기준 진행 (Tick이 지난 처리 이후 흐른 UTC 초로 부름 · 자동 시험도 사용).
	 *  0 이하(시계를 뒤로 돌림)는 무시 · CatchupGapSeconds 이상 비었으면(절전·멈춤 복귀) 방치 정산과 같은 경로(최대 12시간)
	 *  · 그 밖에는 1초씩 진행 · 진행한 게임 초 수를 돌려줌 (방치 정산 경로면 0)
	 */
	int32 AdvanceRealSeconds(double RealSeconds, const TCHAR* Source);
	/** 디버그: 가공 '제작 추가' 규칙 자체 검사 (실제 규칙 함수로 시험 · 끝나면 검사 전 상태로 되돌림 · 결과는 로그 [자체 검사]) */
	FString DebugRunProcessingAppendCheck();

	/** 상태가 바뀔 때마다 알림 (UI 새로 고침용) */
	FOnCozyEstateChanged OnEstateChanged;

	/** 영지 일 알림 (수확·가공 완료·업그레이드 완료·판매·배치 확정·구매·주민 배치 · 방치 정산 중에는 보내지 않음) */
	FOnCozyEstateEvent OnCozyEstateEvent;
	/** 지금 알림을 보내지 않는 중인가 (방치 정산 등) */
	bool IsEventSuppressed() const { return EventSuppressDepth > 0; }

	// --- UTickableWorldSubsystem ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:

	bool LoadAllData(FString& OutErrors);
	/** 저장 파일을 읽어 State에 넣는다 · 없거나 읽을 수 없으면 false (상태는 그대로) */
	bool LoadEstateFromSave(FDateTime& OutSavedUtc);
	/** 저장 파일에서 불러오고, 못 불러오면 새 게임 상태를 만든 뒤 시설 액터를 놓는다 (시작 · 다시 불러오기 공통) · 불러왔으면 true */
	bool LoadOrBuildState(FDateTime& OutSavedUtc);
	/** 불러온 상태에서 데이터에 없는 시설·주민·작업·재료 참조를 정리 (정리 6-6 · 지운 개수를 로그) */
	void SanitizeLoadedState();
	/** 칸 좌표 · 크기 → 월드 위치 (실제 지면 높이) */
	FVector ComputeFacilityLocation(FIntPoint Coord, FIntPoint Size, const FGuid* Surface = nullptr) const;
	ACozyFacilityActor* FindFacilityActor(const FGuid& FacilityId) const;
	/** 시설 액터를 목록에서 빼고 없앤다 (보관 · 지형 복구) */
	void DestroyFacilityActor(const FGuid& FacilityId);
	/** 칸이 영지 안인가 */
	bool IsCellInGrid(FIntPoint Cell) const;
	/** 칸의 최소 모서리 월드 위치 (높이는 영지 원점 높이) */
	FVector GetCellMin(FIntPoint Cell) const;
	/** 바닥 타일 액터가 없으면 만들고 지금 타일 목록으로 다시 그린다 */
	void RebuildFloorTileActor();
	bool FindFreeSpot(const FGuid& FacilityId, int32 Rotation, FIntPoint& OutCoord) const;

	/** 꺼 둔 시간만큼 접속 중과 같은 규칙으로 1초씩 진행 (최대 12시간) + 방치 재화 · 정산 직후 반드시 저장 */
	void ApplyOfflineProgress(double AwaySeconds, const TCHAR* Source);
	UDataTable* LoadCsvTable(const FString& FileName, UScriptStruct* RowStruct, FString& OutErrors);
	void BuildNewGameState();
	void SpawnFacilityActors();
	void SpawnFacilityActor(const FCozyFacilityState& Facility);
	void DestroyFacilityActors();

	/** 게임 시간 1초마다 작업 진행 (게임 시간: 1초 단위 갱신) */
	void StepJobs();
	/** 게임 시간 1초: 시간 +1 · 작업 진행 · 방치 재화 누적 (켜 둔 동안·방치 정산 공통 · D44) */
	void AdvanceOneSecond(int32& OutTalismans, int32& OutShards);
	/** 방치 재화 누적 시간에 Seconds를 더하고 다 찬 만큼 지급 (남는 시간은 상태에 남아 다음에 이어짐) */
	void AccrueIdleRewards(double Seconds, int32& OutTalismans, int32& OutShards);
	void StepProduction(FCozyFacilityState& Facility, const FCozyFacilityRow& Def);
	double GetProductionDuration(const FCozyFacilityState& Facility, const FCozyCropRow& Crop, const FCozyFacilityRow& Def) const;
	/** 지금 속도로 생산 주기 작업을 새로 등록하고 시설의 진행 작업으로 정한다 (StartAt = 주기 시작 시각) */
	const FCozyJobRecord& AddProductionJob(FCozyFacilityState& Facility, const FCozyCropRow& Crop, const FCozyFacilityRow& Def, double StartAt);
	bool IsFacilityWorking(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const;

	/** 🙋 필요 인원이 부족해지면 진행 중인 생산 주기를 취소하고 진행도를 초기화 (이미 지급한 생산물은 유지 · D26) */
	void CancelProductionIfUnderstaffed(FCozyFacilityState& Facility);

	/** 🙋 가공은 필요 인원이 부족해지면 현재 회차의 진행도를 유지한 채 일시 정지 · 다시 채워지면 이어서 (D30) */
	void UpdateProcessingPause(FCozyFacilityState& Facility);
	/** 가공 칸이 비면 대기 중인 추가 제작을 순서대로 시작 (StartAt = 시작 시각 · 앞 작업이 끝난 시각) */
	void PromoteQueuedProcessing(const FGuid& FacilityId, double StartAt);
	void StepProcessing(FCozyFacilityState& Facility, const FCozyFacilityRow& Def);
	double GetProcessingDuration(const FCozyFacilityState& Facility, const FCozyRecipeRow& Recipe, const FCozyFacilityRow& Def) const;

	/** 진행·일시 정지 중인 가공 작업이 확보한 미수령 공간 */
	int32 GetReservedUnclaimed(const FCozyFacilityState& Facility) const;
	/** 지금 이 시설이 맡은 품목 (미수령분 우선, 없으면 진행 작업의 완료품) */
	FName GetLockedOutputItem(const FCozyFacilityState& Facility) const;
	/** 미수령분에 더한다 (품목이 비어 있으면 이 품목으로 정함) */
	void AddUnclaimed(FCozyFacilityState& Facility, FName ItemId, int32 Amount);
	FText GetItemName(FName ItemId) const;
	/** 작물·시설 정의 이름 (정의가 없으면 ID 그대로) */
	FText GetCropName(FName CropId) const;
	FText GetFacilityDefName(FName DefinitionId) const;

	/** 성장 설정표의 행 (ID로) */
	const FCozyGrowthRow* GetGrowthDef(FName RowId) const;
	/** 성장 설정표에서 이 시설 정의·레벨의 다음 단계 행 */
	const FCozyGrowthRow* FindGrowthRow(FName DefinitionId, int32 FromLevel, FName* OutRowId = nullptr) const;
	/** 생산·가공 속도 배율 · 관리 시설이 정해진 시설은 관리 시설 단계의 효과 (D39) */
	double GetSpeedMultiplier(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const;
	/** 성장(레벨·관리 시설)만의 속도 배율 · 꾸미기 보너스는 GetSpeedMultiplier가 곱함 */
	double GetGrowthSpeedMultiplier(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const;
	/**
	 *  이 정의의 시설 중 가장 높은 레벨 (없으면 0).
	 *  레벨 효과(관리 속도·선행 조건)는 영지에 놓인 시설만 셈 · bIncludeStored는 해금(진행 기록) 판단용
	 */
	int32 GetHighestLevelOf(FName DefinitionId, bool bIncludeStored = false) const;
	/** 시간이 다 된 업그레이드를 완료 처리 */
	void StepGrowth();
	void RemoveResidentFromFacility(FCozyResidentState& Resident);

	FCozyFacilityState* FindFacilityMutable(const FGuid& Id);
	FCozyResidentState* FindResidentMutable(const FGuid& Id);
	FCozyJobRecord* FindJobMutable(const FGuid& Id);

	void NotifyChanged(bool bStructural = true);
	/** 영지 일 알림 보내기 (방치 정산 중이면 보내지 않음) */
	void BroadcastEstateEvent(ECozyEstateEventKind Kind, const FGuid& FacilityId, FName ItemId, int32 Amount, int32 Level = 0);
	/** 0보다 크면 알림을 보내지 않음 (방치 정산 · 자체 검사) */
	int32 EventSuppressDepth = 0;
	/** 모아 둔 알림 (작업 진행 중에는 모았다가 끝난 뒤 한 번에 보냄 · 받는 쪽이 진행 중인 목록을 건드리지 않게) */
	TArray<FCozyEstateEvent> PendingEvents;
	void FlushEstateEvents();
	/** 동시 가공 칸 수 (상태·정의로 · GetProcessingSlotCount 공통) */
	int32 GetProcessingSlotCount(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> FacilityTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CropTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ItemTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ResidentTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> RecipeTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> GrowthTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StartFacilityTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StartResidentTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StartResourceTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> RewardTable;

	/** 첫 실행 안내 단계 (Tutorial.csv · 없어도 시작은 됨) */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> TutorialTable;

	/** 꾸미기 장식 (Decorations.csv · 행마다 시설 정의도 함께 등록됨) */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DecorationTable;

	/** 바닥 꾸미기 (Floors.csv · 없으면 바닥 칠하기가 무료) */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> FloorTable;
	/** 장식 행을 기능 없는 시설 정의로 시설 표에 더한다 (같은 ID가 이미 있으면 경고 후 건너뜀) */
	void RegisterDecorationFacilities(FString& OutErrors);

	FCozyEstateConfigRow Config;

	UPROPERTY(Transient)
	FCozyEstateState State;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACozyFacilityActor>> FacilityActors;

	bool bStarted = false;
	bool bNewGameSession = false;
	float TimeScale = 1.f;
	double StepAccumulator = 0.0;
	ECozyNightOverride NightOverride = ECozyNightOverride::Auto;
	/** 디버그 배속으로 앞당긴 게임 시계 (현실 초 · 저장하지 않음) */
	double DayClockBoostSeconds = 0.0;
	bool bShowTestRecipes = false;
	/** 이번 Tick에 업그레이드가 끝나 구조가 바뀌었는가 (UI가 해금·레벨 표시를 갱신) */
	bool bStructuralPending = false;
	/** 마지막 저장 시각 (UTC) · 자동 저장까지 흐른 실제 시간 */
	FDateTime LastSavedUtc;
	double AutosaveAccumulator = 0.0;
	/** 마지막으로 시간을 처리한 실제 시각 (UTC) · 켜 둔 동안 진행은 DeltaTime 누적이 아니라 이 시각과의 차이로 (절전·멈춤에도 시간이 사라지지 않게) */
	FDateTime LastProcessedUtc;
	/** 묶어서 저장할 요청이 있는가 · 첫 요청 뒤 흐른 실제 시간 */
	bool bSaveRequested = false;
	double SaveRequestAge = 0.0;
	FString SaveRequestReason;
	bool bAutosavePaused = false;
	bool bSaveBlockedByInvalidFile = false;
	FCozyEstateSaveIO SaveWriter;
	FString RecoveryNotice;
	void PollSave(bool bWait = false);
	void RestoreGroundReferences();
	bool bGroundRestorePending = true;
	FCozyOfflineReport OfflineReport;
	bool bOfflineReportPending = false;

	// 배치 미리보기 (확정 전에는 상태를 바꾸지 않음)
	FGuid PlacementId;
	FGuid PlacementSurfaceId;
	FIntPoint PlacementCoord = FIntPoint::ZeroValue;
	int32 PlacementRotation = 0;
	bool bPlacementFromStorage = false;
	UPROPERTY(Transient)
	TObjectPtr<ACozyFacilityActor> PlacementActor;
	UPROPERTY(Transient) TObjectPtr<class ACozyFloorTiles> FloorTileActor;
};
