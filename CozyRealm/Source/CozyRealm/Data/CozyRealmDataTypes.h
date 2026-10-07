#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CozyRealmDataTypes.generated.h"

/**
 *  게임 데이터 표(CSV)의 행 구조.
 *  숫자는 코드가 아니라 Data/GameData/*.csv에만 둔다 (기획서 결정 카드).
 *  각 행의 Note 칸에 🙋 확정 / 🤖 테스트 값 / ❓ 미정을 적어 구분한다.
 */

/** 시설이 가진 기능 (시설 정의 표에서 조합) */
UENUM(BlueprintType)
enum class ECozyFacilityFunction : uint8
{
	/** 주기마다 재료를 만든다 (밭) */
	Production,
	/** 레시피로 재료를 바꾼다 (제분소) */
	Processing,
	/** 재료를 골드로 판다 (판매소) */
	Sales,
	/** 주민 목록·배치를 관리한다 (나가야) */
	ResidentHousing,
	/** 업그레이드 공통 슬롯을 제공한다 (후신소) */
	UpgradeQueue,
	/** 영지 레벨 · 다른 시설의 레벨 상한 (신사) */
	ShrineCore,
	/** 관리 대상 생산 시설(밭) 전체의 공통 성장·작물 해금 (밭 관리 시설 · D37·D39) */
	FieldManagement
};

/** 재료·재화 구분 */
UENUM(BlueprintType)
enum class ECozyItemCategory : uint8
{
	/** 창고 한도를 받는 재료 */
	Material,
	/** 한도 없는 재화 (골드 · 시간 부적 · 심상 조각) */
	Currency
};

/** 시설 정의 (시설 종류마다 1행) */
USTRUCT(BlueprintType)
struct FCozyFacilityRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 보이는 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FText DisplayName;

	/** 차지하는 칸 수 (가로 X · 세로 Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FIntPoint Size = FIntPoint(2, 2);

	/** 회색 상자 높이 (cm) · 에셋이 생기기 전 임시 모습용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	float GreyboxHeight = 150.f;

	/** 회색 상자 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FLinearColor GreyboxColor = FLinearColor(0.6f, 0.6f, 0.6f);

	/** 이 시설이 가진 기능 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	TArray<ECozyFacilityFunction> Functions;

	/** 주민이 있어야 작동하는가 (D4: 생산·가공은 켬) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Residents")
	bool bRequiresResidents = false;

	/** 작동에 필요한 최소 주민 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Residents")
	int32 MinResidents = 0;

	/** 배치할 수 있는 최대 주민 수 (0이면 배치 칸 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Residents")
	int32 MaxResidents = 0;

	/** 생산 기능이 만들 수 있는 작물(생산 항목) ID 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	TArray<FName> ProductionItems;

	/** 플레이어가 생산 항목을 고를 수 있는가 (공통 밭은 켬, 차밭처럼 하나뿐이면 끔) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	bool bProductionItemSelectable = false;

	/** 레벨당 생산 속도 증가율 (0.2 = 레벨마다 +20%) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	float SpeedBonusPerLevel = 0.f;

	/** 이 시설에 쌓아 둘 수 있는 미수령 완료품 최대 개수 (생산·가공 공통) · 한 번에 한 종류만 · 공용 창고 한도와 별개 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	int32 UnclaimedCapacity = 0;

	/** 동시에 진행할 수 있는 가공 작업 수 (가공 시설만 · 0이면 가공 불가) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	int32 ProcessingSlots = 0;

	/** 이 시설의 성장 효과를 주는 관리 시설 정의 ID (밭 → 밭 관리 시설 · D39) · 비어 있으면 자기 레벨의 효과를 받음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FName ManagerFacilityId;

	/** 동시에 진행할 수 있는 업그레이드 수 (후신소 · 0이면 업그레이드 대기열 아님 · D9·D25) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	int32 UpgradeSlots = 0;

	/** 미수령 생산물을 창고로 옮기는 버튼 이름 (예: 수확 · 수령) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	FText CollectButtonLabel;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 작물·생산 항목 (작물마다 1행 · D16) */
USTRUCT(BlueprintType)
struct FCozyCropRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 보이는 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FText DisplayName;

	/** 한 주기마다 얻는 재료 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FName ProducedItem;

	/** 한 주기마다 얻는 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	int32 ProducedAmount = 1;

	/** Lv1 기준 한 주기 시간 (초) · 레벨이 오르면 짧아짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	float ProductionSeconds = 10.f;

	/** 재배 외형 ID (작물 모습 · D18) · 비어 있으면 회색 상자만 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FName CultivationLookId;

	/** 처음부터 고를 수 있는 작물인가 · 아니면 성장 설정표의 '해금 작물'로 열림 (D37) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	bool bStartUnlocked = false;

	/** 🙋/🤖/❓ 표시와 메모 · 해금 조건은 ❓라 아직 칸 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 재료·재화 (1행 = 1종) */
