#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Hero/CozyHeroSettings.h"
#include "CozyHeroCharacter.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** 회색 도형 SD 인형 모양 (몸통 원기둥 + 머리 구 + 앞쪽 표시) · 주인공과 비교용 인형이 같이 씀 */
USTRUCT()
struct FCozyHeroBody
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Nose;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeadMaterial;

	/** 액터 생성자에서 부품을 만듦 */
	void Create(AActor* Owner, USceneComponent* Parent);
	/** 키 · 비율 · 색 적용 · BaseZ = 부모 기준 발바닥 높이 */
	void Apply(const FCozyHeroConfig& Config, float Height, float BaseZ);
};

/**
 *  주인공 (4주차 기능 1 · 회색 도형 · 5주차 주인공 3D 에셋 전까지).
 *  WASD(카메라 방향 기준) · 오른쪽 클릭 땅 이동(칸 격자 길찾기로 시설을 돌아감) · E 상호작용 · 조작이 없으면 산책.
 *  플레이어 컨트롤러는 카메라를 조종하고, 이 액터는 컨트롤러 없이 스스로 움직인다.
 */
UCLASS()
class ACozyHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	ACozyHeroCharacter();

	void SetSettings(UCozyHeroSettings* InSettings);

	/** 키보드 이동 방향 (월드 · 길이 0~1) · 매 프레임 컨트롤러가 넣음 · 0이면 멈춤 */
	void SetKeyboardInput(const FVector& WorldDirection, bool bRun);
	/** 오른쪽 클릭 이동: 칸 격자로 길을 찾아 감 · 갈 수 없으면 false */
	bool MoveToLocation(const FVector& Target);
	/** 플레이어가 조작함 → 산책 대기 시간 처음부터 */
	void NotifyPlayerInput();
	/** 배치 모드 등에서 멈춰 세움 (산책도 안 함) */
	void SetFrozen(bool bFreeze);

	/** E: 상호작용 거리 안의 가장 가까운 시설 (없으면 빈 ID) */
	FGuid FindInteractFacility() const;

	/** 보이는 몸을 캡슐보다 내린 높이 (경사로에서 떠 보이지 않게 · 충돌·이동은 그대로) */
	float GetVisualDrop() const { return VisualDrop; }
	/** 캡슐 아래 끝 (발바닥 높이) */
	FVector GetFeetLocation() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:

	void ApplyConfig();
	void HandleSettingsChanged(const UCozyHeroSettings* Changed);
	void SpawnCompareDolls();
	void DestroyCompareDolls();

	/** 지금 발 위치에서 Goal 칸 가운데까지 표면 길찾기 · 결과는 월드 경유점 */
	bool FindPath(FIntPoint Goal, TArray<FVector>& OutPoints) const;
	/** 발밑이 시설에 덮여 있으면 가장 가까운 빈 곳으로 순간 이동 · 옮겼으면 true */
	bool TeleportOutOfFacility();
	void StartWander();
	void FollowPath();
	void KeepInsideEstate();

	UPROPERTY(Transient)
	TObjectPtr<UCozyHeroSettings> Settings;

	UPROPERTY(VisibleAnywhere)
	FCozyHeroBody Shape;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CompareDolls;

	FCozyHeroConfig Config;
	FDelegateHandle SettingsChangedHandle;

	FVector KeyboardDirection = FVector::ZeroVector;
	bool bKeyboardRun = false;
	TArray<FVector> PathPoints;
	bool bWandering = false;
	bool bFrozen = false;
	double LastInputTime = 0.0;
	double WanderResumeTime = 0.0;
	float VisualDrop = 0.f;
	/** 캡슐 아래와 실제 지면 사이(평지 약 2cm · 30도 경사 약 6cm)만큼 보이는 몸만 내린다 */
	void UpdateVisualDrop(float DeltaSeconds);
};

/** 비교용 인형 (크기 기준 시험 · 움직이지 않음 · 머리 위에 키 숫자) */
UCLASS(NotPlaceable)
class ACozyScaleDoll : public AActor
{
	GENERATED_BODY()

public:

	ACozyScaleDoll();
	void Setup(const FCozyHeroConfig& Config, float Height);

private:

	UPROPERTY(VisibleAnywhere)
	FCozyHeroBody Shape;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
};
