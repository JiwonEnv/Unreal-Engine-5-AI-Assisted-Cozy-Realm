#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "CozyHeroSettings.generated.h"

/**
 *  주인공 값 (4주차 기능 1 · 크기 기준 시험).
 *  키 · 비율 · 속도를 여기서 바꾸면 플레이 중에도 바로 반영된다. 길이 단위는 cm, 거리·속도는 칸(1칸 = 영지 칸 크기 · 지금 200cm).
 */
USTRUCT(BlueprintType)
struct FCozyHeroConfig
{
	GENERATED_BODY()

	/** 주인공 키 (cm) · 크기 기준 카드에서 시험 중 (후보 90 · 120 · 160) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Height (키 cm)", ClampMin="30.0", ClampMax="400.0"))
	float Height = 120.f;

	/** 머리 지름 ÷ 키 (SD 비율 · 0.4면 머리가 키의 40%) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Head Ratio (머리 비율)", ClampMin="0.15", ClampMax="0.6"))
	float HeadRatio = 0.4f;

	/** 몸통 굵기 ÷ 머리 지름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Body Width Ratio (몸통 굵기 · 머리 대비)", ClampMin="0.3", ClampMax="1.5"))
	float BodyWidthRatio = 0.75f;

	/** 비교용 인형 키 목록 (cm) · 주인공 시작 위치 옆에 서서 머리 위에 키 숫자를 보여 줌 · 비우면 인형 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Compare Heights (비교용 인형 키 cm)"))
	TArray<float> CompareHeights = { 90.f, 160.f };

	/** 비교용 오니비 구슬 (크기 기준 시험 · X = 지름 cm · Y = 땅에서 구슬 가운데까지 높이 cm) · 주인공 앞줄 빈 칸에 하나씩 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Compare Onibi (비교용 오니비 · X 지름 · Y 떠 있는 높이 cm)"))
	TArray<FVector2D> CompareOnibi = { FVector2D(40.f, 100.f), FVector2D(50.f, 100.f), FVector2D(60.f, 120.f) };

	/** 비교용 문 벽 (크기 기준 시험 · X = 문 높이 · Y = 문 너비 · Z = 처마(벽) 높이 cm) · 주인공 뒷줄에 한 칸씩 띄워 세움 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Compare Doors (비교용 문 · X 문 높이 · Y 문 너비 · Z 처마 cm)"))
	TArray<FVector> CompareDoors = { FVector(170.f, 90.f, 220.f), FVector(190.f, 100.f, 250.f), FVector(210.f, 110.f, 270.f) };

	/** 몸통 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Body Color (몸통 색)"))
	FLinearColor BodyColor = FLinearColor(0.78f, 0.35f, 0.26f);

	/** 머리 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 크기", meta=(DisplayName="Head Color (머리 색)"))
	FLinearColor HeadColor = FLinearColor(0.95f, 0.86f, 0.74f);

	/** 걷는 속도 (칸/초) · 🤖 기획서 초안은 3칸(= 초당 6m) · 크기 기준 카드에서 시험 후 확정 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 이동", meta=(DisplayName="Walk Speed (걷기 칸/초)", ClampMin="0.2", ClampMax="10.0"))
	float WalkSpeedCells = 1.5f;

	/** Shift를 누른 채 달리는 속도 (칸/초) · 0이면 달리기 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 이동", meta=(DisplayName="Run Speed (Shift 달리기 칸/초 · 0이면 없음)", ClampMin="0.0", ClampMax="10.0"))
	float RunSpeedCells = 3.f;

	/** 몸을 돌리는 속도 (도/초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 이동", meta=(DisplayName="Turn Speed (도는 속도 도/초)", ClampMin="60.0"))
	float TurnSpeed = 720.f;

	/** E로 시설 메뉴를 여는 거리 (칸 · 시설이 차지한 칸 테두리까지) · 🤖 기획서 초안 1.5칸 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 상호작용·산책", meta=(DisplayName="Interact Range (E 상호작용 거리 칸)", ClampMin="0.1"))
	float InteractRangeCells = 1.5f;

	/** 이 시간(초) 동안 조작이 없으면 스스로 산책 · 0이면 산책 안 함 · 🤖 기획서 초안 10초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 상호작용·산책", meta=(DisplayName="Idle Before Wander (산책 시작까지 초)", ClampMin="0.0"))
	float IdleWanderSeconds = 10.f;

	/** 산책: 한 번에 가는 최대 거리 (칸) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 상호작용·산책", meta=(DisplayName="Wander Radius (산책 한 번 거리 칸)", ClampMin="1.0"))
	float WanderRadiusCells = 4.f;

	/** 산책: 도착한 뒤 쉬는 시간 (초 · 최소~최대 사이에서 골라 씀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 상호작용·산책", meta=(DisplayName="Wander Pause (산책 쉬는 시간 초)"))
	FVector2D WanderPauseSeconds = FVector2D(2.f, 4.f);

	/** 산책 속도 ÷ 걷기 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 상호작용·산책", meta=(DisplayName="Wander Speed Ratio (산책 속도 · 걷기 대비)", ClampMin="0.1", ClampMax="1.0"))
	float WanderSpeedRatio = 0.6f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyHeroSettingsChanged, const class UCozyHeroSettings*);

/**
 *  주인공 설정 에셋 (DA_HeroSettings).
 *  에디터에서 값을 바꾸면(플레이 중에도) 주인공 · 비교용 인형에 바로 반영된다 (카메라 설정 에셋과 같은 방식).
 */
UCLASS(BlueprintType, meta=(DisplayName="Cozy Hero Settings (주인공 설정)"))
class UCozyHeroSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ShowOnlyInnerProperties))
	FCozyHeroConfig Config;

	FOnCozyHeroSettingsChanged OnSettingsChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** 프로젝트 설정 'Cozy Hero' · 쓸 주인공 설정 에셋을 고른다 (코드에 에셋 경로를 두지 않음) */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Cozy Hero (주인공 설정)"))
class UCozyHeroProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 주인공 설정 에셋 · 비우면 주인공을 만들지 않음 (예전처럼 카메라만) */
	UPROPERTY(Config, EditAnywhere, Category="Hero", meta=(DisplayName="Hero Settings (주인공 설정 에셋)"))
	TSoftObjectPtr<UCozyHeroSettings> HeroSettings;

	static const UCozyHeroProjectSettings* Get() { return GetDefault<UCozyHeroProjectSettings>(); }
};
