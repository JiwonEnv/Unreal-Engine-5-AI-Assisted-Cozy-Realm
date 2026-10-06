#pragma once

#include "CoreMinimal.h"
#include "CozyEstateState.generated.h"

/**
 *  영지의 저장 가능한 상태 (정리 6-4 · 6-6 · D13 · D23).
 *  서로를 에디터 객체가 아니라 ID로 참조해서, 2주차에 이 구조를 그대로 파일에 연결한다.
 */

/** 작업 상태 (UI는 이 값과 이유만 읽는다 · 6-7) */
UENUM(BlueprintType)
enum class ECozyJobState : uint8
{
	Running,
	Completed,
	Held,
	Cancelled
};

/** 작업 종류 */
UENUM(BlueprintType)
enum class ECozyJobType : uint8
{
	Production,
	Processing,
	Growth
};

/** 놓인 시설 1개의 상태 */
USTRUCT(BlueprintType)
struct FCozyFacilityState
{
	GENERATED_BODY()

	/** 시설 고유 ID (같은 종류가 여러 개여도 각자 다름) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	FGuid InstanceId;

	/** 시설 정의 ID (Facilities.csv 행 이름) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	FName DefinitionId;

	/** 놓인 칸 (왼쪽 아래 모서리) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	FIntPoint GridCoord = FIntPoint::ZeroValue;

	/** 90도 회전 횟수 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	int32 Rotation = 0;

	/** 시설 레벨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	int32 Level = 1;

	/** 시설 외형 단계 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	int32 AppearanceStage = 1;

	/** 밭이면 선택한 작물 ID (D23 · 불러와도 유지) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	FName SelectedCropId;

	/** 배치된 주민 고유 ID */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	TArray<FGuid> AssignedResidents;

	/** 이 시설에 쌓인 미수령 생산물 (재료 ID → 개수) · 수령하기 전에는 가공·판매·업그레이드에 쓸 수 없음 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	TMap<FName, int32> UnclaimedItems;

	/** 진행 중인 작업 ID (작업 기록이 유일한 원본 · 없으면 무효 ID) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	FGuid ActiveJobId;
};

/** 주민 1명의 상태 */
USTRUCT(BlueprintType)
struct FCozyResidentState
{
	GENERATED_BODY()

	/** 주민 고유 ID */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	FGuid InstanceId;

	/** 주민 종류 ID (Residents.csv 행 이름) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	FName DefinitionId;

	/** 배치된 시설 고유 ID · 무효면 나가야에 미배치 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	FGuid AssignedFacility;
};

/** 작업 기록 1개 (진행 중 작업의 유일한 원본 · 6-4) */
USTRUCT(BlueprintType)
struct FCozyJobRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	FGuid JobId;

	/** 작업을 하는 시설 고유 ID */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	FGuid FacilityId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	ECozyJobType Type = ECozyJobType::Production;

	/** 작업 내용 (생산이면 작물 ID) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	FName ContentId;

	/** 시작한 게임 시각 (초) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	double StartGameSeconds = 0.0;

	/** 한 번(한 주기)에 필요한 시간 (초) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	double DurationSeconds = 0.0;

	/** 보류로 멈춰 있던 시간 누적 (초) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	double PausedSeconds = 0.0;

	/** 보류를 시작한 게임 시각 (보류 중일 때만 의미 있음) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	double HeldSinceGameSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	ECozyJobState State = ECozyJobState::Running;

	/** 보류 이유 (UI 표시용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	FText HeldReason;

	/** 반복 생산에서 지금까지 미수령분으로 넘긴 주기 수 (같은 주기를 두 번 넘기지 않게 · 6-5) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Job")
	int32 PaidCycles = 0;
};

/** 영지 전체 상태 (한 번에 같은 시점으로 저장할 묶음) */
USTRUCT(BlueprintType)
struct FCozyEstateState
{
	GENERATED_BODY()

	/** 저장 형식 버전 (6-6) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	int32 SaveVersion = 1;

	/** 누적 게임 시간 (초 · 디버그 배속 반영) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	double GameSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	TArray<FCozyFacilityState> Facilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	TArray<FCozyResidentState> Residents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	TArray<FCozyJobRecord> Jobs;

	/** 재료·재화 보유량 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	TMap<FName, int32> Resources;

	/** 지급한 일회성 보상 ID (7-11 · 2주차) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estate")
	TArray<FName> GrantedRewards;
};
