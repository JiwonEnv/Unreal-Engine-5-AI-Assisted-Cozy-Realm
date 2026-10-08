#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CozyRealmEstatePlayerController.generated.h"

class UCozyHudWidget;
class ACozyFacilityActor;

/**
 *  Player controller for the estate.
 *  Mouse-driven: the cursor stays visible and click/hover events are enabled for selecting facilities.
 */
UCLASS()
class ACozyRealmEstatePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ACozyRealmEstatePlayerController();

	/** 지금 선택된 시설 (없으면 nullptr) */
	ACozyFacilityActor* GetSelectedFacility() const { return SelectedFacility.Get(); }

	/** 화면 UI (편집 가능한 화면이 기존 기능을 부를 때 씀) */
	UCozyHudWidget* GetHud() const { return Hud; }

	/** 이 정의의 시설(같은 시설이 여러 개면 레벨이 가장 높은 것)을 선택 · 업그레이드 조건의 '이동' 버튼 */
	void SelectFacilityByDefinition(FName DefinitionId);

	/** 배치 모드 (B) · 시설을 끌어서 옮기고 회전·보관·확정·취소 (D45) */
	void SetPlacementMode(bool bEnable);
	bool IsPlacementMode() const { return bPlacementMode; }

	/** 시설 선택 해제 (선택 표시 · 기능 아이콘 숨김) · 상세 창을 열 때 HUD도 부름 */
	void ClearSelection();

protected:

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** 영지 화면 UI 클래스 (재화 표시 · 기능 아이콘 · 전용 창 · 디버그 메뉴) */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCozyHudWidget> HudClass;

private:

	/** 왼쪽 클릭: 시설이면 선택하고 기능 아이콘을, 빈 곳이면 선택 해제 */
	void HandleWorldClick();
	void SelectFacility(ACozyFacilityActor* Facility);

	UPROPERTY(Transient)
	TObjectPtr<UCozyHudWidget> Hud;

	TWeakObjectPtr<ACozyFacilityActor> SelectedFacility;

	/** 배치 모드 입력 (누르고 끌기 · R 회전) */
	void TickPlacement();
	bool bPlacementMode = false;
	bool bDraggingPlacement = false;
};