USTRUCT(BlueprintType)
struct FCozyItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 보이는 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** 재료인지 재화인지 (재료만 창고 한도를 받음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ECozyItemCategory Category = ECozyItemCategory::Material;

	/** 판매소에서 1개당 받는 골드 · 0이면 팔 수 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 SellPrice = 0;

	/** 화면 위쪽 HUD에 표시하는 순서 · 0이면 표시 안 함 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 HudOrder = 0;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 주민 종류 (1행 = 1종) */
USTRUCT(BlueprintType)
struct FCozyResidentRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 보이는 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	FText DisplayName;

	/** 등급 (1~4성) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	int32 Grade = 1;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 가공 레시피 (1행 = 1회 실행 · 공통 가공 기능이 모든 제작 시설에서 사용 · D32) */
USTRUCT(BlueprintType)
struct FCozyRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 레시피를 쓰는 시설 정의 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName FacilityId;

	/** 넣는 재료와 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TMap<FName, int32> Inputs;

	/** 나오는 재료 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName OutputItem;

	/** 1회 실행마다 나오는 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	int32 OutputAmount = 1;

	/** 1회 실행에 걸리는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	float Seconds = 15.f;

	/** 검증용 레시피 · 디버그 메뉴에서 '테스트 레시피 보이기'를 켰을 때만 가공 창에 나옴 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	bool bTestOnly = false;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 성장 설정표 (기능 3에서 사용 · 지금은 데이터 구조만 · 정리 7장) */
USTRUCT(BlueprintType)
struct FCozyGrowthRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 성장 대상 시설 정의 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FName TargetFacilityId;

	/** 몇 레벨에서 시작하는 단계인가 (1이면 Lv1→2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	int32 FromLevel = 1;

	/** 필요한 신사 레벨 (상한 규칙) · 0이면 조건 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	int32 RequiredShrineLevel = 0;

	/** 시작 비용 (비어 있으면 비용 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	TMap<FName, int32> StartCost;

	/** 기본 시간 (초) · 0이면 즉시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	float Seconds = 0.f;

	/** 이 단계가 끝나면 해금되는 작물 ID (Crops.csv 행 이름 · D37) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	TArray<FName> UnlockCrops;

	/** 선행 시설 조건: 시설 정의 ID → 필요한 최소 레벨 (같은 시설이 여러 개면 가장 높은 레벨 기준 · D41) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	TMap<FName, int32> RequiredFacilities;

	/** 이 단계가 끝나면 해금되는 시설 정의 ID (건설 기능에서 사용 · 시설별 해금 레벨 ❓ D15) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	TArray<FName> UnlockFacilities;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 시작 설정 · 시설 (새 게임에 놓이는 시설 1개 = 1행) */
USTRUCT(BlueprintType)
struct FCozyStartFacilityRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 시설 정의 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FName FacilityId;

	/** 놓이는 칸 (왼쪽 아래 모서리 기준) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FIntPoint GridCoord = FIntPoint::ZeroValue;

	/** 90도 회전 횟수 (0~3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	int32 Rotation = 0;

	/** 시작 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	int32 Level = 1;

	/** 생산 시설이면 시작 작물 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FName SelectedCrop;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 시작 설정 · 주민 (새 게임에 주는 주민 1명 = 1행) */
USTRUCT(BlueprintType)
struct FCozyStartResidentRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 주민 종류 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FName ResidentId;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 시작 설정 · 재료·재화 (1행 = 1종) */
USTRUCT(BlueprintType)
struct FCozyStartResourceRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 재료·재화 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FName ItemId;

	/** 시작 보유량 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	int32 Amount = 0;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 일회성 보상 (같은 RewardId 의 행들이 한 보상 · 한 번만 지급 · 정리 7-11) */
USTRUCT(BlueprintType)
struct FCozyRewardRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 보상 ID (예: Tutorial_FirstSpeedup) · 지급 기록은 이 ID로 저장 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName RewardId;

	/** 지급할 재료·재화 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName ItemId;

	/** 지급량 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int32 Amount = 0;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};

/** 영지 공통 설정 (Default 1행) */
USTRUCT(BlueprintType)
struct FCozyEstateConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 영지 가로·세로 칸 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FIntPoint GridSize = FIntPoint(12, 12);

	/** 한 칸 크기 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
	float CellSize = 200.f;

	/** 칸 (0,0)의 왼쪽 아래 모서리 월드 위치 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FVector GridOrigin = FVector(-1200.f, -1200.f, 10.f);

	/** 재료 한 종류당 창고 한도 · 가득 차면 그 재료 생산이 멈춤 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
	int32 StorageCapPerItem = 100;

	/** 밤이 시작되는 PC 현지 시각 (시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time")
	int32 NightStartHour = 18;

	/** 밤이 끝나는 PC 현지 시각 (시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time")
	int32 NightEndHour = 6;

	/** 판매 대금으로 받는 재화 ID (Items.csv 행 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sales")
	FName SaleCurrencyId = TEXT("Gold");

	/** 시간 단축 재화 ID (Items.csv 행 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedup")
	FName SpeedupItemId = TEXT("TimeTalisman");

	/** 부적 1장이 줄이는 시간 (초) · 시설 레벨과 무관하게 고정 (D44 · 🤖 T1 테스트 값 60초 · 정식 밸런스 ❓) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedup")
	float SpeedupSecondsPerItem = 60.f;

	/** 자동 저장 간격 (실제 초 · 🙋 1분마다 + 종료 시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save")
	float AutosaveSeconds = 60.f;

	/** 🙋/🤖/❓ 표시와 메모 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FString Note;
};
