#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CozyRealmDataTypes.h"
#include "Estate/CozyEstateState.h"
#include "CozyEstateSubsystem.generated.h"

class UDataTable;
class ACozyFacilityActor;

/** bStructural: 배치·재료처럼 구조가 바뀌면 true, 시간만 흐르면 false */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyEstateChanged, bool /*bStructural*/);

/** 밤 판정 디버그 덮어쓰기 */
UENUM(BlueprintType)
enum class ECozyNightOverride : uint8
{
	/** PC 현지 시각으로 판정 */
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

/** 수령 결과 (UI가 메시지로 보여 줌) */
struct FCozyCollectResult
{
	bool bSuccess = false;
	int32 Moved = 0;
	int32 Remaining = 0;
	FText Message;
};

/**
 *  영지 공통 서비스 (정리 6-2): 게임 데이터 · 영지 상태 · 재료·재화 · 작업 기록과 게임 시간.
 *  UI와 시설 액터는 이 서비스의 상태를 읽고 요청만 보낸다 (6-7).
 */
UCLASS()
class UCozyEstateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	// --- 시작 ---

	/** CSV 데이터를 읽고 새 게임 상태를 만든 뒤 시설 액터를 놓는다 (게임 모드가 호출) */
	void StartEstate();

	/** 데이터를 다시 검사해 로그에 경고를 남긴다 (디버그 메뉴) · 문제 개수를 돌려줌 */
	int32 ValidateData() const;

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
	/** 지금 Amount만큼 보관할 수 있는가 (재료는 창고 한도 · 재화는 항상 가능) */
	bool CanStore(FName ItemId, int32 Amount) const;
	/** 보관 가능한 만큼만 더하고 실제로 더한 개수를 돌려줌 */
	int32 AddResource(FName ItemId, int32 Amount);

	/** 공용 창고에 이 재료를 더 받을 수 있는 수량 (재화는 한도 없음 → MAX_int32) */
	int32 GetStorageSpace(FName ItemId) const;

	/** 창고 화면에 보여 줄 재료·재화 ID (재료 먼저, 표 순서) */
	TArray<FName> GetStorageItems() const;

	// --- 미수령 생산물 ---

	/** 시설의 미수령 생산물을 공용 창고로 옮긴다 · 창고에 들어갈 만큼만 옮기고 나머지는 시설에 남김 */
	FCozyCollectResult CollectUnclaimed(const FGuid& FacilityId);

	/** 시설의 미수령 생산물 수량 */
	int32 GetUnclaimedTotal(const FGuid& FacilityId) const;

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
	ECozyNightOverride GetNightOverride() const { return NightOverride; }
	void SetNightOverride(ECozyNightOverride NewOverride);

	// --- 디버그 ---

	void DebugAddResource(FName ItemId, int32 Amount);
	/** 재료 보유량을 정해진 값으로 맞춘다 (수령 검증용) */
	void DebugSetResource(FName ItemId, int32 Amount);
	/** 테스트용으로 같은 종류 시설을 빈 칸에 하나 더 놓는다 (추가 건설 규칙과 무관한 디버그 기능) */
	bool DebugAddFacility(FName DefinitionId);
	/** 상태를 지우고 시작 설정으로 새 게임을 다시 만든다 */
	void DebugRestartNewGame();

	/** 상태가 바뀔 때마다 알림 (UI 새로 고침용) */
	FOnCozyEstateChanged OnEstateChanged;

	// --- UTickableWorldSubsystem ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	bool LoadAllData(FString& OutErrors);
	UDataTable* LoadCsvTable(const FString& FileName, UScriptStruct* RowStruct, FString& OutErrors);
	void BuildNewGameState();
	void SpawnFacilityActors();
	void SpawnFacilityActor(const FCozyFacilityState& Facility);
	void DestroyFacilityActors();

	/** 게임 시간 1초마다 작업 진행 (게임 시간: 1초 단위 갱신) */
	void StepJobs();
	void StepProduction(FCozyFacilityState& Facility, const FCozyFacilityRow& Def);
	double GetProductionDuration(const FCozyFacilityState& Facility, const FCozyCropRow& Crop, const FCozyFacilityRow& Def) const;
	bool IsFacilityWorking(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const;

	/** 🙋 필요 인원이 부족해지면 진행 중인 생산 주기를 취소하고 진행도를 초기화 (이미 지급한 생산물은 유지 · D26) */
	void CancelProductionIfUnderstaffed(FCozyFacilityState& Facility);

	/** 🙋 가공은 필요 인원이 부족해지면 현재 회차의 진행도를 유지한 채 일시 정지 · 다시 채워지면 이어서 (D30) */
	void UpdateProcessingPause(FCozyFacilityState& Facility);
	void StepProcessing(FCozyFacilityState& Facility, const FCozyFacilityRow& Def);
	double GetProcessingDuration(const FCozyFacilityState& Facility, const FCozyRecipeRow& Recipe, const FCozyFacilityRow& Def) const;

	/** 진행·일시 정지 중인 가공 작업이 확보한 미수령 공간 */
	int32 GetReservedUnclaimed(const FCozyFacilityState& Facility) const;
	/** 지금 이 시설이 맡은 품목 (미수령분 우선, 없으면 진행 작업의 완료품) */
	FName GetLockedOutputItem(const FCozyFacilityState& Facility) const;
	/** 미수령분에 더한다 (품목이 비어 있으면 이 품목으로 정함) */
	void AddUnclaimed(FCozyFacilityState& Facility, FName ItemId, int32 Amount);
	FText GetItemName(FName ItemId) const;
	void RemoveResidentFromFacility(FCozyResidentState& Resident);

	FCozyFacilityState* FindFacilityMutable(const FGuid& Id);
	FCozyResidentState* FindResidentMutable(const FGuid& Id);
	FCozyJobRecord* FindJobMutable(const FGuid& Id);

	void NotifyChanged(bool bStructural = true);

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

	FCozyEstateConfigRow Config;

	UPROPERTY(Transient)
	FCozyEstateState State;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACozyFacilityActor>> FacilityActors;

	bool bStarted = false;
	float TimeScale = 1.f;
	double StepAccumulator = 0.0;
	ECozyNightOverride NightOverride = ECozyNightOverride::Auto;
	bool bShowTestRecipes = false;
};
